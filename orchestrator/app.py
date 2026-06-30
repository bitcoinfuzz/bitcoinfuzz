"""FastAPI application for the bitcoinfuzz orchestrator.

Provides the complete REST API surface described in proposal §6.1,
including campaign lifecycle management, log streaming via SSE,
real-time metrics via WebSocket, and historical query endpoints.
"""

import asyncio
import json
import logging
import time
from contextlib import asynccontextmanager
from pathlib import Path
from typing import AsyncGenerator, Optional

from fastapi import FastAPI, HTTPException, Query, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import StreamingResponse
from sse_starlette.sse import EventSourceResponse

from .campaign_manager import CampaignManager
from .config import load_default_config
from .crash_watcher import CrashWatcher
from .database import Database
from .docker_manager import DockerManager
from .models import (
    CampaignCreate,
    CampaignResponse,
    CampaignState,
    CampaignSummary,
    CrashRecord,
    DashboardSummary,
    HealthResponse,
)
from .scheduler import ResourceScheduler
from .scheduled_campaigns import CampaignScheduler, load_schedules
from .targets import build_target_registry, load_targets

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# App state (populated during lifespan)
# ---------------------------------------------------------------------------

_start_time: float = 0.0
_manager: Optional[CampaignManager] = None
_db: Optional[Database] = None
_campaign_scheduler: Optional[CampaignScheduler] = None
_config = load_default_config()
_target_registry: dict = {}


@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncGenerator:
    """Application lifespan: initialise all subsystems on startup,
    tear them down on shutdown."""
    global _start_time, _manager, _db, _campaign_scheduler, _target_registry

    _start_time = time.time()
    config = _config

    # --- Database ---
    db = Database(config.db_path)
    await asyncio.to_thread(db.connect)
    _db = db

    # --- Target registry ---
    _target_registry = build_target_registry(config.compose_file)
    logger.info("Loaded %d targets from %s", len(_target_registry), config.compose_file)

    # --- Docker manager ---
    try:
        docker_mgr = DockerManager(data_dir=str(config.data_dir))
    except Exception as exc:
        logger.warning(
            "Docker not available — running in API-only mode: %s", exc
        )
        docker_mgr = None  # type: ignore[assignment]

    # --- Resource scheduler ---
    scheduler = ResourceScheduler()

    # --- Crash watcher ---
    crash_watcher = CrashWatcher(
        data_dir=config.data_dir,
        poll_interval=config.crash_poll_interval,
    )

    # --- Campaign manager ---
    if docker_mgr is not None:
        manager = CampaignManager(
            db=db,
            docker_mgr=docker_mgr,
            scheduler=scheduler,
            crash_watcher=crash_watcher,
            target_registry=_target_registry,
            data_dir=config.data_dir,
            health_poll_interval=config.health_poll_interval,
            max_retries=config.max_retries,
        )
        # Wire up crash alert callback
        crash_watcher._alert_callback = manager.handle_crash_alert
        await manager.start()
        _manager = manager
    else:
        _manager = None

    # --- Scheduled campaigns ---
    if config.schedule_file and config.schedule_file.exists() and _manager:
        entries = load_schedules(config.schedule_file)
        cs = CampaignScheduler()
        cs.configure(entries, _manager.launch_campaign)
        cs.start()
        _campaign_scheduler = cs

    logger.info("Orchestrator ready on %s:%d", config.host, config.port)
    yield

    # --- Shutdown ---
    if _campaign_scheduler:
        _campaign_scheduler.stop()
    if _manager:
        await _manager.stop()
    if _db:
        await asyncio.to_thread(_db.close)
    logger.info("Orchestrator shut down")


# ---------------------------------------------------------------------------
# App factory
# ---------------------------------------------------------------------------

def build_app() -> FastAPI:
    """Create the FastAPI application with all routes."""
    app = FastAPI(
        title="bitcoinfuzz orchestrator",
        description="Campaign orchestration and monitoring for bitcoinfuzz",
        version="0.1.0",
        lifespan=lifespan,
    )

    # CORS — allow dashboard to connect
    app.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )

    # -----------------------------------------------------------------------
    # Health
    # -----------------------------------------------------------------------

    @app.get("/health", response_model=HealthResponse, tags=["system"])
    def health() -> HealthResponse:
        active = _manager.active_campaign_count if _manager else 0
        return HealthResponse(
            status="ok",
            version="0.1.0",
            active_campaigns=active,
            uptime_seconds=round(time.time() - _start_time, 1),
        )

    # -----------------------------------------------------------------------
    # Targets
    # -----------------------------------------------------------------------

    @app.get("/targets", tags=["targets"])
    def list_targets() -> list[dict]:
        """List all registered fuzz targets from docker-compose.yml."""
        return [t.to_dict() for t in _target_registry.values()]

    @app.get("/targets/{name}", tags=["targets"])
    def get_target(name: str) -> dict:
        """Get a single target by name."""
        target = _target_registry.get(name)
        if target is None:
            raise HTTPException(404, f"Target not found: {name}")
        return target.to_dict()

    @app.get("/targets/{name}/history", tags=["targets"])
    async def target_history(
        name: str,
        limit: int = Query(10, ge=1, le=100),
    ) -> list[dict]:
        """Historical campaign summaries for a target."""
        if name not in _target_registry:
            raise HTTPException(404, f"Target not found: {name}")
        return await asyncio.to_thread(_db.get_target_history, name, limit)

    # -----------------------------------------------------------------------
    # Campaigns
    # -----------------------------------------------------------------------

    @app.post("/campaigns", status_code=201, tags=["campaigns"])
    async def create_campaign(request: CampaignCreate) -> dict:
        """Launch a new fuzzing campaign (or queue it if resources are full)."""
        if _manager is None:
            raise HTTPException(503, "Docker not available")
        try:
            campaign = await _manager.launch_campaign(request)
            return campaign.to_dict()
        except ValueError as exc:
            raise HTTPException(400, str(exc))

    @app.get("/campaigns", tags=["campaigns"])
    async def list_campaigns(
        state: Optional[str] = None,
        target_name: Optional[str] = None,
    ) -> list[dict]:
        """List all campaigns with optional state/target filter."""
        state_enum = CampaignState(state) if state else None
        if _manager:
            campaigns = await _manager.list_campaigns(
                state=state_enum, target_name=target_name
            )
        elif _db:
            campaigns = await asyncio.to_thread(
                _db.list_campaigns, state=state_enum, target_name=target_name
            )
        else:
            campaigns = []
        return [c.to_dict() for c in campaigns]

    @app.get("/campaigns/{campaign_id}", tags=["campaigns"])
    async def get_campaign(campaign_id: str) -> dict:
        """Get details for a single campaign."""
        campaign = None
        if _manager:
            campaign = await _manager.get_campaign(campaign_id)
        elif _db:
            campaign = await asyncio.to_thread(_db.get_campaign, campaign_id)
        if campaign is None:
            raise HTTPException(404, f"Campaign not found: {campaign_id}")
        return campaign.to_dict()

    @app.delete("/campaigns/{campaign_id}", tags=["campaigns"])
    async def delete_campaign(campaign_id: str) -> dict:
        """Stop and remove a campaign."""
        if _manager is None:
            raise HTTPException(503, "Docker not available")
        try:
            campaign = await _manager.stop_campaign(campaign_id)
            return campaign.to_dict()
        except KeyError:
            raise HTTPException(404, f"Campaign not found: {campaign_id}")
        except ValueError as exc:
            raise HTTPException(409, str(exc))

    @app.post("/campaigns/{campaign_id}/pause", tags=["campaigns"])
    async def pause_campaign(campaign_id: str) -> dict:
        """Pause a running campaign (SIGSTOP)."""
        if _manager is None:
            raise HTTPException(503, "Docker not available")
        try:
            campaign = await _manager.pause_campaign(campaign_id)
            return campaign.to_dict()
        except KeyError:
            raise HTTPException(404, f"Campaign not found: {campaign_id}")
        except ValueError as exc:
            raise HTTPException(409, str(exc))

    @app.post("/campaigns/{campaign_id}/resume", tags=["campaigns"])
    async def resume_campaign(campaign_id: str) -> dict:
        """Resume a paused campaign (SIGCONT)."""
        if _manager is None:
            raise HTTPException(503, "Docker not available")
        try:
            campaign = await _manager.resume_campaign(campaign_id)
            return campaign.to_dict()
        except KeyError:
            raise HTTPException(404, f"Campaign not found: {campaign_id}")
        except ValueError as exc:
            raise HTTPException(409, str(exc))

    @app.get("/campaigns/{campaign_id}/logs", tags=["campaigns"])
    async def campaign_logs(
        campaign_id: str,
        follow: bool = Query(False),
        tail: int = Query(100, ge=1, le=10000),
    ):
        """Stream container logs.  Use follow=true for SSE streaming."""
        if _manager is None:
            raise HTTPException(503, "Docker not available")

        try:
            if follow:
                # SSE streaming
                async def event_generator():
                    campaign = await _manager.get_campaign(campaign_id)
                    if not campaign or not campaign.container_id:
                        return
                    for line in _manager._docker.get_container_logs(
                        campaign.container_id, follow=True, tail=tail
                    ):
                        yield {"event": "log", "data": line}

                return EventSourceResponse(event_generator())
            else:
                lines = await _manager.get_campaign_logs(campaign_id, tail=tail)
                return {"logs": lines}
        except KeyError:
            raise HTTPException(404, f"Campaign not found: {campaign_id}")

    @app.get("/campaigns/{campaign_id}/crashes", tags=["campaigns"])
    async def campaign_crashes(campaign_id: str) -> list[dict]:
        """List crash artefacts for a campaign."""
        if _manager:
            return await _manager.get_campaign_crashes(campaign_id)
        elif _db:
            return await asyncio.to_thread(
                _db.get_crashes, campaign_id=campaign_id
            )
        return []

    # -----------------------------------------------------------------------
    # Dashboard
    # -----------------------------------------------------------------------

    @app.get("/dashboard/summary", response_model=DashboardSummary, tags=["dashboard"])
    async def dashboard_summary() -> DashboardSummary:
        """Aggregate stats for the dashboard overview."""
        stats = await asyncio.to_thread(_db.get_dashboard_stats) if _db else {}
        resources = _manager.resource_summary if _manager else {}
        return DashboardSummary(
            active_campaigns=stats.get("active_campaigns", 0),
            queued_campaigns=stats.get("queued_campaigns", 0),
            completed_campaigns=stats.get("completed_campaigns", 0),
            failed_campaigns=stats.get("failed_campaigns", 0),
            total_crashes=stats.get("total_crashes", 0),
            total_targets=len(_target_registry),
            cpu_allocated=resources.get("cpu_allocated", 0),
            cpu_total=resources.get("cpu_total", 0),
            memory_allocated_mb=resources.get("memory_allocated_mb", 0),
            memory_total_mb=resources.get("memory_total_mb", 0),
        )

    @app.get("/dashboard/resources", tags=["dashboard"])
    def resource_utilization() -> dict:
        """Current resource allocation vs. host capacity."""
        if _manager:
            return _manager.resource_summary
        return {}

    # -----------------------------------------------------------------------
    # Historical comparison
    # -----------------------------------------------------------------------

    @app.get("/targets/{name}/compare", tags=["targets"])
    async def compare_campaigns(
        name: str,
        campaign_ids: str = Query(..., description="Comma-separated campaign IDs"),
    ) -> list[dict]:
        """Compare historical stats across multiple campaigns for a target."""
        ids = [cid.strip() for cid in campaign_ids.split(",") if cid.strip()]
        if not ids:
            raise HTTPException(400, "No campaign IDs provided")
        return await asyncio.to_thread(_db.compare_campaigns, ids)

    # -----------------------------------------------------------------------
    # Alerting webhook (receives from Alertmanager)
    # -----------------------------------------------------------------------

    @app.post("/api/alerts/webhook", tags=["alerts"])
    async def alerts_webhook(payload: dict) -> dict:
        """Receive alerts from Alertmanager and log them."""
        alerts = payload.get("alerts", [])
        for alert in alerts:
            logger.warning(
                "Alert received: status=%s name=%s summary=%s",
                alert.get("status"),
                alert.get("labels", {}).get("alertname"),
                alert.get("annotations", {}).get("summary"),
            )
        return {"status": "received", "count": len(alerts)}

    # -----------------------------------------------------------------------
    # CI/CD webhook (receives from GitHub Actions)
    # -----------------------------------------------------------------------

    @app.post("/api/ci-hook", tags=["ci"])
    async def ci_hook(payload: dict) -> dict:
        """Trigger campaigns from CI (e.g. on main branch push)."""
        commit = payload.get("commit", "unknown")
        trigger = payload.get("trigger", "unknown")
        logger.info("CI hook received: commit=%s trigger=%s", commit, trigger)
        # TODO: optionally launch a full campaign round
        return {"status": "acknowledged", "commit": commit, "trigger": trigger}

    # -----------------------------------------------------------------------
    # WebSocket for real-time metrics
    # -----------------------------------------------------------------------

    @app.websocket("/ws/campaigns/{campaign_id}/metrics")
    async def ws_campaign_metrics(
        websocket: WebSocket, campaign_id: str
    ) -> None:
        """WebSocket endpoint for real-time metric streaming."""
        await websocket.accept()
        try:
            while True:
                campaign = None
                if _manager:
                    campaign = await _manager.get_campaign(campaign_id)
                if campaign is None:
                    await websocket.send_json({"error": "Campaign not found"})
                    break

                data = campaign.to_dict()
                # Add resource info
                if _manager:
                    data["resources"] = _manager.resource_summary
                await websocket.send_json(data)
                await asyncio.sleep(5)  # Push every 5 seconds
        except WebSocketDisconnect:
            pass
        except Exception as exc:
            logger.debug("WebSocket error for %s: %s", campaign_id, exc)

    # -----------------------------------------------------------------------
    # Scheduled campaigns info
    # -----------------------------------------------------------------------

    @app.get("/schedules", tags=["schedules"])
    def list_schedules() -> list[dict]:
        """List all configured campaign schedules."""
        if _campaign_scheduler:
            return _campaign_scheduler.get_scheduled_jobs()
        return []

    return app


# Module-level app instance for uvicorn
app = build_app()

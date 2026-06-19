"""Campaign lifecycle manager — ties all orchestrator components together.

Owns the state machine, Docker integration, resource scheduler, crash
watcher, and database persistence.  Provides the high-level API that
the FastAPI routes call.
"""

import asyncio
import logging
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

from .crash_watcher import CrashWatcher
from .database import Database
from .docker_manager import DockerManager
from .models import (
    Campaign,
    CampaignCreate,
    CampaignState,
    ResourceLimits,
    TERMINAL_STATES,
)
from .scheduler import ResourceScheduler
from .targets import FuzzTarget

logger = logging.getLogger(__name__)


class CampaignManager:
    """Central campaign lifecycle controller.

    Coordinates between all sub-systems:
    - ``DockerManager`` for container operations
    - ``ResourceScheduler`` for resource accounting and queueing
    - ``CrashWatcher`` for new-crash detection
    - ``Database`` for persistence

    Background tasks:
    - Health monitor: polls container status every N seconds
    - Crash watcher: polls crash directories
    """

    def __init__(
        self,
        db: Database,
        docker_mgr: DockerManager,
        scheduler: ResourceScheduler,
        crash_watcher: CrashWatcher,
        target_registry: dict[str, FuzzTarget],
        data_dir: Path,
        health_poll_interval: int = 5,
        max_retries: int = 3,
    ) -> None:
        self._db = db
        self._docker = docker_mgr
        self._scheduler = scheduler
        self._crash_watcher = crash_watcher
        self._targets = target_registry
        self._data_dir = data_dir
        self._health_interval = health_poll_interval
        self._max_retries = max_retries

        # In-memory campaign cache for fast access
        self._campaigns: dict[str, Campaign] = {}

        # Background tasks
        self._health_task: Optional[asyncio.Task] = None

    # -- startup / shutdown --------------------------------------------------

    async def start(self) -> None:
        """Initialise the manager: reload state from DB, start background tasks."""
        # Reload active campaigns from database
        for state in [
            CampaignState.RUNNING,
            CampaignState.STARTING,
            CampaignState.PAUSED,
            CampaignState.QUEUED,
            CampaignState.RESTARTING,
            CampaignState.CRASHED,
        ]:
            campaigns = await asyncio.to_thread(self._db.list_campaigns, state=state)
            for c in campaigns:
                self._campaigns[c.id] = c
                if c.state in {CampaignState.RUNNING, CampaignState.PAUSED}:
                    self._scheduler.allocate(c.id, c.resource_limits)
                    self._crash_watcher.register(c.id, c.target_name)

        logger.info(
            "Campaign manager started: %d active campaigns loaded",
            len(self._campaigns),
        )

        # Start background tasks
        self._crash_watcher.start()
        self._health_task = asyncio.create_task(self._health_monitor_loop())

    async def stop(self) -> None:
        """Shut down background tasks gracefully."""
        self._crash_watcher.stop()
        if self._health_task and not self._health_task.done():
            self._health_task.cancel()
            try:
                await self._health_task
            except asyncio.CancelledError:
                pass
        logger.info("Campaign manager stopped")

    # -- campaign operations -------------------------------------------------

    async def launch_campaign(self, request: CampaignCreate) -> Campaign:
        """Create and launch (or queue) a new campaign.

        Returns the Campaign object.
        Raises ValueError if the target name is not in the registry.
        """
        target = self._targets.get(request.target_name)
        if target is None:
            raise ValueError(f"Unknown target: {request.target_name}")

        limits = ResourceLimits(
            cpu_quota=request.cpu_quota,
            memory_mb=request.memory_mb,
        )

        campaign = Campaign(
            target_name=request.target_name,
            resource_limits=limits,
            priority=request.priority,
            max_retries=request.max_retries,
            modules=request.modules or target.modules,
            env_overrides={**target.env_overrides, **request.env_overrides},
            image_tag=f"bitcoinfuzz:{request.target_name}",
        )

        # Persist to DB
        await asyncio.to_thread(self._db.create_campaign, campaign)
        self._campaigns[campaign.id] = campaign

        # Try to launch immediately or queue
        if self._scheduler.can_launch(limits):
            await self._start_campaign(campaign)
        else:
            self._scheduler.enqueue(campaign.id, campaign.priority)
            logger.info(
                "Campaign %s queued (insufficient resources)", campaign.id
            )

        return campaign

    async def stop_campaign(self, campaign_id: str) -> Campaign:
        """Stop a running campaign and archive it.

        Raises KeyError if not found, ValueError if already terminal.
        """
        campaign = self._get_campaign_or_raise(campaign_id)

        if campaign.state in TERMINAL_STATES:
            raise ValueError(
                f"Campaign {campaign_id} is already in terminal state: {campaign.state.value}"
            )

        # If queued, just remove from queue
        if campaign.state == CampaignState.QUEUED:
            self._scheduler.remove_from_queue(campaign_id)
            campaign.transition_to(CampaignState.COMPLETED)
            campaign.exit_reason = "stopped"
        else:
            # Stop the container
            if campaign.container_id:
                await asyncio.to_thread(
                    self._docker.stop_container, campaign.container_id
                )
            campaign.transition_to(CampaignState.COMPLETED)
            campaign.exit_reason = "stopped"

        await self._finalise_campaign(campaign)
        return campaign

    async def pause_campaign(self, campaign_id: str) -> Campaign:
        """Pause a running campaign (SIGSTOP the container)."""
        campaign = self._get_campaign_or_raise(campaign_id)

        if campaign.state != CampaignState.RUNNING:
            raise ValueError(
                f"Can only pause RUNNING campaigns, got: {campaign.state.value}"
            )

        if campaign.container_id:
            await asyncio.to_thread(
                self._docker.pause_container, campaign.container_id
            )

        campaign.transition_to(CampaignState.PAUSED)
        await asyncio.to_thread(self._db.update_campaign, campaign)
        return campaign

    async def resume_campaign(self, campaign_id: str) -> Campaign:
        """Resume a paused campaign (SIGCONT the container)."""
        campaign = self._get_campaign_or_raise(campaign_id)

        if campaign.state != CampaignState.PAUSED:
            raise ValueError(
                f"Can only resume PAUSED campaigns, got: {campaign.state.value}"
            )

        if campaign.container_id:
            await asyncio.to_thread(
                self._docker.resume_container, campaign.container_id
            )

        campaign.transition_to(CampaignState.RUNNING)
        await asyncio.to_thread(self._db.update_campaign, campaign)
        return campaign

    async def get_campaign(self, campaign_id: str) -> Optional[Campaign]:
        """Retrieve a campaign by ID."""
        return self._campaigns.get(campaign_id) or await asyncio.to_thread(
            self._db.get_campaign, campaign_id
        )

    async def list_campaigns(
        self,
        state: Optional[CampaignState] = None,
        target_name: Optional[str] = None,
    ) -> list[Campaign]:
        """List campaigns with optional filters."""
        return await asyncio.to_thread(
            self._db.list_campaigns, state=state, target_name=target_name
        )

    async def get_campaign_logs(
        self, campaign_id: str, tail: int = 100
    ) -> list[str]:
        """Return recent log lines from a campaign's container."""
        campaign = self._get_campaign_or_raise(campaign_id)
        if not campaign.container_id:
            return []
        lines = list(
            await asyncio.to_thread(
                lambda: list(
                    self._docker.get_container_logs(
                        campaign.container_id, follow=False, tail=tail
                    )
                )
            )
        )
        return lines

    async def get_campaign_crashes(self, campaign_id: str) -> list[dict]:
        """Return crash records for a campaign."""
        return await asyncio.to_thread(
            self._db.get_crashes, campaign_id=campaign_id
        )

    # -- internal lifecycle --------------------------------------------------

    async def _start_campaign(self, campaign: Campaign) -> None:
        """Move a campaign from QUEUED to RUNNING via Docker launch."""
        try:
            campaign.transition_to(CampaignState.STARTING)
            await asyncio.to_thread(self._db.update_campaign, campaign)

            target = self._targets.get(campaign.target_name)
            cxxflags = target.cxxflags if target else ""

            container_id = await asyncio.to_thread(
                self._docker.launch_container,
                campaign,
                cxxflags,
                campaign.target_name,
            )
            campaign.container_id = container_id
            campaign.transition_to(CampaignState.RUNNING)

            self._scheduler.allocate(campaign.id, campaign.resource_limits)
            self._crash_watcher.register(campaign.id, campaign.target_name)

            await asyncio.to_thread(self._db.update_campaign, campaign)
            logger.info(
                "Campaign %s RUNNING (container=%s)",
                campaign.id,
                container_id[:12],
            )
        except Exception as exc:
            logger.error(
                "Failed to start campaign %s: %s", campaign.id, exc
            )
            campaign.state = CampaignState.FAILED
            campaign.exit_reason = f"start_failed: {exc}"
            campaign.ended_at = datetime.now(timezone.utc)
            await asyncio.to_thread(self._db.update_campaign, campaign)

    async def _restart_campaign(self, campaign: Campaign) -> None:
        """Attempt to restart a crashed campaign."""
        if campaign.retry_count >= campaign.max_retries:
            logger.warning(
                "Campaign %s exceeded max retries (%d), marking FAILED",
                campaign.id,
                campaign.max_retries,
            )
            campaign.transition_to(CampaignState.FAILED)
            campaign.exit_reason = "max_retries_exceeded"
            await self._finalise_campaign(campaign)
            return

        campaign.retry_count += 1
        campaign.transition_to(CampaignState.RESTARTING)
        await asyncio.to_thread(self._db.update_campaign, campaign)

        # Clean up old container
        if campaign.container_id:
            await asyncio.to_thread(
                self._docker.remove_container, campaign.container_id
            )
            campaign.container_id = None

        logger.info(
            "Restarting campaign %s (attempt %d/%d)",
            campaign.id,
            campaign.retry_count,
            campaign.max_retries,
        )

        campaign.transition_to(CampaignState.STARTING)
        await self._start_campaign_container(campaign)

    async def _start_campaign_container(self, campaign: Campaign) -> None:
        """Launch the container (used by both start and restart paths)."""
        try:
            target = self._targets.get(campaign.target_name)
            cxxflags = target.cxxflags if target else ""

            container_id = await asyncio.to_thread(
                self._docker.launch_container,
                campaign,
                cxxflags,
                campaign.target_name,
            )
            campaign.container_id = container_id
            campaign.transition_to(CampaignState.RUNNING)
            self._crash_watcher.register(campaign.id, campaign.target_name)
            await asyncio.to_thread(self._db.update_campaign, campaign)
            logger.info(
                "Campaign %s restarted (container=%s)",
                campaign.id,
                container_id[:12],
            )
        except Exception as exc:
            logger.error("Restart failed for campaign %s: %s", campaign.id, exc)
            campaign.state = CampaignState.FAILED
            campaign.exit_reason = f"restart_failed: {exc}"
            campaign.ended_at = datetime.now(timezone.utc)
            await self._finalise_campaign(campaign)

    async def _finalise_campaign(self, campaign: Campaign) -> None:
        """Archive a terminal campaign and release its resources."""
        self._scheduler.deallocate(campaign.id)
        self._crash_watcher.unregister(campaign.id)

        target = self._targets.get(campaign.target_name)
        cxxflags = target.cxxflags if target else ""

        await asyncio.to_thread(
            self._db.archive_campaign, campaign, cxxflags
        )
        await asyncio.to_thread(self._db.update_campaign, campaign)

        # Try to launch queued campaigns now that resources are free
        await self._process_queue()

        logger.info(
            "Campaign %s finalised (state=%s, reason=%s)",
            campaign.id,
            campaign.state.value,
            campaign.exit_reason,
        )

    async def _process_queue(self) -> None:
        """Try to launch queued campaigns when resources become available."""
        eligible = self._scheduler.dequeue_eligible()
        for cid in eligible:
            campaign = self._campaigns.get(cid)
            if campaign is None:
                continue
            if self._scheduler.can_launch(campaign.resource_limits):
                await self._start_campaign(campaign)
            else:
                # Re-enqueue if still not enough resources
                self._scheduler.enqueue(cid, campaign.priority)

    # -- health monitoring ---------------------------------------------------

    async def _health_monitor_loop(self) -> None:
        """Background loop that polls container health for all active campaigns."""
        while True:
            try:
                await self._check_all_health()
            except asyncio.CancelledError:
                raise
            except Exception:
                logger.exception("Error in health monitor loop")
            await asyncio.sleep(self._health_interval)

    async def _check_all_health(self) -> None:
        """Check container status for all running campaigns."""
        for campaign_id, campaign in list(self._campaigns.items()):
            if campaign.state not in {CampaignState.RUNNING, CampaignState.PAUSED}:
                continue
            if not campaign.container_id:
                continue

            status = await asyncio.to_thread(
                self._docker.get_container_status, campaign.container_id
            )
            if status is None:
                # Container disappeared
                logger.warning(
                    "Container for campaign %s disappeared", campaign_id
                )
                await self._handle_crash(campaign, "container_disappeared")
                continue

            if not status["running"] and not status.get("paused", False):
                exit_code = status.get("exit_code", -1)
                oom = status.get("oom_killed", False)
                reason = "oom_killed" if oom else f"exited_code_{exit_code}"

                if exit_code == 0:
                    # Normal completion
                    campaign.transition_to(CampaignState.COMPLETED)
                    campaign.exit_reason = "completed"
                    await self._finalise_campaign(campaign)
                else:
                    await self._handle_crash(campaign, reason)

    async def _handle_crash(self, campaign: Campaign, reason: str) -> None:
        """Handle a crashed campaign — attempt restart or mark failed."""
        logger.warning(
            "Campaign %s crashed: %s (retry %d/%d)",
            campaign.id,
            reason,
            campaign.retry_count,
            campaign.max_retries,
        )
        campaign.transition_to(CampaignState.CRASHED)
        campaign.exit_reason = reason
        await asyncio.to_thread(self._db.update_campaign, campaign)
        await self._restart_campaign(campaign)

    # -- crash alert callback ------------------------------------------------

    async def handle_crash_alert(
        self, campaign_id: str, target_name: str, crash_path: Path
    ) -> None:
        """Called by the CrashWatcher when a new crash file is found."""
        file_size = crash_path.stat().st_size if crash_path.exists() else None
        await asyncio.to_thread(
            self._db.record_crash,
            campaign_id,
            target_name,
            str(crash_path),
            file_size,
        )

        campaign = self._campaigns.get(campaign_id)
        if campaign:
            campaign.crash_count += 1
            await asyncio.to_thread(self._db.update_campaign, campaign)

        logger.warning(
            "Crash recorded: campaign=%s target=%s file=%s size=%s",
            campaign_id,
            target_name,
            crash_path.name,
            file_size,
        )

    # -- helpers -------------------------------------------------------------

    def _get_campaign_or_raise(self, campaign_id: str) -> Campaign:
        """Return a campaign from cache or raise KeyError."""
        campaign = self._campaigns.get(campaign_id)
        if campaign is None:
            raise KeyError(f"Campaign not found: {campaign_id}")
        return campaign

    @property
    def active_campaign_count(self) -> int:
        return sum(1 for c in self._campaigns.values() if c.is_active)

    @property
    def resource_summary(self) -> dict:
        return self._scheduler.get_resource_summary()

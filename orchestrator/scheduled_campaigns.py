"""Cron-style campaign scheduling.

Reads schedule definitions from a YAML config file and uses APScheduler
to trigger campaigns at the specified intervals.
"""

import logging
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Optional

import yaml
from apscheduler.schedulers.asyncio import AsyncIOScheduler
from apscheduler.triggers.cron import CronTrigger

from .models import CampaignCreate

logger = logging.getLogger(__name__)


@dataclass
class ScheduleEntry:
    """A single scheduled campaign definition."""

    target: str
    cron: str  # Standard 5-field cron expression
    max_duration_seconds: Optional[int] = None
    modules: Optional[str] = None
    priority: int = 1
    cpu_quota: int = 200_000
    memory_mb: int = 2048
    max_retries: int = 3
    enabled: bool = True


def load_schedules(config_path: Path) -> list[ScheduleEntry]:
    """Load schedule entries from a YAML config file.

    Expected format::

        schedules:
          - target: script
            cron: "0 */6 * * *"
            max_duration_seconds: 21600
            modules: "BITCOIN_CORE,RUST_BITCOIN"
            priority: 2
          - target: deserialize_block
            cron: "0 0 * * *"
            enabled: false
    """
    if not config_path.exists():
        logger.info("No schedule config found at %s", config_path)
        return []

    with config_path.open("r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}

    entries = []
    for item in data.get("schedules", []):
        try:
            entry = ScheduleEntry(
                target=item["target"],
                cron=item["cron"],
                max_duration_seconds=item.get("max_duration_seconds"),
                modules=item.get("modules"),
                priority=item.get("priority", 1),
                cpu_quota=item.get("cpu_quota", 200_000),
                memory_mb=item.get("memory_mb", 2048),
                max_retries=item.get("max_retries", 3),
                enabled=item.get("enabled", True),
            )
            entries.append(entry)
        except (KeyError, TypeError) as exc:
            logger.warning("Skipping invalid schedule entry: %s (%s)", item, exc)

    logger.info("Loaded %d schedule entries from %s", len(entries), config_path)
    return entries


class CampaignScheduler:
    """Manages cron-based campaign scheduling using APScheduler.

    Each enabled schedule entry is registered as an APScheduler job.
    When triggered, it calls the provided ``launch_callback`` with a
    ``CampaignCreate`` request.
    """

    def __init__(self) -> None:
        self._scheduler = AsyncIOScheduler()
        self._entries: list[ScheduleEntry] = []

    def configure(
        self,
        entries: list[ScheduleEntry],
        launch_callback: Any,  # async callable(CampaignCreate) -> Campaign
    ) -> None:
        """Register schedule entries as APScheduler jobs.

        Args:
            entries: Schedule definitions loaded from config.
            launch_callback: Async function to call when a schedule triggers.
        """
        self._entries = entries
        for i, entry in enumerate(entries):
            if not entry.enabled:
                logger.debug("Skipping disabled schedule: %s", entry.target)
                continue

            job_id = f"schedule_{entry.target}_{i}"

            async def _trigger(e: ScheduleEntry = entry) -> None:
                logger.info(
                    "Scheduled campaign triggered: target=%s cron=%s",
                    e.target,
                    e.cron,
                )
                request = CampaignCreate(
                    target_name=e.target,
                    modules=e.modules,
                    cpu_quota=e.cpu_quota,
                    memory_mb=e.memory_mb,
                    priority=e.priority,
                    max_retries=e.max_retries,
                    max_duration_seconds=e.max_duration_seconds,
                )
                try:
                    await launch_callback(request)
                except Exception:
                    logger.exception(
                        "Failed to launch scheduled campaign for %s", e.target
                    )

            try:
                trigger = CronTrigger.from_crontab(entry.cron)
                self._scheduler.add_job(
                    _trigger,
                    trigger=trigger,
                    id=job_id,
                    replace_existing=True,
                    name=f"Scheduled: {entry.target}",
                )
                logger.info(
                    "Registered schedule: %s -> %s", entry.target, entry.cron
                )
            except ValueError as exc:
                logger.error(
                    "Invalid cron expression for %s: %s (%s)",
                    entry.target,
                    entry.cron,
                    exc,
                )

    def start(self) -> None:
        """Start the scheduler."""
        if not self._scheduler.running:
            self._scheduler.start()
            logger.info("Campaign scheduler started")

    def stop(self) -> None:
        """Stop the scheduler."""
        if self._scheduler.running:
            self._scheduler.shutdown(wait=False)
            logger.info("Campaign scheduler stopped")

    def get_scheduled_jobs(self) -> list[dict]:
        """Return info about all scheduled jobs."""
        jobs = []
        for job in self._scheduler.get_jobs():
            jobs.append(
                {
                    "id": job.id,
                    "name": job.name,
                    "next_run": (
                        job.next_run_time.isoformat()
                        if job.next_run_time
                        else None
                    ),
                }
            )
        return jobs

    @property
    def entries(self) -> list[ScheduleEntry]:
        return self._entries

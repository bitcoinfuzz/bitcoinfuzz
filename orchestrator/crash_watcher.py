"""Background crash directory watcher.

Polls the ``crash/`` subdirectory of each active campaign's data volume
and fires alerts when new crash artefacts appear.
"""

import asyncio
import logging
from pathlib import Path
from typing import Callable, Coroutine, Optional

logger = logging.getLogger(__name__)

# Type alias for the alert callback
AlertCallback = Callable[[str, str, Path], Coroutine]
# Signature: async callback(campaign_id, target_name, crash_file_path)


class CrashWatcher:
    """Watches crash directories for active campaigns.

    Runs as an asyncio background task.  For each registered campaign it
    periodically scans the crash directory and calls the alert callback
    when new files appear.
    """

    def __init__(
        self,
        data_dir: Path,
        poll_interval: int = 10,
        alert_callback: Optional[AlertCallback] = None,
    ) -> None:
        """
        Args:
            data_dir: Root data directory (e.g. ``./docker``).  Campaign
                      crash files live at ``<data_dir>/<target>/crash/``.
            poll_interval: Seconds between scans.
            alert_callback: Async callable invoked for each new crash.
        """
        self._data_dir = Path(data_dir)
        self._poll_interval = poll_interval
        self._alert_callback = alert_callback

        # campaign_id -> (target_name, set of known crash file names)
        self._watched: dict[str, tuple[str, set[str]]] = {}
        self._task: Optional[asyncio.Task] = None

    # -- registration --------------------------------------------------------

    def register(self, campaign_id: str, target_name: str) -> None:
        """Start watching the crash directory for a campaign."""
        crash_dir = self._crash_dir_for(target_name)
        known = self._scan_existing(crash_dir)
        self._watched[campaign_id] = (target_name, known)
        logger.info(
            "Watching crash dir for campaign %s (target=%s, existing=%d files)",
            campaign_id,
            target_name,
            len(known),
        )

    def unregister(self, campaign_id: str) -> None:
        """Stop watching a campaign's crash directory."""
        self._watched.pop(campaign_id, None)
        logger.debug("Stopped watching crashes for campaign %s", campaign_id)

    # -- background task -----------------------------------------------------

    def start(self) -> None:
        """Start the background polling task."""
        if self._task is None or self._task.done():
            self._task = asyncio.create_task(self._poll_loop())
            logger.info("Crash watcher started (interval=%ds)", self._poll_interval)

    def stop(self) -> None:
        """Cancel the background polling task."""
        if self._task and not self._task.done():
            self._task.cancel()
            logger.info("Crash watcher stopped")

    async def _poll_loop(self) -> None:
        """Main polling loop — runs until cancelled."""
        while True:
            try:
                await self._scan_all()
            except asyncio.CancelledError:
                raise
            except Exception:
                logger.exception("Error in crash watcher poll loop")
            await asyncio.sleep(self._poll_interval)

    async def _scan_all(self) -> None:
        """Scan all registered campaigns for new crashes."""
        for campaign_id, (target_name, known) in list(self._watched.items()):
            crash_dir = self._crash_dir_for(target_name)
            if not crash_dir.exists():
                continue

            current = set(self._list_crash_files(crash_dir))
            new_files = current - known

            for fname in sorted(new_files):
                crash_path = crash_dir / fname
                logger.warning(
                    "New crash detected: campaign=%s target=%s file=%s",
                    campaign_id,
                    target_name,
                    crash_path,
                )
                if self._alert_callback:
                    try:
                        await self._alert_callback(
                            campaign_id, target_name, crash_path
                        )
                    except Exception:
                        logger.exception(
                            "Alert callback failed for crash %s", crash_path
                        )

            # Update known set
            known.update(new_files)

    # -- helpers -------------------------------------------------------------

    def _crash_dir_for(self, target_name: str) -> Path:
        """Return the path to a target's crash directory."""
        return self._data_dir / target_name / "crash"

    @staticmethod
    def _scan_existing(crash_dir: Path) -> set[str]:
        """Return the set of filenames currently in a crash directory."""
        if not crash_dir.exists():
            return set()
        return set(CrashWatcher._list_crash_files(crash_dir))

    @staticmethod
    def _list_crash_files(crash_dir: Path) -> list[str]:
        """List filenames in a crash directory, ignoring subdirs."""
        try:
            return [
                entry.name
                for entry in crash_dir.iterdir()
                if entry.is_file()
            ]
        except OSError:
            return []

    # -- inspection ----------------------------------------------------------

    def get_crash_count(self, campaign_id: str) -> int:
        """Return the number of known crash files for a campaign."""
        entry = self._watched.get(campaign_id)
        if entry is None:
            return 0
        _, known = entry
        return len(known)

    @property
    def watched_campaigns(self) -> list[str]:
        """Return IDs of campaigns currently being watched."""
        return list(self._watched.keys())

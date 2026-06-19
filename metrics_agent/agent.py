"""Metrics agent — attaches to a bitcoinfuzz container and streams metrics.

Runs as an asyncio task per campaign.  Attaches to the Docker container's
log stream, parses libFuzzer output, and updates the shared Prometheus
collector.  Also runs corpus and crash directory watchers.
"""

import asyncio
import logging
from pathlib import Path
from typing import Optional

from .collector import MetricsCollector
from .parser import FuzzStats, ReadStats, FuzzEvent, parse_line
from .watcher import CorpusWatcher, CrashDirectoryWatcher

logger = logging.getLogger(__name__)


class MetricsAgent:
    """Per-campaign metrics collection agent.

    Attaches to a running container's log stream (via docker-py),
    parses libFuzzer output lines, updates Prometheus metrics, and
    watches corpus/crash directories for growth.
    """

    def __init__(
        self,
        campaign_id: str,
        target_name: str,
        container_id: str,
        data_dir: Path,
        collector: MetricsCollector,
        docker_client=None,
    ) -> None:
        self._campaign_id = campaign_id
        self._target = target_name
        self._container_id = container_id
        self._data_dir = data_dir
        self._collector = collector
        self._docker = docker_client

        # Directory watchers
        target_dir = data_dir / target_name
        self._corpus_watcher = CorpusWatcher(target_dir / "corpus")
        self._crash_watcher = CrashDirectoryWatcher(target_dir / "crash")

        # Latest stats (for WebSocket streaming)
        self._latest_stats: Optional[FuzzStats] = None
        self._task: Optional[asyncio.Task] = None
        self._watcher_task: Optional[asyncio.Task] = None

    async def start(self) -> None:
        """Start the log parsing and directory watching tasks."""
        self._collector.set_container_status(
            self._target, self._campaign_id, True
        )
        self._task = asyncio.create_task(self._parse_log_stream())
        self._watcher_task = asyncio.create_task(self._watch_directories())
        logger.info(
            "Metrics agent started for campaign %s (target=%s)",
            self._campaign_id,
            self._target,
        )

    async def stop(self) -> None:
        """Stop all agent tasks."""
        for task in [self._task, self._watcher_task]:
            if task and not task.done():
                task.cancel()
                try:
                    await task
                except asyncio.CancelledError:
                    pass
        self._collector.set_container_status(
            self._target, self._campaign_id, False
        )
        logger.info(
            "Metrics agent stopped for campaign %s", self._campaign_id
        )

    async def _parse_log_stream(self) -> None:
        """Attach to container logs and parse each line."""
        if self._docker is None:
            logger.warning("No Docker client — agent running in watch-only mode")
            return

        try:
            container = self._docker.containers.get(self._container_id)
            log_stream = container.logs(
                follow=True, stream=True, tail=0, timestamps=False
            )

            for chunk in log_stream:
                line = chunk.decode("utf-8", errors="replace").rstrip("\n")
                if not line:
                    continue

                result = parse_line(line)
                if isinstance(result, FuzzStats):
                    self._latest_stats = result
                    self._collector.update_from_fuzz_stats(
                        target=self._target,
                        campaign_id=self._campaign_id,
                        iteration=result.iteration,
                        coverage=result.coverage,
                        features=result.features,
                        corpus_count=result.corpus_count,
                        corpus_bytes=result.corpus_bytes,
                        exec_per_sec=result.exec_per_sec,
                        rss_mb=result.rss_mb,
                    )
                elif isinstance(result, FuzzEvent):
                    if result.event_type in ("OOM", "TIMEOUT", "CRASH"):
                        logger.warning(
                            "Fuzzer event %s in campaign %s: %s",
                            result.event_type,
                            self._campaign_id,
                            result.raw_line[:200],
                        )

                # Yield control to avoid blocking the event loop
                await asyncio.sleep(0)

        except asyncio.CancelledError:
            raise
        except Exception as exc:
            logger.error(
                "Log stream error for campaign %s: %s",
                self._campaign_id,
                exc,
            )

    async def _watch_directories(self) -> None:
        """Periodically scan corpus and crash directories."""
        while True:
            try:
                # Corpus
                corpus_stats = await asyncio.to_thread(
                    self._corpus_watcher.scan
                )
                self._collector.update_corpus_stats(
                    self._target,
                    self._campaign_id,
                    corpus_stats.file_count,
                    corpus_stats.total_bytes,
                )

                # Crashes
                crash_count = await asyncio.to_thread(
                    self._crash_watcher.scan
                )
                self._collector.update_crash_count(
                    self._target, self._campaign_id, crash_count
                )
            except asyncio.CancelledError:
                raise
            except Exception:
                logger.exception(
                    "Directory watcher error for campaign %s",
                    self._campaign_id,
                )

            await asyncio.sleep(15)

    @property
    def latest_stats(self) -> Optional[FuzzStats]:
        return self._latest_stats

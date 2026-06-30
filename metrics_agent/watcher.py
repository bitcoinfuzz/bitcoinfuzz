"""Corpus and crash directory watchers.

Polls directories on the shared Docker volume to track corpus growth
and crash counts without reading file contents (only metadata).
"""

import logging
import os
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

logger = logging.getLogger(__name__)


@dataclass
class DirectoryStats:
    """Snapshot of a directory's metadata."""

    file_count: int = 0
    total_bytes: int = 0
    newest_mtime: float = 0.0
    last_scan_time: float = 0.0


class CorpusWatcher:
    """Watches a corpus directory for growth.

    Scans the directory at a configurable interval and tracks:
    - Total file count
    - Total size in bytes
    - Modification time of the newest file
    """

    def __init__(self, corpus_path: Path, poll_interval: int = 15) -> None:
        self._path = corpus_path
        self._poll_interval = poll_interval
        self._stats = DirectoryStats()
        self._last_scan: float = 0.0

    def scan(self) -> DirectoryStats:
        """Scan the corpus directory and update stats.

        This is intentionally non-destructive — it reads only file
        metadata (name, size, mtime), never file contents.
        """
        now = time.time()
        if now - self._last_scan < self._poll_interval:
            return self._stats

        self._last_scan = now

        if not self._path.exists():
            self._stats = DirectoryStats(last_scan_time=now)
            return self._stats

        file_count = 0
        total_bytes = 0
        newest_mtime = 0.0

        try:
            with os.scandir(self._path) as entries:
                for entry in entries:
                    if entry.is_file(follow_symlinks=False):
                        try:
                            stat = entry.stat(follow_symlinks=False)
                            file_count += 1
                            total_bytes += stat.st_size
                            if stat.st_mtime > newest_mtime:
                                newest_mtime = stat.st_mtime
                        except OSError:
                            continue
        except OSError as exc:
            logger.debug("Error scanning corpus dir %s: %s", self._path, exc)

        self._stats = DirectoryStats(
            file_count=file_count,
            total_bytes=total_bytes,
            newest_mtime=newest_mtime,
            last_scan_time=now,
        )
        return self._stats

    @property
    def stats(self) -> DirectoryStats:
        return self._stats


class CrashDirectoryWatcher:
    """Watches a crash directory and counts new crash files.

    Unlike the orchestrator's CrashWatcher (which fires alerts), this
    watcher is purely for metrics — it exposes a monotonic crash count
    for Prometheus scraping.
    """

    def __init__(self, crash_path: Path, poll_interval: int = 10) -> None:
        self._path = crash_path
        self._poll_interval = poll_interval
        self._crash_count: int = 0
        self._known_files: set[str] = set()
        self._last_scan: float = 0.0

        # Initialise with existing crash files
        self._scan_initial()

    def _scan_initial(self) -> None:
        """Record existing crash files so we don't double-count."""
        if not self._path.exists():
            return
        try:
            with os.scandir(self._path) as entries:
                for entry in entries:
                    if entry.is_file(follow_symlinks=False):
                        self._known_files.add(entry.name)
        except OSError:
            pass
        self._crash_count = len(self._known_files)

    def scan(self) -> int:
        """Scan for new crash files and return total crash count."""
        now = time.time()
        if now - self._last_scan < self._poll_interval:
            return self._crash_count

        self._last_scan = now

        if not self._path.exists():
            return self._crash_count

        try:
            with os.scandir(self._path) as entries:
                for entry in entries:
                    if entry.is_file(follow_symlinks=False):
                        if entry.name not in self._known_files:
                            self._known_files.add(entry.name)
                            self._crash_count += 1
                            logger.info(
                                "New crash file detected: %s", entry.name
                            )
        except OSError as exc:
            logger.debug("Error scanning crash dir %s: %s", self._path, exc)

        return self._crash_count

    @property
    def crash_count(self) -> int:
        return self._crash_count

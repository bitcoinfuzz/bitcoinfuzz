"""Tests for the crash watcher."""

import asyncio
import tempfile
from pathlib import Path

import pytest

from orchestrator.crash_watcher import CrashWatcher


@pytest.fixture
def data_dir():
    with tempfile.TemporaryDirectory() as tmpdir:
        yield Path(tmpdir)


class TestCrashWatcher:
    """Test crash directory watching and alerting."""

    def test_register_empty_dir(self, data_dir):
        watcher = CrashWatcher(data_dir=data_dir, poll_interval=1)
        watcher.register("c1", "script")
        assert "c1" in watcher.watched_campaigns
        assert watcher.get_crash_count("c1") == 0

    def test_register_with_existing_crashes(self, data_dir):
        crash_dir = data_dir / "script" / "crash"
        crash_dir.mkdir(parents=True)
        (crash_dir / "crash-001").write_text("crash data")
        (crash_dir / "crash-002").write_text("crash data")

        watcher = CrashWatcher(data_dir=data_dir, poll_interval=1)
        watcher.register("c1", "script")
        # Existing crashes should be counted but not trigger alerts
        assert watcher.get_crash_count("c1") == 2

    def test_unregister(self, data_dir):
        watcher = CrashWatcher(data_dir=data_dir, poll_interval=1)
        watcher.register("c1", "script")
        watcher.unregister("c1")
        assert "c1" not in watcher.watched_campaigns

    def test_detect_new_crash(self, data_dir):
        crash_dir = data_dir / "script" / "crash"
        crash_dir.mkdir(parents=True)

        alerts = []

        async def alert_cb(campaign_id, target_name, crash_path):
            alerts.append((campaign_id, target_name, crash_path))

        watcher = CrashWatcher(
            data_dir=data_dir, poll_interval=1, alert_callback=alert_cb
        )
        watcher.register("c1", "script")

        # Write a new crash file
        (crash_dir / "crash-new").write_text("crash")

        # Manually trigger scan
        asyncio.run(watcher._scan_all())

        assert len(alerts) == 1
        assert alerts[0][0] == "c1"
        assert alerts[0][1] == "script"
        assert alerts[0][2].name == "crash-new"

    def test_no_false_positives(self, data_dir):
        crash_dir = data_dir / "script" / "crash"
        crash_dir.mkdir(parents=True)
        (crash_dir / "existing-crash").write_text("data")

        alerts = []

        async def alert_cb(campaign_id, target_name, crash_path):
            alerts.append(crash_path)

        watcher = CrashWatcher(
            data_dir=data_dir, poll_interval=1, alert_callback=alert_cb
        )
        watcher.register("c1", "script")

        # Scan should not alert on existing files
        asyncio.run(watcher._scan_all())
        assert len(alerts) == 0

    def test_multiple_new_crashes(self, data_dir):
        crash_dir = data_dir / "script" / "crash"
        crash_dir.mkdir(parents=True)

        alerts = []

        async def alert_cb(campaign_id, target_name, crash_path):
            alerts.append(crash_path.name)

        watcher = CrashWatcher(
            data_dir=data_dir, poll_interval=1, alert_callback=alert_cb
        )
        watcher.register("c1", "script")

        # Write multiple crash files
        for i in range(5):
            (crash_dir / f"crash-{i:03d}").write_text(f"crash {i}")

        asyncio.run(watcher._scan_all())
        assert len(alerts) == 5

    def test_missing_crash_dir(self, data_dir):
        # Should not raise if crash dir doesn't exist
        watcher = CrashWatcher(data_dir=data_dir, poll_interval=1)
        watcher.register("c1", "script")
        asyncio.run(watcher._scan_all())
        assert watcher.get_crash_count("c1") == 0

    def test_get_crash_count_unknown_campaign(self, data_dir):
        watcher = CrashWatcher(data_dir=data_dir, poll_interval=1)
        assert watcher.get_crash_count("nonexistent") == 0

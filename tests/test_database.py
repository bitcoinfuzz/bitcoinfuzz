"""Tests for the SQLite database persistence layer."""

import pytest
import tempfile
from pathlib import Path

from orchestrator.database import Database
from orchestrator.models import Campaign, CampaignState, ResourceLimits


@pytest.fixture
def db():
    """Create an in-memory database for testing."""
    with tempfile.TemporaryDirectory() as tmpdir:
        db_path = Path(tmpdir) / "test.db"
        database = Database(db_path)
        database.connect()
        yield database
        database.close()


class TestDatabaseCampaignCRUD:
    """Test basic CRUD operations on campaigns."""

    def test_create_and_get(self, db: Database):
        campaign = Campaign(target_name="script")
        db.create_campaign(campaign)
        loaded = db.get_campaign(campaign.id)
        assert loaded is not None
        assert loaded.target_name == "script"
        assert loaded.state == CampaignState.QUEUED

    def test_get_nonexistent(self, db: Database):
        assert db.get_campaign("nonexistent") is None

    def test_update_campaign(self, db: Database):
        campaign = Campaign(target_name="script")
        db.create_campaign(campaign)
        campaign.transition_to(CampaignState.STARTING)
        campaign.transition_to(CampaignState.RUNNING)
        campaign.container_id = "abc123"
        db.update_campaign(campaign)
        loaded = db.get_campaign(campaign.id)
        assert loaded.state == CampaignState.RUNNING
        assert loaded.container_id == "abc123"

    def test_list_campaigns(self, db: Database):
        for name in ["script", "psbt_parse", "ecdh"]:
            db.create_campaign(Campaign(target_name=name))
        all_campaigns = db.list_campaigns()
        assert len(all_campaigns) == 3

    def test_list_by_state(self, db: Database):
        c1 = Campaign(target_name="script")
        c2 = Campaign(target_name="psbt_parse")
        db.create_campaign(c1)
        db.create_campaign(c2)
        c1.transition_to(CampaignState.STARTING)
        c1.transition_to(CampaignState.RUNNING)
        db.update_campaign(c1)
        running = db.list_campaigns(state=CampaignState.RUNNING)
        assert len(running) == 1
        assert running[0].target_name == "script"

    def test_list_by_target(self, db: Database):
        db.create_campaign(Campaign(target_name="script"))
        db.create_campaign(Campaign(target_name="script"))
        db.create_campaign(Campaign(target_name="ecdh"))
        scripts = db.list_campaigns(target_name="script")
        assert len(scripts) == 2

    def test_delete_campaign(self, db: Database):
        campaign = Campaign(target_name="script")
        db.create_campaign(campaign)
        assert db.delete_campaign(campaign.id) is True
        assert db.get_campaign(campaign.id) is None

    def test_delete_nonexistent(self, db: Database):
        assert db.delete_campaign("nope") is False

    def test_env_overrides_roundtrip(self, db: Database):
        campaign = Campaign(
            target_name="script",
            env_overrides={"LIBFUZZ_DETECT_LEAKS": "0", "ASAN_OPTIONS": "detect_leaks=0"},
        )
        db.create_campaign(campaign)
        loaded = db.get_campaign(campaign.id)
        assert loaded.env_overrides == {"LIBFUZZ_DETECT_LEAKS": "0", "ASAN_OPTIONS": "detect_leaks=0"}

    def test_resource_limits_roundtrip(self, db: Database):
        limits = ResourceLimits(cpu_quota=400_000, memory_mb=4096)
        campaign = Campaign(target_name="script", resource_limits=limits)
        db.create_campaign(campaign)
        loaded = db.get_campaign(campaign.id)
        assert loaded.resource_limits.cpu_quota == 400_000
        assert loaded.resource_limits.memory_mb == 4096


class TestDatabaseArchival:
    """Test campaign archival to summaries table."""

    def test_archive_campaign(self, db: Database):
        campaign = Campaign(target_name="script")
        campaign.transition_to(CampaignState.STARTING)
        campaign.transition_to(CampaignState.RUNNING)
        campaign.transition_to(CampaignState.COMPLETED)
        campaign.exit_reason = "completed"
        campaign.final_coverage = 847
        campaign.final_corpus_count = 412
        campaign.crash_count = 2
        db.create_campaign(campaign)
        db.archive_campaign(campaign, cxxflags="-DBITCOIN_CORE -DRUST_BITCOIN")

        history = db.get_target_history("script")
        assert len(history) == 1
        assert history[0]["final_coverage"] == 847
        assert history[0]["crash_count"] == 2
        assert history[0]["cxxflags"] == "-DBITCOIN_CORE -DRUST_BITCOIN"

    def test_archive_with_duration(self, db: Database):
        campaign = Campaign(target_name="script")
        campaign.transition_to(CampaignState.STARTING)
        campaign.transition_to(CampaignState.RUNNING)
        campaign.transition_to(CampaignState.COMPLETED)
        db.create_campaign(campaign)
        db.archive_campaign(campaign)
        history = db.get_target_history("script")
        assert history[0]["duration_seconds"] is not None

    def test_compare_campaigns(self, db: Database):
        ids = []
        for i in range(3):
            c = Campaign(target_name="script")
            c.transition_to(CampaignState.STARTING)
            c.transition_to(CampaignState.RUNNING)
            c.transition_to(CampaignState.COMPLETED)
            c.final_coverage = 100 * (i + 1)
            db.create_campaign(c)
            db.archive_campaign(c)
            ids.append(c.id)

        compared = db.compare_campaigns(ids[:2])
        assert len(compared) == 2


class TestDatabaseCrashRecords:
    """Test crash record operations."""

    def test_record_crash(self, db: Database):
        campaign = Campaign(target_name="script")
        db.create_campaign(campaign)
        crash_id = db.record_crash(
            campaign.id, "script", "/data/script/crash/crash-abc", 1024
        )
        assert crash_id is not None

        crashes = db.get_crashes(campaign_id=campaign.id)
        assert len(crashes) == 1
        assert crashes[0]["file_path"] == "/data/script/crash/crash-abc"
        assert crashes[0]["file_size"] == 1024

    def test_crash_increments_campaign_count(self, db: Database):
        campaign = Campaign(target_name="script")
        db.create_campaign(campaign)
        db.record_crash(campaign.id, "script", "/crash1", 100)
        db.record_crash(campaign.id, "script", "/crash2", 200)
        loaded = db.get_campaign(campaign.id)
        assert loaded.crash_count == 2

    def test_get_crashes_by_target(self, db: Database):
        c1 = Campaign(target_name="script")
        c2 = Campaign(target_name="ecdh")
        db.create_campaign(c1)
        db.create_campaign(c2)
        db.record_crash(c1.id, "script", "/crash1")
        db.record_crash(c2.id, "ecdh", "/crash2")
        script_crashes = db.get_crashes(target_name="script")
        assert len(script_crashes) == 1


class TestDashboardStats:
    """Test dashboard aggregate stats."""

    def test_dashboard_stats(self, db: Database):
        c1 = Campaign(target_name="script")
        c2 = Campaign(target_name="ecdh")
        c3 = Campaign(target_name="psbt_parse")
        db.create_campaign(c1)
        db.create_campaign(c2)
        db.create_campaign(c3)

        c1.transition_to(CampaignState.STARTING)
        c1.transition_to(CampaignState.RUNNING)
        db.update_campaign(c1)

        c3.transition_to(CampaignState.STARTING)
        c3.transition_to(CampaignState.RUNNING)
        c3.transition_to(CampaignState.COMPLETED)
        db.update_campaign(c3)

        stats = db.get_dashboard_stats()
        assert stats["active_campaigns"] == 1  # c1 running
        assert stats["queued_campaigns"] == 1  # c2 queued
        assert stats["completed_campaigns"] == 1  # c3 completed

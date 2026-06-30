"""Tests for the campaign state machine and data models."""

import pytest
from datetime import datetime, timezone

from orchestrator.models import (
    Campaign,
    CampaignCreate,
    CampaignState,
    ResourceLimits,
    TERMINAL_STATES,
    VALID_TRANSITIONS,
    validate_transition,
)


# ---------------------------------------------------------------------------
# State machine transitions
# ---------------------------------------------------------------------------

class TestValidateTransition:
    """Test the state transition validation function."""

    def test_queued_to_starting(self):
        assert validate_transition(CampaignState.QUEUED, CampaignState.STARTING) is True

    def test_queued_to_running_invalid(self):
        assert validate_transition(CampaignState.QUEUED, CampaignState.RUNNING) is False

    def test_starting_to_running(self):
        assert validate_transition(CampaignState.STARTING, CampaignState.RUNNING) is True

    def test_starting_to_crashed(self):
        assert validate_transition(CampaignState.STARTING, CampaignState.CRASHED) is True

    def test_running_to_paused(self):
        assert validate_transition(CampaignState.RUNNING, CampaignState.PAUSED) is True

    def test_running_to_crashed(self):
        assert validate_transition(CampaignState.RUNNING, CampaignState.CRASHED) is True

    def test_running_to_completed(self):
        assert validate_transition(CampaignState.RUNNING, CampaignState.COMPLETED) is True

    def test_paused_to_running(self):
        assert validate_transition(CampaignState.PAUSED, CampaignState.RUNNING) is True

    def test_paused_to_completed(self):
        assert validate_transition(CampaignState.PAUSED, CampaignState.COMPLETED) is True

    def test_crashed_to_restarting(self):
        assert validate_transition(CampaignState.CRASHED, CampaignState.RESTARTING) is True

    def test_crashed_to_failed(self):
        assert validate_transition(CampaignState.CRASHED, CampaignState.FAILED) is True

    def test_crashed_to_running_invalid(self):
        assert validate_transition(CampaignState.CRASHED, CampaignState.RUNNING) is False

    def test_restarting_to_starting(self):
        assert validate_transition(CampaignState.RESTARTING, CampaignState.STARTING) is True

    def test_completed_is_terminal(self):
        for target_state in CampaignState:
            assert validate_transition(CampaignState.COMPLETED, target_state) is False

    def test_failed_is_terminal(self):
        for target_state in CampaignState:
            assert validate_transition(CampaignState.FAILED, target_state) is False

    def test_any_active_can_fail(self):
        """All non-terminal states should be able to transition to FAILED."""
        for state in CampaignState:
            if state not in TERMINAL_STATES:
                assert validate_transition(state, CampaignState.FAILED) is True


# ---------------------------------------------------------------------------
# Campaign lifecycle
# ---------------------------------------------------------------------------

class TestCampaign:
    """Test the Campaign dataclass."""

    def test_default_creation(self):
        c = Campaign(target_name="script")
        assert c.state == CampaignState.QUEUED
        assert c.target_name == "script"
        assert c.retry_count == 0
        assert c.is_active is True
        assert c.is_running is False
        assert len(c.id) == 12

    def test_transition_to_running(self):
        c = Campaign(target_name="script")
        c.transition_to(CampaignState.STARTING)
        c.transition_to(CampaignState.RUNNING)
        assert c.state == CampaignState.RUNNING
        assert c.started_at is not None
        assert c.is_running is True

    def test_transition_to_completed(self):
        c = Campaign(target_name="script")
        c.transition_to(CampaignState.STARTING)
        c.transition_to(CampaignState.RUNNING)
        c.transition_to(CampaignState.COMPLETED)
        assert c.state == CampaignState.COMPLETED
        assert c.ended_at is not None
        assert c.is_active is False

    def test_invalid_transition_raises(self):
        c = Campaign(target_name="script")
        with pytest.raises(ValueError, match="Invalid state transition"):
            c.transition_to(CampaignState.COMPLETED)

    def test_crash_restart_cycle(self):
        c = Campaign(target_name="script")
        c.transition_to(CampaignState.STARTING)
        c.transition_to(CampaignState.RUNNING)
        c.transition_to(CampaignState.CRASHED)
        c.transition_to(CampaignState.RESTARTING)
        c.transition_to(CampaignState.STARTING)
        c.transition_to(CampaignState.RUNNING)
        assert c.state == CampaignState.RUNNING
        assert c.started_at is not None

    def test_crash_to_failed(self):
        c = Campaign(target_name="script", max_retries=0)
        c.transition_to(CampaignState.STARTING)
        c.transition_to(CampaignState.RUNNING)
        c.transition_to(CampaignState.CRASHED)
        c.transition_to(CampaignState.FAILED)
        assert c.state == CampaignState.FAILED
        assert c.is_active is False

    def test_to_dict(self):
        c = Campaign(target_name="script")
        d = c.to_dict()
        assert d["target_name"] == "script"
        assert d["state"] == "queued"
        assert isinstance(d["resource_limits"], dict)
        assert "cpu_quota" in d["resource_limits"]

    def test_started_at_set_only_on_first_running(self):
        c = Campaign(target_name="script")
        c.transition_to(CampaignState.STARTING)
        c.transition_to(CampaignState.RUNNING)
        first_start = c.started_at
        c.transition_to(CampaignState.CRASHED)
        c.transition_to(CampaignState.RESTARTING)
        c.transition_to(CampaignState.STARTING)
        c.transition_to(CampaignState.RUNNING)
        assert c.started_at == first_start  # Should not change


# ---------------------------------------------------------------------------
# Resource limits
# ---------------------------------------------------------------------------

class TestResourceLimits:
    def test_defaults(self):
        rl = ResourceLimits()
        assert rl.cpu_quota == 200_000
        assert rl.memory_mb == 2048

    def test_custom_values(self):
        rl = ResourceLimits(cpu_quota=400_000, memory_mb=4096)
        assert rl.cpu_quota == 400_000
        assert rl.memory_mb == 4096

    def test_frozen(self):
        rl = ResourceLimits()
        with pytest.raises(AttributeError):
            rl.cpu_quota = 100  # type: ignore


# ---------------------------------------------------------------------------
# Pydantic models
# ---------------------------------------------------------------------------

class TestCampaignCreate:
    def test_valid_request(self):
        req = CampaignCreate(target_name="script")
        assert req.target_name == "script"
        assert req.cpu_quota == 200_000
        assert req.memory_mb == 2048
        assert req.priority == 1

    def test_with_overrides(self):
        req = CampaignCreate(
            target_name="psbt_parse",
            modules="BITCOIN_CORE,RUST_BITCOIN",
            cpu_quota=400_000,
            memory_mb=4096,
            priority=5,
            env_overrides={"LIBFUZZ_DETECT_LEAKS": "0"},
        )
        assert req.modules == "BITCOIN_CORE,RUST_BITCOIN"
        assert req.priority == 5

    def test_priority_validation(self):
        with pytest.raises(Exception):  # Pydantic validation error
            CampaignCreate(target_name="script", priority=0)

    def test_priority_max_validation(self):
        with pytest.raises(Exception):
            CampaignCreate(target_name="script", priority=11)

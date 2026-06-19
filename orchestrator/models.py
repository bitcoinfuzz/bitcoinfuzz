"""Core data models for campaign orchestration.

Defines the campaign state machine, data classes for campaigns and resource
limits, and Pydantic models for API request/response validation.
"""

import enum
import uuid
from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Optional

from pydantic import BaseModel, Field


# ---------------------------------------------------------------------------
# State machine
# ---------------------------------------------------------------------------

class CampaignState(str, enum.Enum):
    """Campaign lifecycle states.

    Valid transitions::

        QUEUED -> STARTING -> RUNNING -> COMPLETED
                                |   ^        |
                                v   |        v
                             PAUSED  |    (archive)
                                |   |
                                v   |
                             CRASHED -> RESTARTING -> RUNNING
                                |
                                v
                              FAILED (max retries exceeded)

    Any state may transition to FAILED on unrecoverable error.
    RUNNING may transition directly to COMPLETED when stopped by the user.
    """

    QUEUED = "queued"
    STARTING = "starting"
    RUNNING = "running"
    PAUSED = "paused"
    CRASHED = "crashed"
    RESTARTING = "restarting"
    COMPLETED = "completed"
    FAILED = "failed"


# Map of valid state transitions: current_state -> set of allowed next states.
VALID_TRANSITIONS: dict[CampaignState, set[CampaignState]] = {
    CampaignState.QUEUED: {CampaignState.STARTING, CampaignState.FAILED},
    CampaignState.STARTING: {
        CampaignState.RUNNING,
        CampaignState.CRASHED,
        CampaignState.FAILED,
    },
    CampaignState.RUNNING: {
        CampaignState.PAUSED,
        CampaignState.CRASHED,
        CampaignState.COMPLETED,
        CampaignState.FAILED,
    },
    CampaignState.PAUSED: {
        CampaignState.RUNNING,
        CampaignState.CRASHED,
        CampaignState.COMPLETED,
        CampaignState.FAILED,
    },
    CampaignState.CRASHED: {
        CampaignState.RESTARTING,
        CampaignState.FAILED,
    },
    CampaignState.RESTARTING: {
        CampaignState.STARTING,
        CampaignState.FAILED,
    },
    CampaignState.COMPLETED: set(),  # Terminal state
    CampaignState.FAILED: set(),  # Terminal state
}

TERMINAL_STATES = {CampaignState.COMPLETED, CampaignState.FAILED}


def validate_transition(current: CampaignState, target: CampaignState) -> bool:
    """Return True if transitioning from *current* to *target* is valid."""
    return target in VALID_TRANSITIONS.get(current, set())


# ---------------------------------------------------------------------------
# Resource limits
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class ResourceLimits:
    """CPU and memory limits for a single campaign container.

    Attributes:
        cpu_quota: Docker CPU quota in microseconds per 100ms period.
                   200_000 means 2 CPUs.  0 means unlimited.
        memory_mb: Memory limit in megabytes.  0 means unlimited.
    """

    cpu_quota: int = 200_000  # 2 CPUs
    memory_mb: int = 2048  # 2 GB


# ---------------------------------------------------------------------------
# Campaign data
# ---------------------------------------------------------------------------

def _generate_id() -> str:
    """Generate a short campaign identifier."""
    return uuid.uuid4().hex[:12]


def _now() -> datetime:
    return datetime.now(timezone.utc)


@dataclass
class Campaign:
    """Mutable record of a single fuzzing campaign."""

    id: str = field(default_factory=_generate_id)
    target_name: str = ""
    state: CampaignState = CampaignState.QUEUED

    # Docker
    container_id: Optional[str] = None
    image_tag: Optional[str] = None

    # Lifecycle
    started_at: Optional[datetime] = None
    ended_at: Optional[datetime] = None
    retry_count: int = 0
    max_retries: int = 3

    # Resource limits applied to the container
    resource_limits: ResourceLimits = field(default_factory=ResourceLimits)

    # Scheduling
    priority: int = 1

    # Configuration overrides
    modules: Optional[str] = None
    env_overrides: dict[str, str] = field(default_factory=dict)

    # Timestamps
    created_at: datetime = field(default_factory=_now)
    updated_at: datetime = field(default_factory=_now)

    # Final stats (populated on completion/failure)
    final_coverage: Optional[int] = None
    final_corpus_count: Optional[int] = None
    final_corpus_bytes: Optional[int] = None
    total_executions: Optional[int] = None
    avg_exec_per_sec: Optional[float] = None
    peak_rss_mb: Optional[int] = None
    crash_count: int = 0
    exit_reason: Optional[str] = None  # 'completed', 'crashed', 'stopped', 'timeout'

    def transition_to(self, new_state: CampaignState) -> None:
        """Transition to *new_state*, raising ValueError on illegal moves."""
        if not validate_transition(self.state, new_state):
            raise ValueError(
                f"Invalid state transition: {self.state.value} -> {new_state.value}"
            )
        self.state = new_state
        self.updated_at = _now()

        if new_state == CampaignState.RUNNING and self.started_at is None:
            self.started_at = self.updated_at
        if new_state in TERMINAL_STATES:
            self.ended_at = self.updated_at

    @property
    def is_active(self) -> bool:
        """True if the campaign is in a non-terminal state."""
        return self.state not in TERMINAL_STATES

    @property
    def is_running(self) -> bool:
        return self.state == CampaignState.RUNNING

    def to_dict(self) -> dict:
        """Serialise the campaign to a JSON-friendly dict."""
        return {
            "id": self.id,
            "target_name": self.target_name,
            "state": self.state.value,
            "container_id": self.container_id,
            "image_tag": self.image_tag,
            "started_at": self.started_at.isoformat() if self.started_at else None,
            "ended_at": self.ended_at.isoformat() if self.ended_at else None,
            "retry_count": self.retry_count,
            "max_retries": self.max_retries,
            "resource_limits": {
                "cpu_quota": self.resource_limits.cpu_quota,
                "memory_mb": self.resource_limits.memory_mb,
            },
            "priority": self.priority,
            "modules": self.modules,
            "env_overrides": self.env_overrides,
            "created_at": self.created_at.isoformat(),
            "updated_at": self.updated_at.isoformat(),
            "crash_count": self.crash_count,
            "exit_reason": self.exit_reason,
            "final_coverage": self.final_coverage,
            "final_corpus_count": self.final_corpus_count,
            "total_executions": self.total_executions,
            "avg_exec_per_sec": self.avg_exec_per_sec,
            "peak_rss_mb": self.peak_rss_mb,
        }


# ---------------------------------------------------------------------------
# Pydantic API models
# ---------------------------------------------------------------------------

class CampaignCreate(BaseModel):
    """Request body for POST /campaigns."""

    target_name: str = Field(..., description="Name of the fuzz target from docker-compose.yml")
    modules: Optional[str] = Field(
        None,
        description="Comma-separated list of modules to load at runtime (e.g. 'BITCOIN_CORE,RUST_BITCOIN')",
    )
    cpu_quota: int = Field(
        200_000,
        ge=0,
        description="Docker CPU quota in microseconds per 100ms period (200000 = 2 CPUs)",
    )
    memory_mb: int = Field(
        2048,
        ge=0,
        description="Memory limit in megabytes",
    )
    priority: int = Field(1, ge=1, le=10, description="Scheduling priority (1=lowest, 10=highest)")
    max_retries: int = Field(3, ge=0, le=20, description="Max auto-restart attempts after crash")
    env_overrides: dict[str, str] = Field(
        default_factory=dict,
        description="Additional environment variables to pass to the container",
    )
    max_duration_seconds: Optional[int] = Field(
        None,
        ge=0,
        description="Maximum campaign duration in seconds (0 or None = unlimited)",
    )


class CampaignResponse(BaseModel):
    """Response model for campaign endpoints."""

    id: str
    target_name: str
    state: str
    container_id: Optional[str] = None
    started_at: Optional[str] = None
    ended_at: Optional[str] = None
    retry_count: int = 0
    max_retries: int = 3
    resource_limits: dict = {}
    priority: int = 1
    modules: Optional[str] = None
    crash_count: int = 0
    exit_reason: Optional[str] = None
    created_at: str = ""
    updated_at: str = ""


class CampaignSummary(BaseModel):
    """Historical campaign summary from SQLite."""

    id: str
    target_name: str
    started_at: Optional[str] = None
    ended_at: Optional[str] = None
    duration_seconds: Optional[int] = None
    final_coverage: Optional[int] = None
    final_corpus_count: Optional[int] = None
    final_corpus_bytes: Optional[int] = None
    total_executions: Optional[int] = None
    avg_exec_per_sec: Optional[float] = None
    peak_rss_mb: Optional[int] = None
    crash_count: int = 0
    exit_reason: Optional[str] = None
    modules: Optional[str] = None
    cxxflags: Optional[str] = None


class CrashRecord(BaseModel):
    """A single crash artifact record."""

    id: str
    campaign_id: str
    target_name: str
    discovered_at: str
    file_path: str
    file_size: Optional[int] = None


class DashboardSummary(BaseModel):
    """Aggregate stats for the dashboard overview."""

    active_campaigns: int = 0
    queued_campaigns: int = 0
    completed_campaigns: int = 0
    failed_campaigns: int = 0
    total_crashes: int = 0
    total_targets: int = 0
    cpu_allocated: int = 0
    cpu_total: int = 0
    memory_allocated_mb: int = 0
    memory_total_mb: int = 0


class HealthResponse(BaseModel):
    """Response for GET /health."""

    status: str = "ok"
    version: str = "0.1.0"
    active_campaigns: int = 0
    uptime_seconds: float = 0.0

"""Orchestrator configuration.

Loads settings from environment variables and an optional YAML config file.
"""

from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

import yaml


@dataclass
class OrchestratorConfig:
    """Central configuration for the orchestrator service."""

    # Paths
    repo_root: Path = field(default_factory=lambda: Path(__file__).resolve().parent.parent)
    compose_file: Path = field(default=None)  # type: ignore[assignment]
    data_dir: Path = field(default=None)  # type: ignore[assignment]
    docs_dir: Path = field(default=None)  # type: ignore[assignment]
    db_path: Path = field(default=None)  # type: ignore[assignment]

    # Campaign lifecycle
    max_retries: int = 3
    health_poll_interval: int = 5  # seconds between container health checks
    crash_poll_interval: int = 10  # seconds between crash directory scans

    # Resource defaults for new campaigns
    default_cpu_quota: int = 200_000  # 2 CPUs (microseconds per 100ms)
    default_memory_mb: int = 2048  # 2 GB

    # Scheduling
    schedule_file: Optional[Path] = None  # YAML file with cron schedules

    # Server
    host: str = "127.0.0.1"
    port: int = 8000

    # Prometheus
    metrics_port: int = 9091  # Port for the metrics agent HTTP server

    def __post_init__(self) -> None:
        if self.compose_file is None:
            self.compose_file = self.repo_root / "docker-compose.yml"
        if self.data_dir is None:
            self.data_dir = self.repo_root / "docker"
        if self.docs_dir is None:
            self.docs_dir = self.repo_root / "agent-docs"
        if self.db_path is None:
            self.db_path = self.repo_root / "orchestrator.db"
        if self.schedule_file is None:
            candidate = self.repo_root / "orchestrator-schedules.yml"
            if candidate.exists():
                self.schedule_file = candidate


def load_default_config() -> OrchestratorConfig:
    """Load configuration with defaults based on repository root."""
    return OrchestratorConfig()


def load_config_from_file(config_path: Path) -> OrchestratorConfig:
    """Load configuration from a YAML file, falling back to defaults.

    Example YAML::

        repo_root: /path/to/bitcoinfuzz
        max_retries: 5
        health_poll_interval: 10
        default_cpu_quota: 400000
        default_memory_mb: 4096
        schedule_file: /path/to/schedules.yml
    """
    config = OrchestratorConfig()

    if not config_path.exists():
        return config

    with config_path.open("r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}

    if "repo_root" in data:
        config.repo_root = Path(data["repo_root"])
    if "compose_file" in data:
        config.compose_file = Path(data["compose_file"])
    if "data_dir" in data:
        config.data_dir = Path(data["data_dir"])
    if "db_path" in data:
        config.db_path = Path(data["db_path"])
    if "max_retries" in data:
        config.max_retries = int(data["max_retries"])
    if "health_poll_interval" in data:
        config.health_poll_interval = int(data["health_poll_interval"])
    if "crash_poll_interval" in data:
        config.crash_poll_interval = int(data["crash_poll_interval"])
    if "default_cpu_quota" in data:
        config.default_cpu_quota = int(data["default_cpu_quota"])
    if "default_memory_mb" in data:
        config.default_memory_mb = int(data["default_memory_mb"])
    if "schedule_file" in data:
        config.schedule_file = Path(data["schedule_file"])
    if "host" in data:
        config.host = str(data["host"])
    if "port" in data:
        config.port = int(data["port"])
    if "metrics_port" in data:
        config.metrics_port = int(data["metrics_port"])

    return config

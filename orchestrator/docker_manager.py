"""Docker SDK integration for managing bitcoinfuzz containers.

Wraps the ``docker-py`` library to provide campaign-specific container
lifecycle operations without modifying the existing Dockerfile or
docker-compose.yml.
"""

import logging
from typing import Any, Generator, Optional

import docker
from docker.errors import APIError, DockerException, NotFound
from docker.models.containers import Container

from .models import Campaign, ResourceLimits

logger = logging.getLogger(__name__)


class DockerManager:
    """Manages Docker container operations for fuzzing campaigns.

    Uses the Docker Engine API via ``docker-py``.  The manager never
    modifies docker-compose.yml — it creates standalone containers that
    mirror the compose service definition.
    """

    def __init__(self, data_dir: str = "./docker") -> None:
        """
        Args:
            data_dir: Host path to the shared data directory that gets
                      bind-mounted to ``/app/data`` inside containers.
        """
        try:
            self._client = docker.from_env()
            self._client.ping()
            logger.info("Connected to Docker daemon")
        except DockerException as exc:
            logger.error("Failed to connect to Docker: %s", exc)
            raise
        self._data_dir = data_dir

    # -- container lifecycle -------------------------------------------------

    def launch_container(
        self,
        campaign: Campaign,
        cxxflags: str = "",
        fuzz_target: str = "",
    ) -> str:
        """Create and start a container for the given campaign.

        Returns the Docker container ID.
        """
        target = fuzz_target or campaign.target_name
        image_tag = campaign.image_tag or f"bitcoinfuzz:{target}"

        environment = {
            "FUZZ": target,
            "MODULES": campaign.modules or "",
        }
        # Merge target-specific env overrides (e.g. LIBFUZZ_DETECT_LEAKS=0)
        environment.update(campaign.env_overrides)

        container_name = f"bitcoinfuzz-{campaign.id}-{target}"

        # Resource constraints
        limits = campaign.resource_limits
        kwargs: dict[str, Any] = {
            "image": image_tag,
            "name": container_name,
            "environment": environment,
            "volumes": {
                self._data_dir: {"bind": "/app/data", "mode": "rw"},
            },
            "detach": True,
            "remove": False,  # Keep for log inspection after exit
            "network_disabled": True,  # Security: no network access
        }

        if limits.cpu_quota > 0:
            kwargs["cpu_quota"] = limits.cpu_quota
            kwargs["cpu_period"] = 100_000  # Default 100ms period
        if limits.memory_mb > 0:
            kwargs["mem_limit"] = f"{limits.memory_mb}m"

        try:
            container = self._client.containers.run(**kwargs)
            logger.info(
                "Launched container %s for campaign %s (target=%s)",
                container.short_id,
                campaign.id,
                target,
            )
            return container.id
        except APIError as exc:
            logger.error(
                "Failed to launch container for campaign %s: %s",
                campaign.id,
                exc,
            )
            raise

    def stop_container(
        self, container_id: str, timeout: int = 30
    ) -> None:
        """Gracefully stop a container."""
        try:
            container = self._client.containers.get(container_id)
            container.stop(timeout=timeout)
            logger.info("Stopped container %s", container_id[:12])
        except NotFound:
            logger.warning("Container %s not found (already removed?)", container_id[:12])
        except APIError as exc:
            logger.error("Error stopping container %s: %s", container_id[:12], exc)
            raise

    def kill_container(self, container_id: str) -> None:
        """Force-kill a container (SIGKILL)."""
        try:
            container = self._client.containers.get(container_id)
            container.kill()
            logger.info("Killed container %s", container_id[:12])
        except NotFound:
            logger.warning("Container %s not found", container_id[:12])
        except APIError as exc:
            logger.error("Error killing container %s: %s", container_id[:12], exc)

    def pause_container(self, container_id: str) -> None:
        """Pause a running container (SIGSTOP)."""
        try:
            container = self._client.containers.get(container_id)
            container.pause()
            logger.info("Paused container %s", container_id[:12])
        except (NotFound, APIError) as exc:
            logger.error("Error pausing container %s: %s", container_id[:12], exc)
            raise

    def resume_container(self, container_id: str) -> None:
        """Unpause a paused container (SIGCONT)."""
        try:
            container = self._client.containers.get(container_id)
            container.unpause()
            logger.info("Resumed container %s", container_id[:12])
        except (NotFound, APIError) as exc:
            logger.error("Error resuming container %s: %s", container_id[:12], exc)
            raise

    def remove_container(self, container_id: str, force: bool = True) -> None:
        """Remove a container."""
        try:
            container = self._client.containers.get(container_id)
            container.remove(force=force)
            logger.info("Removed container %s", container_id[:12])
        except NotFound:
            pass  # Already gone
        except APIError as exc:
            logger.error("Error removing container %s: %s", container_id[:12], exc)

    # -- status queries ------------------------------------------------------

    def get_container_status(self, container_id: str) -> Optional[dict]:
        """Return container status info or None if not found.

        Returns a dict with keys:
        - ``status``: 'running', 'exited', 'paused', 'created', etc.
        - ``exit_code``: int (only meaningful when status is 'exited')
        - ``oom_killed``: bool
        - ``running``: bool convenience flag
        """
        try:
            container = self._client.containers.get(container_id)
            container.reload()
            state = container.attrs.get("State", {})
            return {
                "status": state.get("Status", "unknown"),
                "exit_code": state.get("ExitCode", -1),
                "oom_killed": state.get("OOMKilled", False),
                "running": state.get("Running", False),
                "paused": state.get("Paused", False),
            }
        except NotFound:
            return None
        except APIError as exc:
            logger.error("Error inspecting container %s: %s", container_id[:12], exc)
            return None

    def get_container_logs(
        self,
        container_id: str,
        follow: bool = False,
        tail: int = 100,
    ) -> Generator[str, None, None]:
        """Yield log lines from a container.

        If *follow* is True, this is a blocking generator that streams
        logs as they appear.
        """
        try:
            container = self._client.containers.get(container_id)
            stream = container.logs(
                follow=follow,
                stream=True,
                tail=tail,
                timestamps=False,
            )
            for chunk in stream:
                line = chunk.decode("utf-8", errors="replace").rstrip("\n")
                if line:
                    yield line
        except NotFound:
            logger.warning("Container %s not found for log streaming", container_id[:12])
        except APIError as exc:
            logger.error("Error reading logs from %s: %s", container_id[:12], exc)

    def get_container_stats(self, container_id: str) -> Optional[dict]:
        """Return a single stats snapshot (CPU, memory, etc.)."""
        try:
            container = self._client.containers.get(container_id)
            return container.stats(stream=False)
        except (NotFound, APIError):
            return None

    # -- discovery -----------------------------------------------------------

    def list_bitcoinfuzz_containers(self) -> list[dict]:
        """Discover existing bitcoinfuzz containers for reattachment.

        Returns a list of dicts with container_id, name, status, and
        labels for each container whose name starts with ``bitcoinfuzz-``.
        """
        result = []
        try:
            containers = self._client.containers.list(
                all=True,
                filters={"name": "bitcoinfuzz-"},
            )
            for c in containers:
                c.reload()
                state = c.attrs.get("State", {})
                result.append(
                    {
                        "container_id": c.id,
                        "name": c.name,
                        "status": state.get("Status", "unknown"),
                        "running": state.get("Running", False),
                        "image": c.image.tags[0] if c.image.tags else "",
                    }
                )
        except APIError as exc:
            logger.error("Error listing containers: %s", exc)
        return result

    def image_exists(self, image_tag: str) -> bool:
        """Check if a Docker image exists locally."""
        try:
            self._client.images.get(image_tag)
            return True
        except (NotFound, APIError):
            return False

"""Tests for orchestrator/docker_manager.py.

All Docker SDK calls are mocked so no Docker daemon is required.
"""

from unittest.mock import MagicMock, patch, PropertyMock

import pytest
from docker.errors import APIError, NotFound

from orchestrator.docker_manager import DockerManager
from orchestrator.models import Campaign, CampaignState, ResourceLimits


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _make_campaign(
    target: str = "script",
    cpu_quota: int = 200_000,
    memory_mb: int = 2048,
    modules: str = "",
    env_overrides: dict | None = None,
) -> Campaign:
    """Return a Campaign in STARTING state with sensible defaults."""
    c = Campaign(target_name=target)
    c.resource_limits = ResourceLimits(cpu_quota=cpu_quota, memory_mb=memory_mb)
    c.modules = modules
    c.env_overrides = env_overrides or {}
    c.transition_to(CampaignState.STARTING)
    return c


def _make_docker_manager() -> tuple[DockerManager, MagicMock]:
    """Return a DockerManager backed by a mocked docker client."""
    with patch("orchestrator.docker_manager.docker.from_env") as mock_from_env:
        mock_client = MagicMock()
        mock_client.ping.return_value = True
        mock_from_env.return_value = mock_client
        mgr = DockerManager(data_dir="/tmp/fuzz-data")
    return mgr, mock_client


# ---------------------------------------------------------------------------
# Initialisation
# ---------------------------------------------------------------------------


class TestDockerManagerInit:
    def test_connects_on_init(self):
        with patch("orchestrator.docker_manager.docker.from_env") as mock_from_env:
            mock_client = MagicMock()
            mock_client.ping.return_value = True
            mock_from_env.return_value = mock_client
            mgr = DockerManager(data_dir="./docker")
        assert mgr is not None
        mock_client.ping.assert_called_once()

    def test_raises_on_docker_unavailable(self):
        from docker.errors import DockerException

        with patch("orchestrator.docker_manager.docker.from_env") as mock_from_env:
            mock_client = MagicMock()
            mock_client.ping.side_effect = DockerException("daemon not running")
            mock_from_env.return_value = mock_client
            with pytest.raises(DockerException):
                DockerManager(data_dir="./docker")


# ---------------------------------------------------------------------------
# launch_container
# ---------------------------------------------------------------------------


class TestLaunchContainer:
    def test_launch_returns_container_id(self):
        mgr, mock_client = _make_docker_manager()
        campaign = _make_campaign()

        mock_container = MagicMock()
        mock_container.id = "abc123def456" * 2  # 24-char fake ID
        mock_client.containers.run.return_value = mock_container

        cid = mgr.launch_container(campaign, fuzz_target="script")

        assert cid == mock_container.id
        mock_client.containers.run.assert_called_once()

    def test_launch_sets_fuzz_env(self):
        mgr, mock_client = _make_docker_manager()
        campaign = _make_campaign(target="deserialize_block")
        mock_container = MagicMock()
        mock_client.containers.run.return_value = mock_container

        mgr.launch_container(campaign)

        call_kwargs = mock_client.containers.run.call_args.kwargs
        assert call_kwargs["environment"]["FUZZ"] == "deserialize_block"

    def test_launch_applies_resource_limits(self):
        mgr, mock_client = _make_docker_manager()
        campaign = _make_campaign(cpu_quota=100_000, memory_mb=1024)
        mock_container = MagicMock()
        mock_client.containers.run.return_value = mock_container

        mgr.launch_container(campaign)

        call_kwargs = mock_client.containers.run.call_args.kwargs
        assert call_kwargs["cpu_quota"] == 100_000
        assert call_kwargs["mem_limit"] == "1024m"

    def test_launch_disables_network(self):
        mgr, mock_client = _make_docker_manager()
        campaign = _make_campaign()
        mock_container = MagicMock()
        mock_client.containers.run.return_value = mock_container

        mgr.launch_container(campaign)

        call_kwargs = mock_client.containers.run.call_args.kwargs
        assert call_kwargs["network_disabled"] is True

    def test_launch_merges_env_overrides(self):
        mgr, mock_client = _make_docker_manager()
        campaign = _make_campaign(env_overrides={"LIBFUZZ_DETECT_LEAKS": "0"})
        mock_container = MagicMock()
        mock_client.containers.run.return_value = mock_container

        mgr.launch_container(campaign)

        call_kwargs = mock_client.containers.run.call_args.kwargs
        assert call_kwargs["environment"]["LIBFUZZ_DETECT_LEAKS"] == "0"

    def test_launch_raises_on_api_error(self):
        mgr, mock_client = _make_docker_manager()
        campaign = _make_campaign()
        mock_client.containers.run.side_effect = APIError("image not found")

        with pytest.raises(APIError):
            mgr.launch_container(campaign)

    def test_launch_with_zero_limits_omits_constraints(self):
        """When cpu_quota=0 / memory_mb=0, no resource limit args are passed."""
        mgr, mock_client = _make_docker_manager()
        campaign = _make_campaign(cpu_quota=0, memory_mb=0)
        mock_container = MagicMock()
        mock_client.containers.run.return_value = mock_container

        mgr.launch_container(campaign)

        call_kwargs = mock_client.containers.run.call_args.kwargs
        assert "cpu_quota" not in call_kwargs
        assert "mem_limit" not in call_kwargs


# ---------------------------------------------------------------------------
# stop_container
# ---------------------------------------------------------------------------


class TestStopContainer:
    def test_stop_calls_container_stop(self):
        mgr, mock_client = _make_docker_manager()
        mock_container = MagicMock()
        mock_client.containers.get.return_value = mock_container

        mgr.stop_container("abc123")

        mock_container.stop.assert_called_once_with(timeout=30)

    def test_stop_swallows_not_found(self):
        """Already-removed containers should not raise."""
        mgr, mock_client = _make_docker_manager()
        mock_client.containers.get.side_effect = NotFound("gone")

        # Should not raise
        mgr.stop_container("abc123")

    def test_stop_custom_timeout(self):
        mgr, mock_client = _make_docker_manager()
        mock_container = MagicMock()
        mock_client.containers.get.return_value = mock_container

        mgr.stop_container("abc123", timeout=60)

        mock_container.stop.assert_called_once_with(timeout=60)


# ---------------------------------------------------------------------------
# kill_container
# ---------------------------------------------------------------------------


class TestKillContainer:
    def test_kill_calls_container_kill(self):
        mgr, mock_client = _make_docker_manager()
        mock_container = MagicMock()
        mock_client.containers.get.return_value = mock_container

        mgr.kill_container("abc123")

        mock_container.kill.assert_called_once()

    def test_kill_swallows_not_found(self):
        mgr, mock_client = _make_docker_manager()
        mock_client.containers.get.side_effect = NotFound("gone")

        # Should not raise
        mgr.kill_container("abc123")


# ---------------------------------------------------------------------------
# pause_container / resume_container
# ---------------------------------------------------------------------------


class TestPauseResume:
    def test_pause_calls_container_pause(self):
        mgr, mock_client = _make_docker_manager()
        mock_container = MagicMock()
        mock_client.containers.get.return_value = mock_container

        mgr.pause_container("abc123")

        mock_container.pause.assert_called_once()

    def test_resume_calls_container_unpause(self):
        mgr, mock_client = _make_docker_manager()
        mock_container = MagicMock()
        mock_client.containers.get.return_value = mock_container

        mgr.resume_container("abc123")

        mock_container.unpause.assert_called_once()

    def test_pause_raises_on_api_error(self):
        mgr, mock_client = _make_docker_manager()
        mock_container = MagicMock()
        mock_container.pause.side_effect = APIError("not running")
        mock_client.containers.get.return_value = mock_container

        with pytest.raises(APIError):
            mgr.pause_container("abc123")


# ---------------------------------------------------------------------------
# get_container_status
# ---------------------------------------------------------------------------


class TestGetContainerStatus:
    def test_running_container(self):
        mgr, mock_client = _make_docker_manager()
        mock_container = MagicMock()
        mock_container.attrs = {
            "State": {
                "Status": "running",
                "Running": True,
                "Paused": False,
                "ExitCode": 0,
                "OOMKilled": False,
            }
        }
        mock_client.containers.get.return_value = mock_container

        status = mgr.get_container_status("abc123")

        assert status is not None
        assert status["status"] == "running"
        assert status["running"] is True
        assert status["oom_killed"] is False

    def test_exited_container(self):
        mgr, mock_client = _make_docker_manager()
        mock_container = MagicMock()
        mock_container.attrs = {
            "State": {
                "Status": "exited",
                "Running": False,
                "Paused": False,
                "ExitCode": 1,
                "OOMKilled": False,
            }
        }
        mock_client.containers.get.return_value = mock_container

        status = mgr.get_container_status("abc123")

        assert status["status"] == "exited"
        assert status["running"] is False
        assert status["exit_code"] == 1

    def test_oom_killed_container(self):
        mgr, mock_client = _make_docker_manager()
        mock_container = MagicMock()
        mock_container.attrs = {
            "State": {
                "Status": "exited",
                "Running": False,
                "Paused": False,
                "ExitCode": 137,
                "OOMKilled": True,
            }
        }
        mock_client.containers.get.return_value = mock_container

        status = mgr.get_container_status("abc123")

        assert status["oom_killed"] is True

    def test_returns_none_when_not_found(self):
        mgr, mock_client = _make_docker_manager()
        mock_client.containers.get.side_effect = NotFound("gone")

        status = mgr.get_container_status("abc123")

        assert status is None


# ---------------------------------------------------------------------------
# get_container_logs
# ---------------------------------------------------------------------------


class TestGetContainerLogs:
    def test_yields_log_lines(self):
        mgr, mock_client = _make_docker_manager()
        mock_container = MagicMock()
        mock_container.logs.return_value = [
            b"#100 pulse  cov: 5 ft: 10 corp: 2/100b exec/s: 100 rss: 50Mb\n",
            b"INFO: Seed: 3918206239\n",
        ]
        mock_client.containers.get.return_value = mock_container

        lines = list(mgr.get_container_logs("abc123"))

        assert len(lines) == 2
        assert "pulse" in lines[0]

    def test_handles_not_found_gracefully(self):
        mgr, mock_client = _make_docker_manager()
        mock_client.containers.get.side_effect = NotFound("gone")

        lines = list(mgr.get_container_logs("abc123"))

        assert lines == []


# ---------------------------------------------------------------------------
# list_bitcoinfuzz_containers (reattachment)
# ---------------------------------------------------------------------------


class TestListBitcoinfuzzContainers:
    def test_returns_running_containers(self):
        mgr, mock_client = _make_docker_manager()

        mock_c = MagicMock()
        mock_c.id = "abc123" * 4
        mock_c.name = "bitcoinfuzz-run1-script"
        mock_c.attrs = {"State": {"Status": "running", "Running": True}}
        mock_c.image.tags = ["bitcoinfuzz:script"]
        mock_client.containers.list.return_value = [mock_c]

        result = mgr.list_bitcoinfuzz_containers()

        assert len(result) == 1
        assert result[0]["name"] == "bitcoinfuzz-run1-script"
        assert result[0]["running"] is True

    def test_returns_empty_list_on_no_containers(self):
        mgr, mock_client = _make_docker_manager()
        mock_client.containers.list.return_value = []

        result = mgr.list_bitcoinfuzz_containers()

        assert result == []

    def test_returns_empty_on_api_error(self):
        mgr, mock_client = _make_docker_manager()
        mock_client.containers.list.side_effect = APIError("daemon error")

        result = mgr.list_bitcoinfuzz_containers()

        assert result == []


# ---------------------------------------------------------------------------
# image_exists
# ---------------------------------------------------------------------------


class TestImageExists:
    def test_returns_true_when_image_found(self):
        mgr, mock_client = _make_docker_manager()
        mock_client.images.get.return_value = MagicMock()

        assert mgr.image_exists("bitcoinfuzz:script") is True

    def test_returns_false_when_not_found(self):
        mgr, mock_client = _make_docker_manager()
        mock_client.images.get.side_effect = NotFound("no such image")

        assert mgr.image_exists("bitcoinfuzz:nonexistent") is False

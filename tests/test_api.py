"""Tests for the FastAPI REST API endpoints.

Uses FastAPI's TestClient for synchronous testing without Docker.
"""

import pytest
from fastapi.testclient import TestClient

from orchestrator.app import build_app


@pytest.fixture
def client():
    """Create a test client.

    The app initialises without Docker (API-only mode), so campaign
    lifecycle endpoints will return 503, but target and health
    endpoints work normally.
    """
    app = build_app()
    with TestClient(app) as c:
        yield c


class TestHealthEndpoint:
    def test_health_returns_ok(self, client):
        resp = client.get("/health")
        assert resp.status_code == 200
        data = resp.json()
        assert data["status"] == "ok"
        assert "version" in data
        assert "uptime_seconds" in data

    def test_health_has_campaign_count(self, client):
        resp = client.get("/health")
        data = resp.json()
        assert "active_campaigns" in data


class TestTargetsEndpoints:
    def test_list_targets(self, client):
        resp = client.get("/targets")
        assert resp.status_code == 200
        targets = resp.json()
        assert isinstance(targets, list)
        assert len(targets) > 0

    def test_target_has_required_fields(self, client):
        resp = client.get("/targets")
        targets = resp.json()
        for target in targets:
            assert "name" in target
            assert "cxxflags" in target
            assert "fuzz" in target

    def test_list_targets_includes_known_target(self, client):
        resp = client.get("/targets")
        names = [t["name"] for t in resp.json()]
        assert "script" in names

    def test_get_specific_target(self, client):
        resp = client.get("/targets/script")
        assert resp.status_code == 200
        data = resp.json()
        assert data["name"] == "script"
        assert "BITCOIN_CORE" in data["cxxflags"]

    def test_get_nonexistent_target(self, client):
        resp = client.get("/targets/nonexistent_target_xyz")
        assert resp.status_code == 404

    def test_target_has_resource_limits(self, client):
        resp = client.get("/targets/script")
        data = resp.json()
        assert "resource_limits" in data
        assert "cpu_quota" in data["resource_limits"]
        assert "memory_mb" in data["resource_limits"]


class TestCampaignEndpoints:
    """Campaign endpoints — test error handling and basic flows."""

    def test_create_campaign_returns_campaign(self, client):
        resp = client.post(
            "/campaigns",
            json={"target_name": "script"},
        )
        # Campaign is created (201) even if container launch fails
        # (it transitions to FAILED state internally)
        assert resp.status_code in (201, 503)
        if resp.status_code == 201:
            data = resp.json()
            assert data["target_name"] == "script"

    def test_create_campaign_invalid_target(self, client):
        resp = client.post(
            "/campaigns",
            json={"target_name": "nonexistent_target_xyz"},
        )
        assert resp.status_code in (400, 503)

    def test_list_campaigns(self, client):
        resp = client.get("/campaigns")
        assert resp.status_code == 200
        assert isinstance(resp.json(), list)

    def test_get_nonexistent_campaign(self, client):
        resp = client.get("/campaigns/nonexistent123")
        assert resp.status_code == 404

    def test_delete_nonexistent_campaign(self, client):
        resp = client.delete("/campaigns/nonexistent123")
        # 404 (not found) or 503 (no docker)
        assert resp.status_code in (404, 503)

    def test_pause_nonexistent_campaign(self, client):
        resp = client.post("/campaigns/nonexistent123/pause")
        assert resp.status_code in (404, 503)

    def test_resume_nonexistent_campaign(self, client):
        resp = client.post("/campaigns/nonexistent123/resume")
        assert resp.status_code in (404, 503)


class TestDashboardEndpoints:
    def test_dashboard_summary(self, client):
        resp = client.get("/dashboard/summary")
        assert resp.status_code == 200
        data = resp.json()
        assert "active_campaigns" in data
        assert "total_targets" in data
        assert data["total_targets"] > 0

    def test_resource_utilization(self, client):
        resp = client.get("/dashboard/resources")
        assert resp.status_code == 200


class TestAlertWebhook:
    def test_alert_webhook(self, client):
        payload = {
            "alerts": [
                {
                    "status": "firing",
                    "labels": {"alertname": "CampaignCrashed", "target": "script"},
                    "annotations": {"summary": "Campaign crashed"},
                }
            ]
        }
        resp = client.post("/api/alerts/webhook", json=payload)
        assert resp.status_code == 200
        assert resp.json()["count"] == 1

    def test_alert_webhook_empty(self, client):
        resp = client.post("/api/alerts/webhook", json={"alerts": []})
        assert resp.status_code == 200
        assert resp.json()["count"] == 0


class TestCIHook:
    def test_ci_hook(self, client):
        payload = {"commit": "abc123", "trigger": "push"}
        resp = client.post("/api/ci-hook", json=payload)
        assert resp.status_code == 200
        data = resp.json()
        assert data["commit"] == "abc123"


class TestSchedulesEndpoint:
    def test_list_schedules_empty(self, client):
        resp = client.get("/schedules")
        assert resp.status_code == 200
        assert isinstance(resp.json(), list)


class TestHistoryEndpoints:
    def test_target_history_unknown_target(self, client):
        resp = client.get("/targets/nonexistent_xyz/history")
        assert resp.status_code == 404

    def test_target_history_empty(self, client):
        resp = client.get("/targets/script/history")
        assert resp.status_code == 200
        assert isinstance(resp.json(), list)


class TestCORSHeaders:
    def test_cors_preflight(self, client):
        resp = client.options(
            "/health",
            headers={
                "Origin": "http://localhost:3000",
                "Access-Control-Request-Method": "GET",
            },
        )
        # CORS should allow any origin
        assert resp.status_code == 200

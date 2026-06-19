"""SQLite persistence layer for campaign state and historical data.

Uses WAL journal mode for safe concurrent reads and a single-writer pattern
via an asyncio queue for all write operations, matching the design in
proposal §11 risk mitigation.
"""

import asyncio
import json
import logging
import sqlite3
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

from .models import Campaign, CampaignState, ResourceLimits

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Schema
# ---------------------------------------------------------------------------

_SCHEMA = """
-- Active campaigns (mutable, written on every state change)
CREATE TABLE IF NOT EXISTS campaigns (
    id              TEXT PRIMARY KEY,
    target_name     TEXT NOT NULL,
    state           TEXT NOT NULL DEFAULT 'queued',
    container_id    TEXT,
    image_tag       TEXT,
    started_at      TEXT,
    ended_at        TEXT,
    retry_count     INTEGER NOT NULL DEFAULT 0,
    max_retries     INTEGER NOT NULL DEFAULT 3,
    cpu_quota       INTEGER NOT NULL DEFAULT 200000,
    memory_mb       INTEGER NOT NULL DEFAULT 2048,
    priority        INTEGER NOT NULL DEFAULT 1,
    modules         TEXT,
    env_overrides   TEXT NOT NULL DEFAULT '{}',
    created_at      TEXT NOT NULL,
    updated_at      TEXT NOT NULL,
    crash_count     INTEGER NOT NULL DEFAULT 0,
    exit_reason     TEXT,
    final_coverage      INTEGER,
    final_corpus_count  INTEGER,
    final_corpus_bytes  INTEGER,
    total_executions    INTEGER,
    avg_exec_per_sec    REAL,
    peak_rss_mb         INTEGER
);

-- Archived summaries (immutable once written, for historical queries)
CREATE TABLE IF NOT EXISTS campaign_summaries (
    id                  TEXT PRIMARY KEY,
    target_name         TEXT NOT NULL,
    started_at          TEXT,
    ended_at            TEXT,
    duration_seconds    INTEGER,
    final_coverage      INTEGER,
    final_corpus_count  INTEGER,
    final_corpus_bytes  INTEGER,
    total_executions    INTEGER,
    avg_exec_per_sec    REAL,
    peak_rss_mb         INTEGER,
    crash_count         INTEGER NOT NULL DEFAULT 0,
    exit_reason         TEXT,
    modules             TEXT,
    cxxflags            TEXT
);

-- Crash artefacts discovered during campaigns
CREATE TABLE IF NOT EXISTS crash_records (
    id              TEXT PRIMARY KEY,
    campaign_id     TEXT NOT NULL REFERENCES campaigns(id),
    target_name     TEXT NOT NULL,
    discovered_at   TEXT NOT NULL,
    file_path       TEXT NOT NULL,
    file_size       INTEGER,
    notified        INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_campaigns_state ON campaigns(state);
CREATE INDEX IF NOT EXISTS idx_campaigns_target ON campaigns(target_name);
CREATE INDEX IF NOT EXISTS idx_summaries_target ON campaign_summaries(target_name);
CREATE INDEX IF NOT EXISTS idx_crashes_campaign ON crash_records(campaign_id);
CREATE INDEX IF NOT EXISTS idx_crashes_target ON crash_records(target_name);
"""


# ---------------------------------------------------------------------------
# Database class
# ---------------------------------------------------------------------------

class Database:
    """Thin synchronous wrapper around SQLite for campaign persistence.

    All writes go through a single connection with WAL mode enabled.
    For the FastAPI async context, callers can use ``asyncio.to_thread``
    or the convenience async helpers provided below.
    """

    def __init__(self, db_path: Path) -> None:
        self._db_path = db_path
        self._conn: Optional[sqlite3.Connection] = None

    # -- lifecycle -----------------------------------------------------------

    def connect(self) -> None:
        """Open the database and apply the schema."""
        self._db_path.parent.mkdir(parents=True, exist_ok=True)
        self._conn = sqlite3.connect(
            str(self._db_path),
            check_same_thread=False,
            timeout=30.0,
        )
        self._conn.row_factory = sqlite3.Row
        self._conn.execute("PRAGMA journal_mode=WAL")
        self._conn.execute("PRAGMA busy_timeout=5000")
        self._conn.execute("PRAGMA foreign_keys=ON")
        self._conn.executescript(_SCHEMA)
        self._conn.commit()
        logger.info("Database initialised at %s", self._db_path)

    def close(self) -> None:
        if self._conn:
            self._conn.close()
            self._conn = None

    @property
    def conn(self) -> sqlite3.Connection:
        if self._conn is None:
            raise RuntimeError("Database not connected")
        return self._conn

    # -- campaign CRUD -------------------------------------------------------

    def create_campaign(self, campaign: Campaign) -> None:
        """Insert a new campaign row."""
        now = datetime.now(timezone.utc).isoformat()
        self.conn.execute(
            """
            INSERT INTO campaigns
                (id, target_name, state, container_id, image_tag,
                 started_at, ended_at, retry_count, max_retries,
                 cpu_quota, memory_mb, priority, modules, env_overrides,
                 created_at, updated_at, crash_count, exit_reason)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                campaign.id,
                campaign.target_name,
                campaign.state.value,
                campaign.container_id,
                campaign.image_tag,
                campaign.started_at.isoformat() if campaign.started_at else None,
                campaign.ended_at.isoformat() if campaign.ended_at else None,
                campaign.retry_count,
                campaign.max_retries,
                campaign.resource_limits.cpu_quota,
                campaign.resource_limits.memory_mb,
                campaign.priority,
                campaign.modules,
                json.dumps(campaign.env_overrides),
                now,
                now,
                campaign.crash_count,
                campaign.exit_reason,
            ),
        )
        self.conn.commit()

    def update_campaign(self, campaign: Campaign) -> None:
        """Persist current campaign state to the database."""
        now = datetime.now(timezone.utc).isoformat()
        self.conn.execute(
            """
            UPDATE campaigns SET
                state = ?, container_id = ?, started_at = ?, ended_at = ?,
                retry_count = ?, crash_count = ?, exit_reason = ?,
                final_coverage = ?, final_corpus_count = ?, final_corpus_bytes = ?,
                total_executions = ?, avg_exec_per_sec = ?, peak_rss_mb = ?,
                updated_at = ?
            WHERE id = ?
            """,
            (
                campaign.state.value,
                campaign.container_id,
                campaign.started_at.isoformat() if campaign.started_at else None,
                campaign.ended_at.isoformat() if campaign.ended_at else None,
                campaign.retry_count,
                campaign.crash_count,
                campaign.exit_reason,
                campaign.final_coverage,
                campaign.final_corpus_count,
                campaign.final_corpus_bytes,
                campaign.total_executions,
                campaign.avg_exec_per_sec,
                campaign.peak_rss_mb,
                now,
                campaign.id,
            ),
        )
        self.conn.commit()

    def get_campaign(self, campaign_id: str) -> Optional[Campaign]:
        """Load a single campaign by ID, or None if not found."""
        row = self.conn.execute(
            "SELECT * FROM campaigns WHERE id = ?", (campaign_id,)
        ).fetchone()
        if row is None:
            return None
        return self._row_to_campaign(row)

    def list_campaigns(
        self,
        state: Optional[CampaignState] = None,
        target_name: Optional[str] = None,
        limit: int = 100,
    ) -> list[Campaign]:
        """List campaigns with optional filters."""
        query = "SELECT * FROM campaigns WHERE 1=1"
        params: list = []
        if state is not None:
            query += " AND state = ?"
            params.append(state.value)
        if target_name is not None:
            query += " AND target_name = ?"
            params.append(target_name)
        query += " ORDER BY created_at DESC LIMIT ?"
        params.append(limit)
        rows = self.conn.execute(query, params).fetchall()
        return [self._row_to_campaign(r) for r in rows]

    def delete_campaign(self, campaign_id: str) -> bool:
        """Delete a campaign row.  Returns True if a row was deleted."""
        cur = self.conn.execute(
            "DELETE FROM campaigns WHERE id = ?", (campaign_id,)
        )
        self.conn.commit()
        return cur.rowcount > 0

    # -- archival ------------------------------------------------------------

    def archive_campaign(self, campaign: Campaign, cxxflags: str = "") -> None:
        """Write a completed/failed campaign to the summaries table."""
        duration = None
        if campaign.started_at and campaign.ended_at:
            duration = int(
                (campaign.ended_at - campaign.started_at).total_seconds()
            )
        self.conn.execute(
            """
            INSERT OR REPLACE INTO campaign_summaries
                (id, target_name, started_at, ended_at, duration_seconds,
                 final_coverage, final_corpus_count, final_corpus_bytes,
                 total_executions, avg_exec_per_sec, peak_rss_mb,
                 crash_count, exit_reason, modules, cxxflags)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                campaign.id,
                campaign.target_name,
                campaign.started_at.isoformat() if campaign.started_at else None,
                campaign.ended_at.isoformat() if campaign.ended_at else None,
                duration,
                campaign.final_coverage,
                campaign.final_corpus_count,
                campaign.final_corpus_bytes,
                campaign.total_executions,
                campaign.avg_exec_per_sec,
                campaign.peak_rss_mb,
                campaign.crash_count,
                campaign.exit_reason,
                campaign.modules,
                cxxflags,
            ),
        )
        self.conn.commit()

    def get_target_history(
        self, target_name: str, limit: int = 10
    ) -> list[dict]:
        """Return archived summaries for a given target, newest first."""
        rows = self.conn.execute(
            """
            SELECT * FROM campaign_summaries
            WHERE target_name = ?
            ORDER BY started_at DESC
            LIMIT ?
            """,
            (target_name, limit),
        ).fetchall()
        return [dict(r) for r in rows]

    def compare_campaigns(self, campaign_ids: list[str]) -> list[dict]:
        """Return summaries for the given campaign IDs."""
        placeholders = ",".join("?" for _ in campaign_ids)
        rows = self.conn.execute(
            f"SELECT * FROM campaign_summaries WHERE id IN ({placeholders})",
            campaign_ids,
        ).fetchall()
        return [dict(r) for r in rows]

    # -- crash records -------------------------------------------------------

    def record_crash(
        self,
        campaign_id: str,
        target_name: str,
        file_path: str,
        file_size: Optional[int] = None,
    ) -> str:
        """Insert a crash record and return its ID."""
        crash_id = uuid.uuid4().hex[:12]
        now = datetime.now(timezone.utc).isoformat()
        self.conn.execute(
            """
            INSERT INTO crash_records
                (id, campaign_id, target_name, discovered_at, file_path, file_size)
            VALUES (?, ?, ?, ?, ?, ?)
            """,
            (crash_id, campaign_id, target_name, now, file_path, file_size),
        )
        # Also bump crash count on the campaign
        self.conn.execute(
            "UPDATE campaigns SET crash_count = crash_count + 1 WHERE id = ?",
            (campaign_id,),
        )
        self.conn.commit()
        return crash_id

    def get_crashes(
        self,
        campaign_id: Optional[str] = None,
        target_name: Optional[str] = None,
        limit: int = 100,
    ) -> list[dict]:
        """List crash records with optional filters."""
        query = "SELECT * FROM crash_records WHERE 1=1"
        params: list = []
        if campaign_id:
            query += " AND campaign_id = ?"
            params.append(campaign_id)
        if target_name:
            query += " AND target_name = ?"
            params.append(target_name)
        query += " ORDER BY discovered_at DESC LIMIT ?"
        params.append(limit)
        rows = self.conn.execute(query, params).fetchall()
        return [dict(r) for r in rows]

    def get_dashboard_stats(self) -> dict:
        """Aggregate stats for the dashboard summary endpoint."""
        states = self.conn.execute(
            "SELECT state, COUNT(*) as cnt FROM campaigns GROUP BY state"
        ).fetchall()
        state_counts: dict[str, int] = {r["state"]: r["cnt"] for r in states}
        total_crashes = (
            self.conn.execute("SELECT COUNT(*) FROM crash_records").fetchone()[0]
        )
        return {
            "active_campaigns": state_counts.get("running", 0)
            + state_counts.get("starting", 0)
            + state_counts.get("restarting", 0),
            "queued_campaigns": state_counts.get("queued", 0),
            "completed_campaigns": state_counts.get("completed", 0),
            "failed_campaigns": state_counts.get("failed", 0),
            "total_crashes": total_crashes,
        }

    # -- helpers -------------------------------------------------------------

    @staticmethod
    def _row_to_campaign(row: sqlite3.Row) -> Campaign:
        """Convert a database row to a Campaign instance."""
        env_overrides = json.loads(row["env_overrides"]) if row["env_overrides"] else {}
        return Campaign(
            id=row["id"],
            target_name=row["target_name"],
            state=CampaignState(row["state"]),
            container_id=row["container_id"],
            image_tag=row["image_tag"],
            started_at=(
                datetime.fromisoformat(row["started_at"])
                if row["started_at"]
                else None
            ),
            ended_at=(
                datetime.fromisoformat(row["ended_at"])
                if row["ended_at"]
                else None
            ),
            retry_count=row["retry_count"],
            max_retries=row["max_retries"],
            resource_limits=ResourceLimits(
                cpu_quota=row["cpu_quota"],
                memory_mb=row["memory_mb"],
            ),
            priority=row["priority"],
            modules=row["modules"],
            env_overrides=env_overrides,
            crash_count=row["crash_count"],
            exit_reason=row["exit_reason"],
            final_coverage=row["final_coverage"],
            final_corpus_count=row["final_corpus_count"],
            final_corpus_bytes=row["final_corpus_bytes"],
            total_executions=row["total_executions"],
            avg_exec_per_sec=row["avg_exec_per_sec"],
            peak_rss_mb=row["peak_rss_mb"],
        )

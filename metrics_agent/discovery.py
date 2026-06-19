"""Prometheus file-based service discovery.

Generates ``targets.json`` files for Prometheus's ``file_sd_configs``
so that Prometheus dynamically discovers metrics endpoints as campaigns
start and stop.
"""

import json
import logging
from pathlib import Path
from typing import Optional

logger = logging.getLogger(__name__)


class ServiceDiscovery:
    """Manages Prometheus file-based service discovery.

    Writes a ``targets.json`` file that Prometheus reads via
    ``file_sd_configs``.  The file is updated whenever campaigns
    start or stop.

    Format::

        [
          {
            "targets": ["host:9091"],
            "labels": {
              "target": "script",
              "campaign_id": "abc123",
              "job": "bitcoinfuzz"
            }
          }
        ]
    """

    def __init__(
        self,
        output_path: Path,
        metrics_host: str = "localhost",
        metrics_port: int = 9091,
    ) -> None:
        self._output_path = output_path
        self._metrics_host = metrics_host
        self._metrics_port = metrics_port
        # campaign_id -> (target_name, port)
        self._entries: dict[str, tuple[str, int]] = {}

    def register(
        self,
        campaign_id: str,
        target_name: str,
        port: Optional[int] = None,
    ) -> None:
        """Add a campaign to the discovery file."""
        p = port or self._metrics_port
        self._entries[campaign_id] = (target_name, p)
        self._write()
        logger.info(
            "Registered service discovery for campaign %s (target=%s, port=%d)",
            campaign_id,
            target_name,
            p,
        )

    def unregister(self, campaign_id: str) -> None:
        """Remove a campaign from the discovery file."""
        if campaign_id in self._entries:
            del self._entries[campaign_id]
            self._write()
            logger.info(
                "Unregistered service discovery for campaign %s", campaign_id
            )

    def _write(self) -> None:
        """Write the current entries to the targets.json file."""
        targets = []
        for campaign_id, (target_name, port) in self._entries.items():
            targets.append(
                {
                    "targets": [f"{self._metrics_host}:{port}"],
                    "labels": {
                        "target": target_name,
                        "campaign_id": campaign_id,
                        "job": "bitcoinfuzz",
                    },
                }
            )

        self._output_path.parent.mkdir(parents=True, exist_ok=True)
        with self._output_path.open("w", encoding="utf-8") as f:
            json.dump(targets, f, indent=2)

        logger.debug(
            "Wrote %d entries to %s", len(targets), self._output_path
        )

    def get_entries(self) -> list[dict]:
        """Return the current service discovery entries."""
        return [
            {
                "campaign_id": cid,
                "target": target,
                "port": port,
            }
            for cid, (target, port) in self._entries.items()
        ]

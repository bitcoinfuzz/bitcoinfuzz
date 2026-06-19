"""Prometheus metrics collector.

Aggregates parsed libFuzzer stats and directory watcher data into
Prometheus gauges and counters, then exposes them on an HTTP ``/metrics``
endpoint.
"""

import logging
import threading
from typing import Optional

from prometheus_client import (
    Counter,
    Gauge,
    Info,
    CollectorRegistry,
    generate_latest,
    start_http_server,
)

logger = logging.getLogger(__name__)


class MetricsCollector:
    """Manages Prometheus metrics for one or more bitcoinfuzz campaigns.

    Each metric is labelled with ``target`` and ``campaign_id`` so that
    Prometheus can distinguish between concurrent campaigns.

    Exposed metrics match proposal §6.2::

        bitcoinfuzz_exec_per_second{target, campaign_id}
        bitcoinfuzz_coverage_edges{target, campaign_id}
        bitcoinfuzz_features{target, campaign_id}
        bitcoinfuzz_corpus_files_total{target, campaign_id}
        bitcoinfuzz_corpus_bytes_total{target, campaign_id}
        bitcoinfuzz_crash_total{target, campaign_id}
        bitcoinfuzz_container_status{target, campaign_id}
        bitcoinfuzz_rss_mb{target, campaign_id}
        bitcoinfuzz_iteration{target, campaign_id}
    """

    def __init__(self, registry: Optional[CollectorRegistry] = None) -> None:
        self._registry = registry or CollectorRegistry()
        self._labels = ["target", "campaign_id"]

        # libFuzzer stats
        self.exec_per_second = Gauge(
            "bitcoinfuzz_exec_per_second",
            "Fuzzer executions per second",
            self._labels,
            registry=self._registry,
        )
        self.coverage_edges = Gauge(
            "bitcoinfuzz_coverage_edges",
            "Total libFuzzer coverage edges discovered",
            self._labels,
            registry=self._registry,
        )
        self.features = Gauge(
            "bitcoinfuzz_features",
            "Total libFuzzer feature count",
            self._labels,
            registry=self._registry,
        )
        self.iteration = Gauge(
            "bitcoinfuzz_iteration",
            "Current libFuzzer iteration number",
            self._labels,
            registry=self._registry,
        )
        self.rss_mb = Gauge(
            "bitcoinfuzz_rss_mb",
            "Resident set size of the fuzzer process in MB",
            self._labels,
            registry=self._registry,
        )

        # Corpus stats
        self.corpus_files_total = Gauge(
            "bitcoinfuzz_corpus_files_total",
            "Number of files in the fuzzing corpus",
            self._labels,
            registry=self._registry,
        )
        self.corpus_bytes_total = Gauge(
            "bitcoinfuzz_corpus_bytes_total",
            "Total size of the fuzzing corpus in bytes",
            self._labels,
            registry=self._registry,
        )

        # Crash counter (monotonic)
        self.crash_total = Gauge(
            "bitcoinfuzz_crash_total",
            "Total number of crash artefacts found",
            self._labels,
            registry=self._registry,
        )

        # Container status
        self.container_status = Gauge(
            "bitcoinfuzz_container_status",
            "Container health (1=running, 0=stopped/crashed)",
            self._labels,
            registry=self._registry,
        )

    def update_from_fuzz_stats(
        self,
        target: str,
        campaign_id: str,
        iteration: int,
        coverage: int,
        features: int,
        corpus_count: int,
        corpus_bytes: int,
        exec_per_sec: int,
        rss_mb: int,
    ) -> None:
        """Update all libFuzzer-derived metrics at once."""
        labels = {"target": target, "campaign_id": campaign_id}
        self.exec_per_second.labels(**labels).set(exec_per_sec)
        self.coverage_edges.labels(**labels).set(coverage)
        self.features.labels(**labels).set(features)
        self.iteration.labels(**labels).set(iteration)
        self.corpus_files_total.labels(**labels).set(corpus_count)
        self.corpus_bytes_total.labels(**labels).set(corpus_bytes)
        self.rss_mb.labels(**labels).set(rss_mb)

    def update_corpus_stats(
        self,
        target: str,
        campaign_id: str,
        file_count: int,
        total_bytes: int,
    ) -> None:
        """Update corpus directory watcher metrics."""
        labels = {"target": target, "campaign_id": campaign_id}
        self.corpus_files_total.labels(**labels).set(file_count)
        self.corpus_bytes_total.labels(**labels).set(total_bytes)

    def update_crash_count(
        self, target: str, campaign_id: str, count: int
    ) -> None:
        """Update crash count from directory watcher."""
        self.crash_total.labels(target=target, campaign_id=campaign_id).set(count)

    def set_container_status(
        self, target: str, campaign_id: str, running: bool
    ) -> None:
        """Set container health status."""
        self.container_status.labels(
            target=target, campaign_id=campaign_id
        ).set(1 if running else 0)

    def remove_campaign(self, target: str, campaign_id: str) -> None:
        """Remove all metric label sets for a completed campaign."""
        labels = {"target": target, "campaign_id": campaign_id}
        for metric in [
            self.exec_per_second,
            self.coverage_edges,
            self.features,
            self.iteration,
            self.corpus_files_total,
            self.corpus_bytes_total,
            self.crash_total,
            self.container_status,
            self.rss_mb,
        ]:
            try:
                metric.remove(*[labels["target"], labels["campaign_id"]])
            except KeyError:
                pass  # Label set didn't exist

    def generate_metrics(self) -> bytes:
        """Return Prometheus-formatted metrics text."""
        return generate_latest(self._registry)

    def start_server(self, port: int = 9091) -> None:
        """Start a background HTTP server on ``/metrics``.

        Uses prometheus_client's built-in threaded server.
        """
        start_http_server(port, registry=self._registry)
        logger.info("Metrics server started on port %d", port)

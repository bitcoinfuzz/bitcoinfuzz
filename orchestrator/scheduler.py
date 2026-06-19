"""Resource-aware scheduler for fuzzing campaigns.

Tracks CPU quota and memory allocation across all running campaigns,
prevents over-subscription, and maintains a priority queue for campaigns
waiting to be launched.
"""

import heapq
import logging
import os
from dataclasses import dataclass, field
from typing import Optional

import psutil

from .models import Campaign, ResourceLimits

logger = logging.getLogger(__name__)


@dataclass(order=True)
class _QueueEntry:
    """Priority queue entry — lower priority number = launched later.

    We negate priority so that higher priority values sort first
    (heapq is a min-heap).
    """

    neg_priority: int
    campaign_id: str = field(compare=False)


class ResourceScheduler:
    """Tracks host resource allocation and queues campaigns that cannot
    be launched immediately.

    The scheduler is conservative: it refuses to launch a campaign if the
    requested resources would exceed the host's capacity, even if the
    actual usage is lower than the allocated amount.
    """

    def __init__(
        self,
        total_cpu_quota: Optional[int] = None,
        total_memory_mb: Optional[int] = None,
    ) -> None:
        """
        Args:
            total_cpu_quota: Total CPU quota budget in microseconds per 100ms.
                             ``None`` means auto-detect from host CPU count.
                             e.g. 800_000 for an 8-core host.
            total_memory_mb: Total memory budget in megabytes.
                             ``None`` means auto-detect (80% of host RAM).
        """
        if total_cpu_quota is None:
            cpu_count = os.cpu_count() or 4
            total_cpu_quota = cpu_count * 100_000
        if total_memory_mb is None:
            total_mem_bytes = psutil.virtual_memory().total
            total_memory_mb = int(total_mem_bytes / (1024 * 1024) * 0.8)

        self.total_cpu_quota = total_cpu_quota
        self.total_memory_mb = total_memory_mb
        self.allocated_cpu: int = 0
        self.allocated_memory: int = 0

        # Priority queue of campaign IDs waiting to launch
        self._queue: list[_QueueEntry] = []
        # Track what's allocated per campaign for deallocation
        self._allocations: dict[str, ResourceLimits] = {}

        logger.info(
            "Scheduler initialised: CPU budget=%d (%.1f cores), memory budget=%d MB",
            self.total_cpu_quota,
            self.total_cpu_quota / 100_000,
            self.total_memory_mb,
        )

    # -- resource checks -----------------------------------------------------

    @property
    def remaining_cpu(self) -> int:
        return self.total_cpu_quota - self.allocated_cpu

    @property
    def remaining_memory(self) -> int:
        return self.total_memory_mb - self.allocated_memory

    def can_launch(self, limits: ResourceLimits) -> bool:
        """Return True if there are enough free resources for the given limits."""
        cpu_ok = limits.cpu_quota == 0 or limits.cpu_quota <= self.remaining_cpu
        mem_ok = limits.memory_mb == 0 or limits.memory_mb <= self.remaining_memory
        return cpu_ok and mem_ok

    # -- allocation tracking -------------------------------------------------

    def allocate(self, campaign_id: str, limits: ResourceLimits) -> None:
        """Reserve resources for a campaign."""
        if campaign_id in self._allocations:
            logger.warning("Campaign %s already has an allocation", campaign_id)
            return
        self.allocated_cpu += limits.cpu_quota
        self.allocated_memory += limits.memory_mb
        self._allocations[campaign_id] = limits
        logger.debug(
            "Allocated resources for %s: CPU=%d, Mem=%dMB (total: CPU=%d/%d, Mem=%d/%d)",
            campaign_id,
            limits.cpu_quota,
            limits.memory_mb,
            self.allocated_cpu,
            self.total_cpu_quota,
            self.allocated_memory,
            self.total_memory_mb,
        )

    def deallocate(self, campaign_id: str) -> None:
        """Release resources for a completed/failed campaign."""
        limits = self._allocations.pop(campaign_id, None)
        if limits is None:
            return
        self.allocated_cpu = max(0, self.allocated_cpu - limits.cpu_quota)
        self.allocated_memory = max(0, self.allocated_memory - limits.memory_mb)
        logger.debug(
            "Deallocated resources for %s (remaining: CPU=%d, Mem=%dMB)",
            campaign_id,
            self.remaining_cpu,
            self.remaining_memory,
        )

    # -- queue management ----------------------------------------------------

    def enqueue(self, campaign_id: str, priority: int = 1) -> None:
        """Add a campaign to the waiting queue."""
        heapq.heappush(self._queue, _QueueEntry(-priority, campaign_id))
        logger.info(
            "Enqueued campaign %s (priority=%d, queue depth=%d)",
            campaign_id,
            priority,
            len(self._queue),
        )

    def dequeue_eligible(self) -> list[str]:
        """Return campaign IDs from the queue that can now be launched.

        This is called after resources are freed (e.g. a campaign completes).
        It pops entries from the front of the priority queue as long as
        resources are available.

        Note: The caller must know each campaign's resource requirements.
        This method cannot check them because it only stores IDs.
        The returned IDs should be checked against ``can_launch`` by the
        campaign manager before actually launching.
        """
        eligible: list[str] = []
        # We can't check resource requirements here (only IDs in queue),
        # so we return candidates and let the caller verify.
        remaining = []
        while self._queue:
            entry = heapq.heappop(self._queue)
            eligible.append(entry.campaign_id)
        # Caller will re-enqueue any that still can't launch
        return eligible

    def remove_from_queue(self, campaign_id: str) -> bool:
        """Remove a specific campaign from the queue.  Returns True if found."""
        original_len = len(self._queue)
        self._queue = [e for e in self._queue if e.campaign_id != campaign_id]
        heapq.heapify(self._queue)
        return len(self._queue) < original_len

    @property
    def queue_depth(self) -> int:
        return len(self._queue)

    # -- summary -------------------------------------------------------------

    def get_resource_summary(self) -> dict:
        """Return a summary dict suitable for the dashboard."""
        return {
            "cpu_allocated": self.allocated_cpu,
            "cpu_total": self.total_cpu_quota,
            "cpu_remaining": self.remaining_cpu,
            "cpu_utilization_pct": round(
                self.allocated_cpu / max(self.total_cpu_quota, 1) * 100, 1
            ),
            "memory_allocated_mb": self.allocated_memory,
            "memory_total_mb": self.total_memory_mb,
            "memory_remaining_mb": self.remaining_memory,
            "memory_utilization_pct": round(
                self.allocated_memory / max(self.total_memory_mb, 1) * 100, 1
            ),
            "active_allocations": len(self._allocations),
            "queue_depth": self.queue_depth,
        }

"""Tests for the resource-aware scheduler."""

import pytest

from orchestrator.models import ResourceLimits
from orchestrator.scheduler import ResourceScheduler


class TestResourceScheduler:
    """Test resource allocation, deallocation, and queueing."""

    def test_initial_state(self):
        sched = ResourceScheduler(
            total_cpu_quota=800_000, total_memory_mb=16384
        )
        assert sched.remaining_cpu == 800_000
        assert sched.remaining_memory == 16384
        assert sched.queue_depth == 0

    def test_can_launch_within_limits(self):
        sched = ResourceScheduler(
            total_cpu_quota=800_000, total_memory_mb=16384
        )
        limits = ResourceLimits(cpu_quota=200_000, memory_mb=2048)
        assert sched.can_launch(limits) is True

    def test_cannot_launch_over_cpu(self):
        sched = ResourceScheduler(total_cpu_quota=100_000, total_memory_mb=16384)
        limits = ResourceLimits(cpu_quota=200_000, memory_mb=2048)
        assert sched.can_launch(limits) is False

    def test_cannot_launch_over_memory(self):
        sched = ResourceScheduler(total_cpu_quota=800_000, total_memory_mb=1024)
        limits = ResourceLimits(cpu_quota=200_000, memory_mb=2048)
        assert sched.can_launch(limits) is False

    def test_allocate_and_deallocate(self):
        sched = ResourceScheduler(
            total_cpu_quota=800_000, total_memory_mb=16384
        )
        limits = ResourceLimits(cpu_quota=200_000, memory_mb=2048)
        sched.allocate("campaign1", limits)
        assert sched.remaining_cpu == 600_000
        assert sched.remaining_memory == 14336

        sched.deallocate("campaign1")
        assert sched.remaining_cpu == 800_000
        assert sched.remaining_memory == 16384

    def test_multiple_allocations(self):
        sched = ResourceScheduler(
            total_cpu_quota=800_000, total_memory_mb=8192
        )
        limits = ResourceLimits(cpu_quota=200_000, memory_mb=2048)
        for i in range(4):
            sched.allocate(f"c{i}", limits)
        assert sched.remaining_cpu == 0
        assert sched.remaining_memory == 0
        # 5th should fail
        assert sched.can_launch(limits) is False

    def test_deallocate_nonexistent(self):
        sched = ResourceScheduler(
            total_cpu_quota=800_000, total_memory_mb=16384
        )
        # Should not raise
        sched.deallocate("nonexistent")
        assert sched.remaining_cpu == 800_000

    def test_double_allocate_ignored(self):
        sched = ResourceScheduler(
            total_cpu_quota=800_000, total_memory_mb=16384
        )
        limits = ResourceLimits(cpu_quota=200_000, memory_mb=2048)
        sched.allocate("c1", limits)
        sched.allocate("c1", limits)  # Should be ignored (warning logged)
        assert sched.remaining_cpu == 600_000

    def test_unlimited_resources(self):
        sched = ResourceScheduler(
            total_cpu_quota=800_000, total_memory_mb=16384
        )
        # cpu_quota=0 means unlimited — should always pass
        unlimited = ResourceLimits(cpu_quota=0, memory_mb=0)
        assert sched.can_launch(unlimited) is True

    # -- queue management ----------------------------------------------------

    def test_enqueue_and_dequeue(self):
        sched = ResourceScheduler(
            total_cpu_quota=800_000, total_memory_mb=16384
        )
        sched.enqueue("c1", priority=1)
        sched.enqueue("c2", priority=5)
        sched.enqueue("c3", priority=3)
        assert sched.queue_depth == 3

        eligible = sched.dequeue_eligible()
        assert len(eligible) == 3
        # Higher priority should come first
        assert eligible[0] == "c2"
        assert eligible[1] == "c3"
        assert eligible[2] == "c1"

    def test_remove_from_queue(self):
        sched = ResourceScheduler(
            total_cpu_quota=800_000, total_memory_mb=16384
        )
        sched.enqueue("c1", priority=1)
        sched.enqueue("c2", priority=2)
        assert sched.remove_from_queue("c1") is True
        assert sched.queue_depth == 1
        assert sched.remove_from_queue("nonexistent") is False

    # -- summary -------------------------------------------------------------

    def test_resource_summary(self):
        sched = ResourceScheduler(
            total_cpu_quota=800_000, total_memory_mb=16384
        )
        limits = ResourceLimits(cpu_quota=200_000, memory_mb=4096)
        sched.allocate("c1", limits)
        summary = sched.get_resource_summary()
        assert summary["cpu_allocated"] == 200_000
        assert summary["cpu_total"] == 800_000
        assert summary["memory_allocated_mb"] == 4096
        assert summary["active_allocations"] == 1
        assert summary["cpu_utilization_pct"] == 25.0

    # -- stress test ---------------------------------------------------------

    def test_many_campaigns(self):
        """Allocate and deallocate 100 campaigns sequentially."""
        sched = ResourceScheduler(
            total_cpu_quota=10_000_000, total_memory_mb=100_000
        )
        small = ResourceLimits(cpu_quota=100_000, memory_mb=1000)
        ids = [f"c{i}" for i in range(100)]
        for cid in ids:
            assert sched.can_launch(small)
            sched.allocate(cid, small)
        assert sched.remaining_cpu == 0
        assert sched.remaining_memory == 0
        for cid in ids:
            sched.deallocate(cid)
        assert sched.remaining_cpu == 10_000_000
        assert sched.remaining_memory == 100_000

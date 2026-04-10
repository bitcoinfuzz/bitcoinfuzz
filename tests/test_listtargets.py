"""
tests/test_listtargets.py

Tests for tools/listtargets.py — ensures all known driver.cpp
targets are parsed correctly.
"""

import json
import subprocess
import sys
from pathlib import Path

import pytest

# Path to the tool and driver relative to repo root
REPO_ROOT = Path(__file__).parent.parent
TOOL_PATH = REPO_ROOT / "tools" / "listtargets.py"
DRIVER_PATH = REPO_ROOT / "driver.cpp"

# The exact count of targets in driver.cpp at the time of this PR.
# Update this number if new targets are added.
EXPECTED_TARGET_COUNT = 29

# Spot-check: a sample of targets that must always be present.
KNOWN_TARGETS = [
    "address_parse",
    "bip32_master_keygen",
    "deserialize_block",
    "deserialize_invoice",
    "psbt_parse",
    "script",
    "script_eval",
    "sign_schnorr",
    "stump_modify_add",
    "verify_script",
]


class TestListTargetsText:
    """Tests for --format text (default) output."""

    def run_tool(self, *args):
        result = subprocess.run(
            [sys.executable, str(TOOL_PATH), "--driver", str(DRIVER_PATH), *args],
            capture_output=True,
            text=True,
        )
        return result

    def test_exits_zero(self):
        result = self.run_tool()
        assert result.returncode == 0, f"Tool exited with error: {result.stderr}"

    def test_outputs_nonempty(self):
        result = self.run_tool()
        lines = [l for l in result.stdout.strip().splitlines() if l]
        assert len(lines) > 0

    def test_target_count(self):
        result = self.run_tool()
        lines = [l for l in result.stdout.strip().splitlines() if l]
        assert len(lines) == EXPECTED_TARGET_COUNT, (
            f"Expected {EXPECTED_TARGET_COUNT} targets, got {len(lines)}. "
            f"Targets found: {lines}"
        )

    def test_known_targets_present(self):
        result = self.run_tool()
        lines = set(result.stdout.strip().splitlines())
        for target in KNOWN_TARGETS:
            assert target in lines, f"Expected target '{target}' not found"

    def test_output_is_sorted(self):
        result = self.run_tool()
        lines = [l for l in result.stdout.strip().splitlines() if l]
        assert lines == sorted(lines), "Output should be sorted alphabetically"

    def test_no_duplicates(self):
        result = self.run_tool()
        lines = [l for l in result.stdout.strip().splitlines() if l]
        assert len(lines) == len(set(lines)), "Output should have no duplicate targets"


class TestListTargetsJSON:
    """Tests for --format json output."""

    def run_tool_json(self):
        result = subprocess.run(
            [
                sys.executable,
                str(TOOL_PATH),
                "--driver",
                str(DRIVER_PATH),
                "--format",
                "json",
            ],
            capture_output=True,
            text=True,
        )
        assert result.returncode == 0
        return json.loads(result.stdout)

    def test_json_is_list(self):
        targets = self.run_tool_json()
        assert isinstance(targets, list)

    def test_json_count(self):
        targets = self.run_tool_json()
        assert len(targets) == EXPECTED_TARGET_COUNT

    def test_json_all_strings(self):
        targets = self.run_tool_json()
        assert all(isinstance(t, str) for t in targets)

    def test_json_sorted(self):
        targets = self.run_tool_json()
        assert targets == sorted(targets)

    def test_json_known_targets(self):
        targets = self.run_tool_json()
        for t in KNOWN_TARGETS:
            assert t in targets


class TestListTargetsEdgeCases:
    """Edge case tests."""

    def test_missing_driver_exits_nonzero(self, tmp_path):
        result = subprocess.run(
            [
                sys.executable,
                str(TOOL_PATH),
                "--driver",
                str(tmp_path / "nonexistent.cpp"),
            ],
            capture_output=True,
            text=True,
        )
        assert result.returncode != 0
        assert "Error" in result.stderr or "not found" in result.stderr.lower()

    def test_empty_file_exits_nonzero(self, tmp_path):
        empty = tmp_path / "driver.cpp"
        empty.write_text("")
        result = subprocess.run(
            [sys.executable, str(TOOL_PATH), "--driver", str(empty)],
            capture_output=True,
            text=True,
        )
        assert result.returncode != 0

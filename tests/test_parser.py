"""Tests for the libFuzzer log parser.

Covers pulse lines, READ lines, NEW/REDUCE entries, event detection,
corpus size parsing, and edge cases with malformed input.
"""

import pytest

from metrics_agent.parser import (
    FuzzEvent,
    FuzzStats,
    ReadStats,
    parse_corpus_size,
    parse_event_line,
    parse_line,
    parse_new_line,
    parse_pulse_line,
    parse_read_line,
    parse_reduce_line,
)


# ---------------------------------------------------------------------------
# Corpus size parsing
# ---------------------------------------------------------------------------

class TestParseCorpusSize:
    """Test the corpus size string parser."""

    def test_megabytes(self):
        assert parse_corpus_size("2Mb") == 2 * 1024 * 1024

    def test_kilobytes(self):
        assert parse_corpus_size("412Kb") == 412 * 1024

    def test_gigabytes(self):
        assert parse_corpus_size("1Gb") == 1024 * 1024 * 1024

    def test_bytes(self):
        assert parse_corpus_size("100b") == 100

    def test_fractional_megabytes(self):
        result = parse_corpus_size("1.5Mb")
        assert result == int(1.5 * 1024 * 1024)

    def test_case_insensitive(self):
        assert parse_corpus_size("2MB") == 2 * 1024 * 1024
        assert parse_corpus_size("2mb") == 2 * 1024 * 1024

    def test_empty_string(self):
        assert parse_corpus_size("") == 0

    def test_no_unit(self):
        assert parse_corpus_size("12345") == 0  # No unit suffix


# ---------------------------------------------------------------------------
# Pulse line parsing
# ---------------------------------------------------------------------------

class TestParsePulseLine:
    """Test parsing of libFuzzer pulse lines."""

    def test_standard_pulse(self):
        line = "#2097152 pulse  cov: 847 ft: 3201 corp: 412/2Mb exec/s: 14832 rss: 387Mb"
        stats = parse_pulse_line(line)
        assert stats is not None
        assert stats.iteration == 2097152
        assert stats.coverage == 847
        assert stats.features == 3201
        assert stats.corpus_count == 412
        assert stats.corpus_size_str == "2Mb"
        assert stats.exec_per_sec == 14832
        assert stats.rss_mb == 387

    def test_small_corpus(self):
        line = "#1024 pulse  cov: 12 ft: 45 corp: 3/100b exec/s: 5000 rss: 50Mb"
        stats = parse_pulse_line(line)
        assert stats is not None
        assert stats.corpus_count == 3
        assert stats.coverage == 12

    def test_large_iteration(self):
        line = "#999999999 pulse  cov: 9999 ft: 99999 corp: 50000/1Gb exec/s: 50000 rss: 1024Mb"
        stats = parse_pulse_line(line)
        assert stats is not None
        assert stats.iteration == 999999999
        assert stats.exec_per_sec == 50000

    def test_non_pulse_line(self):
        assert parse_pulse_line("Some random log output") is None

    def test_empty_line(self):
        assert parse_pulse_line("") is None

    def test_partial_pulse_line(self):
        line = "#2097152 pulse  cov: 847"
        assert parse_pulse_line(line) is None

    def test_timestamp_is_set(self):
        line = "#100 pulse  cov: 10 ft: 20 corp: 5/1Kb exec/s: 1000 rss: 100Mb"
        stats = parse_pulse_line(line)
        assert stats is not None
        assert stats.timestamp is not None


# ---------------------------------------------------------------------------
# READ line parsing
# ---------------------------------------------------------------------------

class TestParseReadLine:
    """Test parsing of libFuzzer READ lines (emitted at startup)."""

    def test_standard_read(self):
        line = "#0       READ units: 412 exec/s: 0 rss: 134Mb"
        stats = parse_read_line(line)
        assert stats is not None
        assert stats.units == 412
        assert stats.exec_per_sec == 0
        assert stats.rss_mb == 134

    def test_nonzero_read(self):
        line = "#100     READ units: 1000 exec/s: 500 rss: 256Mb"
        stats = parse_read_line(line)
        assert stats is not None
        assert stats.units == 1000

    def test_non_read_line(self):
        assert parse_read_line("not a read line") is None


# ---------------------------------------------------------------------------
# NEW and REDUCE line parsing
# ---------------------------------------------------------------------------

class TestParseNewLine:
    def test_new_corpus_entry(self):
        line = "#1234  NEW    cov: 100 ft: 200 corp: 5/1Kb exec/s: 3000 rss: 200Mb"
        stats = parse_new_line(line)
        assert stats is not None
        assert stats.coverage == 100
        assert stats.corpus_count == 5

    def test_non_new_line(self):
        assert parse_new_line("regular log output") is None


class TestParseReduceLine:
    def test_reduce_entry(self):
        line = "#5678  REDUCE cov: 100 ft: 200 corp: 5/900b exec/s: 2500 rss: 180Mb"
        stats = parse_reduce_line(line)
        assert stats is not None
        assert stats.corpus_count == 5

    def test_non_reduce_line(self):
        assert parse_reduce_line("not a reduce line") is None


# ---------------------------------------------------------------------------
# Event line detection
# ---------------------------------------------------------------------------

class TestParseEventLine:
    def test_done_event(self):
        line = "Done 1000000 runs in 60 seconds"
        event = parse_event_line(line)
        assert event is not None
        assert event.event_type == "DONE"

    def test_oom_event(self):
        line = "==12345== ERROR: libFuzzer: out-of-memory (used: 1024Mb; limit: 1024Mb)"
        event = parse_event_line(line)
        assert event is not None
        assert event.event_type == "OOM"

    def test_timeout_event(self):
        line = "==12345== ERROR: libFuzzer: timeout after 300 seconds"
        event = parse_event_line(line)
        assert event is not None
        assert event.event_type == "TIMEOUT"

    def test_crash_event(self):
        line = "==12345== ERROR: AddressSanitizer: heap-buffer-overflow"
        event = parse_event_line(line)
        assert event is not None
        assert event.event_type == "CRASH"

    def test_summary_event(self):
        line = "SUMMARY: AddressSanitizer: heap-buffer-overflow src/file.cpp:123"
        event = parse_event_line(line)
        assert event is not None
        assert event.event_type == "SUMMARY"

    def test_non_event_line(self):
        assert parse_event_line("regular fuzzer output") is None


# ---------------------------------------------------------------------------
# Universal parser
# ---------------------------------------------------------------------------

class TestParseLine:
    """Test the unified parse_line function."""

    def test_parses_pulse(self):
        line = "#1000 pulse  cov: 50 ft: 100 corp: 10/5Kb exec/s: 2000 rss: 128Mb"
        result = parse_line(line)
        assert isinstance(result, FuzzStats)

    def test_parses_read(self):
        line = "#0       READ units: 100 exec/s: 0 rss: 64Mb"
        result = parse_line(line)
        assert isinstance(result, ReadStats)

    def test_parses_event(self):
        line = "==1== ERROR: libFuzzer: timeout after 300 seconds"
        result = parse_line(line)
        assert isinstance(result, FuzzEvent)

    def test_returns_none_for_unknown(self):
        assert parse_line("INFO: Seed: 12345") is None
        assert parse_line("") is None
        assert parse_line("Loading modules...") is None


# ---------------------------------------------------------------------------
# Real-world log lines from actual bitcoinfuzz runs
# ---------------------------------------------------------------------------

class TestRealWorldLogLines:
    """Test against realistic libFuzzer output patterns."""

    PULSE_LINES = [
        "#2097152 pulse  cov: 847 ft: 3201 corp: 412/2Mb exec/s: 14832 rss: 387Mb",
        "#4194304 pulse  cov: 847 ft: 3201 corp: 412/2Mb exec/s: 15012 rss: 387Mb",
        "#1048576 pulse  cov: 523 ft: 1890 corp: 201/500Kb exec/s: 8932 rss: 256Mb",
        "#8388608 pulse  cov: 1200 ft: 5600 corp: 800/5Mb exec/s: 20000 rss: 512Mb",
        "#100 pulse  cov: 5 ft: 10 corp: 2/100b exec/s: 100 rss: 50Mb",
        "#16777216 pulse  cov: 2000 ft: 10000 corp: 1500/10Mb exec/s: 25000 rss: 800Mb",
    ]

    @pytest.mark.parametrize("line", PULSE_LINES)
    def test_pulse_lines_parse(self, line):
        result = parse_pulse_line(line)
        assert result is not None
        assert result.coverage > 0
        assert result.exec_per_sec >= 0

    READ_LINES = [
        "#0       READ units: 412 exec/s: 0 rss: 134Mb",
        "#0       READ units: 0 exec/s: 0 rss: 100Mb",
        "#0       READ units: 10000 exec/s: 0 rss: 512Mb",
    ]

    @pytest.mark.parametrize("line", READ_LINES)
    def test_read_lines_parse(self, line):
        result = parse_read_line(line)
        assert result is not None
        assert result.rss_mb > 0

    NON_METRIC_LINES = [
        "INFO: Seed: 3918206239",
        "INFO: Loaded 1 modules   (5432 inline 8-bit counters)",
        "INFO: Loaded 1 PC tables (5432 PCs)",
        "INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes",
        "INFO: A]I table size 1024",
        "",
        "INFO: Running with entropic power schedule (0xFF, 100).",
    ]

    @pytest.mark.parametrize("line", NON_METRIC_LINES)
    def test_non_metric_lines_return_none(self, line):
        assert parse_pulse_line(line) is None
        assert parse_read_line(line) is None

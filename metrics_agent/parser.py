"""libFuzzer log line parser.

Parses the structured text output from libFuzzer into typed data
structures.  Handles pulse lines, READ lines, and event lines
(DONE, OOM, TIMEOUT, CRASH).

The parser is intentionally lenient — unparseable lines return None
rather than raising exceptions, matching proposal §11 risk mitigation
for libFuzzer format changes.
"""

import re
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Optional

# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------


@dataclass
class FuzzStats:
    """Parsed statistics from a libFuzzer pulse line.

    Example input::

        #2097152 pulse  cov: 847 ft: 3201 corp: 412/2Mb exec/s: 14832 rss: 387Mb
    """

    iteration: int
    coverage: int  # Edge coverage count
    features: int  # Feature count
    corpus_count: int
    corpus_size_str: str  # Raw size string (e.g. "2Mb", "412Kb")
    corpus_bytes: int  # Parsed byte count
    exec_per_sec: int
    rss_mb: int
    timestamp: datetime


@dataclass
class ReadStats:
    """Parsed data from a libFuzzer READ line.

    Example input::

        #0       READ units: 412 exec/s: 0 rss: 134Mb
    """

    units: int
    exec_per_sec: int
    rss_mb: int
    timestamp: datetime


@dataclass
class FuzzEvent:
    """A libFuzzer event (non-metric line with semantic meaning)."""

    event_type: str  # "DONE", "OOM", "TIMEOUT", "CRASH", "RELOAD", "NEW", "REDUCE"
    raw_line: str
    timestamp: datetime


# ---------------------------------------------------------------------------
# Compiled regex patterns
# ---------------------------------------------------------------------------

# Pulse line:  #2097152 pulse  cov: 847 ft: 3201 corp: 412/2Mb exec/s: 14832 rss: 387Mb
_PULSE_PATTERN = re.compile(
    r"#(?P<iter>\d+)\s+pulse\s+"
    r"cov:\s*(?P<cov>\d+)\s+"
    r"ft:\s*(?P<ft>\d+)\s+"
    r"corp:\s*(?P<corp>\d+)/(?P<corp_size>\S+)\s+"
    r"exec/s:\s*(?P<exec_per_s>\d+)\s+"
    r"rss:\s*(?P<rss>\d+)Mb"
)

# READ line:   #0       READ units: 412 exec/s: 0 rss: 134Mb
_READ_PATTERN = re.compile(
    r"#\d+\s+READ\s+"
    r"units:\s*(?P<units>\d+)\s+"
    r"exec/s:\s*(?P<exec_per_s>\d+)\s+"
    r"rss:\s*(?P<rss>\d+)Mb"
)

# NEW corpus entry: #1234  NEW   cov: 100 ft: 200 corp: 5/1Kb ...
_NEW_PATTERN = re.compile(
    r"#(?P<iter>\d+)\s+NEW\s+"
    r"cov:\s*(?P<cov>\d+)\s+"
    r"ft:\s*(?P<ft>\d+)\s+"
    r"corp:\s*(?P<corp>\d+)/(?P<corp_size>\S+)\s+"
    r"exec/s:\s*(?P<exec_per_s>\d+)\s+"
    r"rss:\s*(?P<rss>\d+)Mb"
)

# REDUCE: #1234  REDUCE cov: 100 ft: 200 corp: 5/1Kb ...
_REDUCE_PATTERN = re.compile(
    r"#(?P<iter>\d+)\s+REDUCE\s+"
    r"cov:\s*(?P<cov>\d+)\s+"
    r"ft:\s*(?P<ft>\d+)\s+"
    r"corp:\s*(?P<corp>\d+)/(?P<corp_size>\S+)\s+"
    r"exec/s:\s*(?P<exec_per_s>\d+)\s+"
    r"rss:\s*(?P<rss>\d+)Mb"
)

# Event patterns
_EVENT_PATTERNS = {
    "DONE": re.compile(r"Done\s+\d+\s+runs"),
    "OOM": re.compile(r"==\d+==\s*ERROR:\s*libFuzzer:\s*out-of-memory"),
    "TIMEOUT": re.compile(r"==\d+==\s*ERROR:\s*libFuzzer:\s*timeout"),
    "CRASH": re.compile(r"==\d+==\s*ERROR:\s*(AddressSanitizer|libFuzzer)"),
    "SUMMARY": re.compile(r"SUMMARY:"),
}


# ---------------------------------------------------------------------------
# Size string parser
# ---------------------------------------------------------------------------

_SIZE_MULTIPLIERS = {
    "b": 1,
    "kb": 1024,
    "mb": 1024 * 1024,
    "gb": 1024 * 1024 * 1024,
}

_SIZE_PATTERN = re.compile(r"(?P<num>\d+(?:\.\d+)?)(?P<unit>[A-Za-z]+)")


def parse_corpus_size(size_str: str) -> int:
    """Parse a libFuzzer corpus size string to bytes.

    Examples::

        >>> parse_corpus_size("2Mb")
        2097152
        >>> parse_corpus_size("412Kb")
        421888
        >>> parse_corpus_size("100b")
        100
    """
    m = _SIZE_PATTERN.match(size_str)
    if not m:
        return 0
    num = float(m.group("num"))
    unit = m.group("unit").lower()
    multiplier = _SIZE_MULTIPLIERS.get(unit, 1)
    return int(num * multiplier)


# ---------------------------------------------------------------------------
# Parser functions
# ---------------------------------------------------------------------------

def _now() -> datetime:
    return datetime.now(timezone.utc)


def parse_pulse_line(line: str) -> Optional[FuzzStats]:
    """Parse a libFuzzer pulse line into a FuzzStats object.

    Returns None if the line does not match the expected format.
    """
    m = _PULSE_PATTERN.search(line)
    if not m:
        return None
    corpus_size_str = m.group("corp_size")
    return FuzzStats(
        iteration=int(m.group("iter")),
        coverage=int(m.group("cov")),
        features=int(m.group("ft")),
        corpus_count=int(m.group("corp")),
        corpus_size_str=corpus_size_str,
        corpus_bytes=parse_corpus_size(corpus_size_str),
        exec_per_sec=int(m.group("exec_per_s")),
        rss_mb=int(m.group("rss")),
        timestamp=_now(),
    )


def parse_new_line(line: str) -> Optional[FuzzStats]:
    """Parse a libFuzzer NEW corpus entry line."""
    m = _NEW_PATTERN.search(line)
    if not m:
        return None
    corpus_size_str = m.group("corp_size")
    return FuzzStats(
        iteration=int(m.group("iter")),
        coverage=int(m.group("cov")),
        features=int(m.group("ft")),
        corpus_count=int(m.group("corp")),
        corpus_size_str=corpus_size_str,
        corpus_bytes=parse_corpus_size(corpus_size_str),
        exec_per_sec=int(m.group("exec_per_s")),
        rss_mb=int(m.group("rss")),
        timestamp=_now(),
    )


def parse_reduce_line(line: str) -> Optional[FuzzStats]:
    """Parse a libFuzzer REDUCE line."""
    m = _REDUCE_PATTERN.search(line)
    if not m:
        return None
    corpus_size_str = m.group("corp_size")
    return FuzzStats(
        iteration=int(m.group("iter")),
        coverage=int(m.group("cov")),
        features=int(m.group("ft")),
        corpus_count=int(m.group("corp")),
        corpus_size_str=corpus_size_str,
        corpus_bytes=parse_corpus_size(corpus_size_str),
        exec_per_sec=int(m.group("exec_per_s")),
        rss_mb=int(m.group("rss")),
        timestamp=_now(),
    )


def parse_read_line(line: str) -> Optional[ReadStats]:
    """Parse a libFuzzer READ line into a ReadStats object."""
    m = _READ_PATTERN.search(line)
    if not m:
        return None
    return ReadStats(
        units=int(m.group("units")),
        exec_per_sec=int(m.group("exec_per_s")),
        rss_mb=int(m.group("rss")),
        timestamp=_now(),
    )


def parse_event_line(line: str) -> Optional[FuzzEvent]:
    """Detect event lines (DONE, OOM, TIMEOUT, CRASH, SUMMARY)."""
    for event_type, pattern in _EVENT_PATTERNS.items():
        if pattern.search(line):
            return FuzzEvent(
                event_type=event_type,
                raw_line=line.strip(),
                timestamp=_now(),
            )
    return None


def parse_line(line: str) -> Optional[FuzzStats | ReadStats | FuzzEvent]:
    """Parse any libFuzzer output line.

    Tries each parser in order of frequency:
    1. Pulse lines (most common during active fuzzing)
    2. NEW corpus entries
    3. REDUCE entries
    4. READ lines (at startup)
    5. Event lines (rare but important)

    Returns None for unrecognised lines.
    """
    result = parse_pulse_line(line)
    if result:
        return result

    result = parse_new_line(line)
    if result:
        return result

    result = parse_reduce_line(line)
    if result:
        return result

    result = parse_read_line(line)
    if result:
        return result

    result = parse_event_line(line)
    if result:
        return result

    return None

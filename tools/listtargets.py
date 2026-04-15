#!/usr/bin/env python3
"""List bitcoinfuzz targets by parsing the Driver::Run dispatch block."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


RUN_BLOCK_PATTERN = re.compile(
    r"void Driver::Run\(.*?\) const \{(?P<body>.*?)\n\};",
    re.DOTALL,
)
TARGET_PATTERN = re.compile(r'target == "([^"]+)"')


def default_driver_path() -> Path:
    return Path(__file__).resolve().parents[1] / "driver.cpp"


def extract_targets(driver_path: Path) -> list[str]:
    source = driver_path.read_text(encoding="utf-8")
    match = RUN_BLOCK_PATTERN.search(source)
    if match is None:
        raise ValueError(f"Could not locate Driver::Run dispatch block in {driver_path}")

    targets = TARGET_PATTERN.findall(match.group("body"))
    if not targets:
        raise ValueError(f"Could not find any targets in {driver_path}")

    return targets


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Parse driver.cpp and list supported bitcoinfuzz targets."
    )
    parser.add_argument(
        "--driver",
        type=Path,
        default=default_driver_path(),
        help="Path to driver.cpp (defaults to the repo root driver.cpp).",
    )
    parser.add_argument(
        "--format",
        choices=("text", "json"),
        default="text",
        help="Output format.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)

    try:
        targets = extract_targets(args.driver)
    except (OSError, ValueError) as exc:
        print(str(exc), file=sys.stderr)
        return 1

    if args.format == "json":
        json.dump(targets, sys.stdout, indent=2)
        sys.stdout.write("\n")
    else:
        for target in targets:
            print(target)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

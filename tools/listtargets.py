#!/usr/bin/env python3
"""
tools/listtargets.py

Parses driver.cpp's Run() dispatch block and outputs a list of
all known fuzz target names.

Usage:
    python tools/listtargets.py
    python tools/listtargets.py --format json
    python tools/listtargets.py --driver path/to/driver.cpp
"""

import re
import json
import argparse
import sys
from pathlib import Path


def parse_targets(driver_path: str = "driver.cpp") -> list[str]:
    """
    Parse all fuzz target names from the Run() dispatch block in driver.cpp.

    Looks for patterns like:  target == "target_name"
    Returns a sorted, deduplicated list of target name strings.
    """
    path = Path(driver_path)
    if not path.exists():
        raise FileNotFoundError(f"driver.cpp not found at: {path.resolve()}")

    content = path.read_text(encoding="utf-8")

    # Match: target == "some_target_name"
    targets = re.findall(r'target\s*==\s*"([^"]+)"', content)

    if not targets:
        raise ValueError(
            f"No targets found in {driver_path}. "
            "Check that the file contains 'target == \"...\"' patterns."
        )

    return sorted(set(targets))


def main() -> None:
    parser = argparse.ArgumentParser(
        description="List all fuzz targets defined in driver.cpp"
    )
    parser.add_argument(
        "--format",
        choices=["json", "text"],
        default="text",
        help="Output format (default: text)",
    )
    parser.add_argument(
        "--driver",
        default="driver.cpp",
        metavar="PATH",
        help="Path to driver.cpp (default: driver.cpp)",
    )
    args = parser.parse_args()

    try:
        targets = parse_targets(args.driver)
    except (FileNotFoundError, ValueError) as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    if args.format == "json":
        print(json.dumps(targets, indent=2))
    else:
        for target in targets:
            print(target)


if __name__ == "__main__":
    main()

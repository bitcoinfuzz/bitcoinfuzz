#!/usr/bin/env python3

import argparse
import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = ROOT / "fuzz-targets.json"
COMPOSE_PATH = ROOT / "docker-compose.yml"
WORKFLOW_PATH = ROOT / ".github" / "workflows" / "workflow.yml"
MATRIX_BEGIN = "        # BEGIN GENERATED MATRIX"
MATRIX_END = "        # END GENERATED MATRIX"


def load_manifest():
    data = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    targets = data.get("targets", [])
    if not isinstance(targets, list) or not targets:
        raise ValueError("fuzz-targets.json must contain a non-empty 'targets' list")
    return targets


def render_compose_service(entry):
    target = entry["target"]
    compose = entry["compose"]
    lines = [
        f"  {compose['service']}:",
        "    build:",
        "      context: .",
        "      tags:",
        f"        - bitcoinfuzz:{target}",
        "      args:",
        f'        CXXFLAGS: "{compose["cxxflags"]}"',
        f"        FUZZ: {target}",
        "    env_file: .env",
    ]

    env_lines = []
    if compose["modules_env"]:
        env_lines.append("      MODULES: ${MODULES:-}")
    if compose["disable_leak_detection"]:
        env_lines.append("      LIBFUZZ_DETECT_LEAKS: 0")
        env_lines.append("      ASAN_OPTIONS: detect_leaks=0")

    if env_lines:
        lines.append("    environment:")
        lines.extend(env_lines)

    lines.extend(
        [
            "    volumes:",
            "      - ./docker:/app/data",
        ]
    )
    return "\n".join(lines)


def render_docker_compose(targets):
    lines = ["services:"]
    for index, entry in enumerate(targets):
        lines.append(render_compose_service(entry))
        if index != len(targets) - 1:
            lines.append("")
    return "\n".join(lines) + "\n"


def render_workflow_matrix(targets):
    lines = [MATRIX_BEGIN]
    for entry in targets:
        ci = entry.get("ci", {})
        if not ci.get("enabled"):
            continue

        lines.extend(
            [
                f'          - corpus: "{ci["corpus"]}"',
                f'            target: "{entry["target"]}"',
                f'            cxxflags: "{ci["cxxflags"]}"',
            ]
        )
        if ci.get("asan_options"):
            lines.append(f'            asan_options: "{ci["asan_options"]}"')
        if ci.get("setup_java"):
            lines.append('            setup_java: "true"')
    lines.append(MATRIX_END)
    return "\n".join(lines)


def update_workflow(workflow_text, matrix_block):
    step_pattern = re.compile(
        r"(?ms)^      - name: Validate .*?\n        run: python3 scripts/(?:validate_targets|generate_targets)\.py(?: --check)?$"
    )
    workflow_text, replacements = step_pattern.subn(
        "      - name: Validate generated files match manifest\n"
        "        run: python3 scripts/generate_targets.py --check",
        workflow_text,
        count=1,
    )
    if replacements != 1:
        raise ValueError("Could not locate the target validation step in workflow.yml")

    matrix_pattern = re.compile(
        rf"(?ms)^{re.escape(MATRIX_BEGIN)}\n.*?^{re.escape(MATRIX_END)}$"
    )
    workflow_text, replacements = matrix_pattern.subn(matrix_block, workflow_text, count=1)
    if replacements != 1:
        raise ValueError("Could not locate generated matrix markers in workflow.yml")

    return workflow_text


def write_or_check(path, expected, check):
    current = path.read_text(encoding="utf-8")
    if current == expected:
        return True

    if check:
        print(f"{path.relative_to(ROOT)} is out of date", file=sys.stderr)
        return False

    path.write_text(expected, encoding="utf-8", newline="\n")
    print(f"Updated {path.relative_to(ROOT)}")
    return True


def main():
    parser = argparse.ArgumentParser(description="Generate fuzz target files from fuzz-targets.json")
    parser.add_argument("--check", action="store_true", help="fail if generated files are out of date")
    args = parser.parse_args()

    try:
        targets = load_manifest()
        compose_text = render_docker_compose(targets)
        workflow_text = update_workflow(
            WORKFLOW_PATH.read_text(encoding="utf-8"),
            render_workflow_matrix(targets),
        )
    except Exception as exc:
        print(f"Failed to generate target files: {exc}", file=sys.stderr)
        return 1

    compose_ok = write_or_check(COMPOSE_PATH, compose_text, args.check)
    workflow_ok = write_or_check(WORKFLOW_PATH, workflow_text, args.check)

    if compose_ok and workflow_ok:
        if args.check:
            print("Generated files match fuzz-targets.json")
        return 0

    return 1


if __name__ == "__main__":
    sys.exit(main())

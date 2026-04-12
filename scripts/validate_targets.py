#!/usr/bin/env python3

import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = ROOT / "fuzz-targets.json"
COMPOSE_PATH = ROOT / "docker-compose.yml"
WORKFLOW_PATH = ROOT / ".github" / "workflows" / "workflow.yml"

NEWLINE = r"(?:\r?\n)"


def load_manifest():
    data = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    targets = data.get("targets", [])
    if not isinstance(targets, list) or not targets:
        raise ValueError("fuzz-targets.json must contain a non-empty 'targets' list")

    manifest = {}
    for entry in targets:
        target = entry["target"]
        if target in manifest:
            raise ValueError(f"Duplicate target '{target}' in fuzz-targets.json")
        manifest[target] = entry
    return manifest


def parse_compose():
    text = COMPOSE_PATH.read_text(encoding="utf-8")
    pattern = re.compile(
        rf"^  (?P<service>[a-z0-9_]+):{NEWLINE}"
        rf"    build:.*?args:{NEWLINE}"
        rf'        CXXFLAGS: "(?P<cxxflags>[^"]+)"{NEWLINE}'
        rf"        FUZZ: (?P<fuzz>[a-z0-9_]+)"
        rf"(?P<body>.*?)(?=^  [a-z0-9_]+:{NEWLINE}|\Z)",
        re.MULTILINE | re.DOTALL,
    )

    services = {}
    for match in pattern.finditer(text):
        service = match.group("service")
        body = match.group("body")
        services[service] = {
            "service": service,
            "fuzz": match.group("fuzz"),
            "cxxflags": match.group("cxxflags"),
            "modules_env": "MODULES: ${MODULES:-}" in body,
            "disable_leak_detection": (
                "LIBFUZZ_DETECT_LEAKS: 0" in body and "ASAN_OPTIONS: detect_leaks=0" in body
            ),
        }
    return services


def parse_workflow():
    text = WORKFLOW_PATH.read_text(encoding="utf-8")
    pattern = re.compile(
        rf'^\s*-\s+corpus:\s*"(?P<corpus>[^"]+)"{NEWLINE}'
        rf'\s*target:\s*"(?P<target>[a-z0-9_]+)"{NEWLINE}'
        rf'\s*cxxflags:\s*"(?P<cxxflags>[^"]+)"'
        rf'(?:{NEWLINE}\s*asan_options:\s*"(?P<asan_options>[^"]*)")?',
        re.MULTILINE,
    )

    targets = {}
    for match in pattern.finditer(text):
        target = match.group("target")
        targets[target] = {
            "corpus": match.group("corpus"),
            "cxxflags": match.group("cxxflags"),
            "asan_options": match.group("asan_options") or "",
        }
    return targets


def validate(manifest, compose, workflow):
    errors = []

    manifest_targets = set(manifest)
    compose_targets = set(compose)
    workflow_targets = set(workflow)

    if compose_targets != manifest_targets:
        missing = sorted(manifest_targets - compose_targets)
        extra = sorted(compose_targets - manifest_targets)
        if missing:
            errors.append(
                "docker-compose.yml is missing manifest targets: " + ", ".join(missing)
            )
        if extra:
            errors.append(
                "docker-compose.yml has targets not in fuzz-targets.json: " + ", ".join(extra)
            )

    expected_workflow_targets = {
        target for target, entry in manifest.items() if entry.get("ci", {}).get("enabled")
    }
    if workflow_targets != expected_workflow_targets:
        missing = sorted(expected_workflow_targets - workflow_targets)
        extra = sorted(workflow_targets - expected_workflow_targets)
        if missing:
            errors.append(
                ".github/workflows/workflow.yml is missing manifest CI targets: "
                + ", ".join(missing)
            )
        if extra:
            errors.append(
                ".github/workflows/workflow.yml has CI targets not enabled in fuzz-targets.json: "
                + ", ".join(extra)
            )

    for target, entry in manifest.items():
        compose_expected = entry["compose"]
        actual_service = compose.get(target)
        if actual_service:
            if actual_service["fuzz"] != target:
                errors.append(
                    f"Compose service '{target}' sets FUZZ={actual_service['fuzz']} instead of {target}"
                )
            if actual_service["service"] != compose_expected["service"]:
                errors.append(
                    f"Compose service '{target}' should be named {compose_expected['service']}"
                )
            if actual_service["cxxflags"] != compose_expected["cxxflags"]:
                errors.append(
                    f"Compose target '{target}' has CXXFLAGS '{actual_service['cxxflags']}' "
                    f"but manifest expects '{compose_expected['cxxflags']}'"
                )
            if actual_service["modules_env"] != compose_expected["modules_env"]:
                errors.append(
                    f"Compose target '{target}' MODULES wiring is {actual_service['modules_env']} "
                    f"but manifest expects {compose_expected['modules_env']}"
                )
            if (
                actual_service["disable_leak_detection"]
                != compose_expected["disable_leak_detection"]
            ):
                errors.append(
                    f"Compose target '{target}' leak-detection override is "
                    f"{actual_service['disable_leak_detection']} but manifest expects "
                    f"{compose_expected['disable_leak_detection']}"
                )

        ci_expected = entry.get("ci", {})
        actual_ci = workflow.get(target)
        if ci_expected.get("enabled"):
            if not actual_ci:
                errors.append(f"Workflow is missing CI target '{target}'")
                continue
            if actual_ci["corpus"] != ci_expected["corpus"]:
                errors.append(
                    f"Workflow target '{target}' uses corpus '{actual_ci['corpus']}' "
                    f"but manifest expects '{ci_expected['corpus']}'"
                )
            if actual_ci["cxxflags"] != ci_expected["cxxflags"]:
                errors.append(
                    f"Workflow target '{target}' has CXXFLAGS '{actual_ci['cxxflags']}' "
                    f"but manifest expects '{ci_expected['cxxflags']}'"
                )
            if actual_ci["asan_options"] != ci_expected.get("asan_options", ""):
                errors.append(
                    f"Workflow target '{target}' has asan_options '{actual_ci['asan_options']}' "
                    f"but manifest expects '{ci_expected.get('asan_options', '')}'"
                )
        elif actual_ci:
            errors.append(
                f"Workflow target '{target}' is present, but fuzz-targets.json marks it CI-disabled"
            )

    return errors


def main():
    try:
        manifest = load_manifest()
    except Exception as exc:
        print(f"Failed to load fuzz-targets.json: {exc}", file=sys.stderr)
        return 1

    compose = parse_compose()
    workflow = parse_workflow()
    errors = validate(manifest, compose, workflow)

    if errors:
        print("Target validation failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print(
        f"Target validation passed for {len(manifest)} manifest entries, "
        f"{len(compose)} Compose services, and {len(workflow)} CI targets."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())

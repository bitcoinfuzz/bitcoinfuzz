#!/usr/bin/env python3
"""Generate a temporary Cargo workspace manifest for Rust module builds.

This keeps tracked manifests untouched while allowing optional git ref overrides.
"""

from __future__ import annotations

import argparse
from collections import OrderedDict
from dataclasses import dataclass
from pathlib import Path
import sys
from typing import Any, Iterable

try:
    import tomllib
except ModuleNotFoundError:
    try:
        import tomli as tomllib  # type: ignore
    except ModuleNotFoundError as exc:
        raise SystemExit("python3 with tomllib (or tomli) is required") from exc

DEP_TABLE_KEYS = {"dependencies", "dev-dependencies", "build-dependencies"}


@dataclass(frozen=True)
class GitDependency:
    dep_key: str
    package: str
    git_url: str


def iter_dependency_tables(node: Any) -> Iterable[dict[str, Any]]:
    """Yield dependency-like tables from a parsed Cargo manifest."""
    if isinstance(node, dict):
        for key, value in node.items():
            if key in DEP_TABLE_KEYS and isinstance(value, dict):
                yield value
            yield from iter_dependency_tables(value)
    elif isinstance(node, list):
        for value in node:
            yield from iter_dependency_tables(value)


def collect_git_dependencies(manifest: dict[str, Any]) -> list[GitDependency]:
    deps: list[GitDependency] = []
    for table in iter_dependency_tables(manifest):
        for dep_key, value in table.items():
            if not isinstance(value, dict):
                continue
            git_url = value.get("git")
            if not isinstance(git_url, str) or not git_url:
                continue

            package = value.get("package", dep_key)
            if not isinstance(package, str) or not package:
                package = dep_key

            deps.append(GitDependency(dep_key=dep_key, package=package, git_url=git_url))

    dedup: OrderedDict[tuple[str, str, str], GitDependency] = OrderedDict()
    for dep in deps:
        dedup[(dep.dep_key, dep.package, dep.git_url)] = dep
    return list(dedup.values())


def resolve_root_urls(
    deps: list[GitDependency],
    patch_roots: list[str],
) -> list[str]:
    root_urls: OrderedDict[str, str] = OrderedDict()

    for root in patch_roots:
        matches = [d for d in deps if d.dep_key == root or d.package == root]
        if not matches:
            raise ValueError(
                f"Could not resolve patch root '{root}' from Cargo manifest dependencies"
            )

        urls = OrderedDict((d.git_url, None) for d in matches)
        if len(urls) != 1:
            raise ValueError(
                f"Patch root '{root}' maps to multiple git URLs: {', '.join(urls.keys())}"
            )

        root_urls[root] = next(iter(urls.keys()))

    return list(OrderedDict((url, None) for url in root_urls.values()).keys())


def generate_manifest_text(
    workspace_member: str,
    deps: list[GitDependency],
    patch_roots: list[str],
    resolved_ref: str,
) -> str:
    lines: list[str] = [
        "[workspace]",
        'resolver = "2"',
        f'members = ["{workspace_member}"]',
    ]

    if resolved_ref:
        if not patch_roots:
            raise ValueError("At least one --patch-root is required when --resolved-ref is set")

        root_urls = resolve_root_urls(deps, patch_roots)

        for url in root_urls:
            packages = sorted({d.package for d in deps if d.git_url == url})
            if not packages:
                raise ValueError(f"No packages found to patch for git URL {url}")

            lines.append("")
            lines.append(f'[patch."{url}"]')
            for package in packages:
                lines.append(f'{package} = {{ git = "{url}", rev = "{resolved_ref}" }}')

    return "\n".join(lines) + "\n"


def write_if_changed(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    if path.exists() and path.read_text(encoding="utf-8") == content:
        return

    path.write_text(content, encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-manifest", required=True, help="Path to source Cargo.toml")
    parser.add_argument("--workspace-member", required=True, help="Workspace member name")
    parser.add_argument("--output", required=True, help="Path to generated workspace Cargo.toml")
    parser.add_argument("--resolved-ref", default="", help="Override git revision")
    parser.add_argument(
        "--patch-root",
        action="append",
        default=[],
        help="Dependency key or package name used to anchor patch URLs",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    source_manifest_path = Path(args.source_manifest)
    output_path = Path(args.output)

    try:
        with source_manifest_path.open("rb") as fh:
            source_manifest = tomllib.load(fh)
    except FileNotFoundError:
        print(f"ERROR: source manifest not found: {source_manifest_path}", file=sys.stderr)
        return 1
    except tomllib.TOMLDecodeError as exc:
        print(f"ERROR: failed to parse TOML {source_manifest_path}: {exc}", file=sys.stderr)
        return 1

    try:
        deps = collect_git_dependencies(source_manifest)
        manifest_text = generate_manifest_text(
            workspace_member=args.workspace_member,
            deps=deps,
            patch_roots=args.patch_root,
            resolved_ref=args.resolved_ref.strip(),
        )
    except ValueError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    write_if_changed(output_path, manifest_text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

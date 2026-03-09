#!/usr/bin/env python3
"""Generate a dependency report that distinguishes feature-specific paths."""

from __future__ import annotations

import json
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = REPO_ROOT / "vcpkg.json"
VCPKG_CONFIG_PATH = REPO_ROOT / "vcpkg-configuration.json"


INTERNAL_PROVENANCE = {
    "kcenon-common-system": {
        "version": "0.2.0",
        "rule": "Pinned by overlay/vcpkg port version 0.2.0 and the repository baseline; source builds must record exact git tag or commit SHA",
        "path": "default",
    },
    "kcenon-thread-system": {
        "version": "0.3.0",
        "rule": "Pinned by overlay/vcpkg port version 0.3.0 and the repository baseline; source builds must record exact git tag or commit SHA",
        "path": "default",
    },
    "kcenon-logger-system": {
        "version": "0.1.0",
        "rule": "Optional logging feature dependency; source builds must record exact git tag or commit SHA",
        "path": "logging",
    },
    "network_system": {
        "version": "source-defined",
        "rule": "CMake-only integration; release SBOMs must capture the exact git tag or commit SHA",
        "path": "network",
    },
}


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def format_dependency(dep: str | dict) -> tuple[str, str, str]:
    if isinstance(dep, str):
        return dep, "-", "-"

    return (
        dep.get("name", "unknown"),
        dep.get("version>=", "-"),
        ", ".join(dep.get("features", [])) or "-",
    )


def print_table(headers: tuple[str, ...], rows: list[tuple[str, ...]]) -> None:
    widths = ["-" * len(header) for header in headers]
    print("| " + " | ".join(headers) + " |")
    print("| " + " | ".join(widths) + " |")
    for row in rows:
        print("| " + " | ".join(row) + " |")


def main() -> int:
    manifest = read_json(MANIFEST_PATH)
    vcpkg_config = read_json(VCPKG_CONFIG_PATH)
    baseline = vcpkg_config["default-registry"]["baseline"]
    overrides = {item["name"]: item["version"] for item in manifest.get("overrides", [])}

    print("# Dependency Inventory")
    print()
    print(f"**Project**: {manifest.get('name', 'N/A')}")
    print(f"**Version**: {manifest.get('version', 'N/A')}")
    print(f"**Pinned vcpkg baseline**: `{baseline}`")
    print()

    print("## Default Build")
    print()
    default_rows: list[tuple[str, ...]] = []
    for dep in manifest.get("dependencies", []):
        dep_name = dep if isinstance(dep, str) else dep.get("name", "unknown")
        meta = INTERNAL_PROVENANCE.get(dep_name, {})
        default_rows.append(
            (
                dep_name,
                meta.get("version", "-"),
                meta.get("rule", "Document exact source provenance in the release SBOM"),
            )
        )
    print_table(("Dependency", "Version", "Provenance Rule"), default_rows)
    print()

    print("## vcpkg Feature Paths")
    print()
    for feature_name, feature_data in manifest.get("features", {}).items():
        print(f"### {feature_name}")
        print()
        rows: list[tuple[str, ...]] = []
        for dep in feature_data.get("dependencies", []):
            name, version, features = format_dependency(dep)
            meta = INTERNAL_PROVENANCE.get(name, {})
            resolved_version = meta.get("version", overrides.get(name, version))
            rule = meta.get("rule", "Pinned through manifest feature dependency")
            if features != "-":
                rule = f"{rule}; features: {features}"
            rows.append((name, resolved_version, rule))
        if rows:
            print_table(("Dependency", "Version", "Provenance Rule"), rows)
        else:
            print("No additional dependencies.")
        print()

    print("## CMake-Only Optional Paths")
    print()
    print_table(
        ("Path", "Activation", "Dependency", "Provenance Rule"),
        [
            (
                "Network export path",
                "`-DMONITORING_WITH_NETWORK_SYSTEM=ON`",
                "network_system",
                INTERNAL_PROVENANCE["network_system"]["rule"],
            ),
        ],
    )
    print()
    print(
        "Interpret feature-specific SBOMs by combining the default build dependencies "
        "with the rows from each enabled vcpkg feature and any active CMake-only optional paths."
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

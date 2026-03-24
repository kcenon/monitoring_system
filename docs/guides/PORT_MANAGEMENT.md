# vcpkg Port Management

This document defines the authoritative port management strategy for the kcenon
ecosystem and serves as the definitive reference for all port-related work.

## Strategy: Centralized in monitoring_system

**Decision**: All vcpkg port definitions are maintained in
`kcenon/monitoring_system/vcpkg-ports/`.

This repository is the **single source of truth** for every kcenon port.
No other repository should maintain its own copy of a port definition.

### Rationale

| Criterion | Outcome |
|-----------|---------|
| Single update location | All eight ports live in one directory |
| Patch management | Upstream patches stored alongside port definitions |
| CI validation | `validate-vcpkg-chain` workflow tests every port change |
| Dependency resolution | All inter-port dependencies visible in one place |

Alternative strategies were considered and rejected:

- **Per-repo ports** (Option C): Stale local copies in `database_system`,
  `network_system`, and `pacs_system` already demonstrated how quickly
  per-repo ports fall out of sync (SHA512 placeholders, missing patches).
- **Dedicated vcpkg-registry repo** (Option B): Adds an extra repository to
  maintain with no benefit over the existing `kcenon/vcpkg-registry.git`
  workflow, which is populated from monitoring_system on each release.

## Port Inventory

| Port | Upstream Repo | Port-Version | Patches |
|------|--------------|-------------|---------|
| kcenon-common-system | `kcenon/common_system` | 0 | — |
| kcenon-thread-system | `kcenon/thread_system` | 0 | — |
| kcenon-logger-system | `kcenon/logger_system` | 0 | fix-unified-deps-target-names |
| kcenon-container-system | `kcenon/container_system` | 0 | — |
| kcenon-monitoring-system | `kcenon/monitoring_system` | 0 | — |
| kcenon-database-system | `kcenon/database_system` | 2 | fix-common-system-target |
| kcenon-network-system | `kcenon/network_system` | 3 | fix-common-system-target |
| kcenon-pacs-system | `kcenon/pacs_system` | 2 | fix-vcpkg-dependency-discovery |

Port-version is incremented when the portfile changes but the upstream source
does not (e.g., adding a patch or fixing a CMake option).

## Port Update Procedure

Follow these steps whenever an upstream repository tags a new release.

### Step 1 — Determine the new commit

```bash
# Fetch the new tag SHA from GitHub
gh release view v<NEW_VERSION> --repo kcenon/<system_name> --json tagName,targetCommitish
```

### Step 2 — Download and hash the archive

```bash
# Download the source archive vcpkg will fetch
REPO="kcenon/<system_name>"
REF="v<NEW_VERSION>"
URL="https://github.com/${REPO}/archive/refs/tags/${REF}.tar.gz"
curl -fsSL "$URL" -o /tmp/source.tar.gz
sha512sum /tmp/source.tar.gz
```

### Step 3 — Update the portfile

Edit `vcpkg-ports/kcenon-<system>-system/portfile.cmake`:

```cmake
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/<system_name>
    REF v<NEW_VERSION>          # ← update
    SHA512 <new_sha512_hash>    # ← update
    HEAD_REF main
)
```

### Step 4 — Update vcpkg.json

Edit `vcpkg-ports/kcenon-<system>-system/vcpkg.json`:

```json
{
  "name": "kcenon-<system>-system",
  "version": "<new_version>",    ← update
  "port-version": 0,             ← reset to 0 on upstream version bump
  ...
}
```

> **Note**: Reset `port-version` to `0` whenever the upstream `version` field
> changes.  Increment `port-version` only for portfile-only fixes.

### Step 5 — Test locally

```bash
# Verify the port installs cleanly
vcpkg install kcenon-<system>-system \
  --overlay-ports=./vcpkg-ports \
  --triplet x64-linux
```

### Step 6 — Update the vcpkg-ports/README.md version table

Update the `Version` and `Port-Version` columns in
`vcpkg-ports/README.md`.

### Step 7 — Open a PR

Create a PR targeting `main`.  The `validate-vcpkg-chain` CI workflow will
automatically build and test the updated port on Ubuntu, macOS, and Windows.

### Step 8 — Sync to vcpkg-registry (automated)

When a release tag is created, the `sync-vcpkg-registry` workflow
automatically opens a PR to `kcenon/vcpkg-registry` with the updated
portfiles and version database entries.  **No manual sync is required.**

The workflow performs the following steps:

1. Downloads the release archive and computes the SHA512 hash
2. Updates `portfile.cmake` with the new SHA512 and verifies the REF tag
3. Updates `vcpkg.json` with the new version (resets `port-version` to 0)
4. Copies the updated port files to the registry
5. Updates `versions/<prefix>/<port-name>.json` with a new version entry
6. Updates `versions/baseline.json` with the new baseline
7. Opens a PR to `kcenon/vcpkg-registry` for review

> **Important**: The registry PR still requires manual approval before merge.
> This safety gate allows a maintainer to verify the changes before they
> reach production consumers.

#### Trigger

The sync is triggered by the caller workflow `on-release-sync-registry.yml`,
which fires on `release: [published]` events.  Each ecosystem repository
adds its own caller workflow (see [Adopting the Workflow in Other
Repositories](#adopting-the-workflow-in-other-repositories)).

#### Manual fallback

If the automated workflow fails or needs to be bypassed, the registry can
still be updated manually:

```bash
# Copy updated port files to the vcpkg-registry checkout
cp -r vcpkg-ports/kcenon-<system>-system/ \
      /path/to/vcpkg-registry/ports/kcenon-<system>-system/

# Update the versions database in vcpkg-registry
# (follow vcpkg versioning documentation for baseline/versions files)
```

## Adopting the Workflow in Other Repositories

Each ecosystem repository needs a caller workflow to trigger the reusable
`sync-vcpkg-registry` workflow on release.  Create
`.github/workflows/on-release-sync-registry.yml`:

```yaml
name: Sync Registry on Release

on:
  release:
    types: [published]

jobs:
  sync:
    uses: kcenon/monitoring_system/.github/workflows/sync-vcpkg-registry.yml@main
    with:
      port-name: kcenon-<system>-system   # e.g., kcenon-common-system
      version: ${{ github.event.release.tag_name }}
    secrets:
      REGISTRY_PAT: ${{ secrets.VCPKG_REGISTRY_PAT }}
```

### Prerequisites

- A fine-grained PAT (`VCPKG_REGISTRY_PAT`) with write access scoped to
  `kcenon/vcpkg-registry`, stored as an organization or repository secret.
- The port definition for the system must exist in
  `monitoring_system/vcpkg-ports/kcenon-<system>-system/`.

## Consumer Repository Setup

Repositories that depend on kcenon packages should configure
`vcpkg-configuration.json` to reference the remote registry:

```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg-configuration.schema.json",
  "default-registry": {
    "kind": "builtin",
    "baseline": "dd306f32e07d87fdb16837af64f33b6b415c770a"
  },
  "registries": [
    {
      "kind": "git",
      "repository": "https://github.com/kcenon/vcpkg-registry.git",
      "baseline": "<latest-vcpkg-registry-commit-sha>",
      "packages": [
        "kcenon-*"
      ]
    }
  ]
}
```

With this configuration, `vcpkg install kcenon-<package>` resolves
automatically without `--overlay-ports`.  All eight ecosystem repositories
(`common_system`, `thread_system`, `logger_system`, `container_system`,
`monitoring_system`, `database_system`, `network_system`, `pacs_system`)
use this pattern.

For development or pre-release testing, the overlay approach remains
available:

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=/path/to/monitoring_system/vcpkg-ports
```

## Stale Local Ports — Cleanup Required

The following repositories previously contained local port copies that are
now stale (outdated versions, `SHA512 0` placeholders, missing patches).
They should be removed in favour of the canonical ports in this repository.

| Repository | Stale Path | Status |
|------------|-----------|--------|
| `kcenon/database_system` | `vcpkg-ports/` | Needs removal (port-v1, SHA512=0) |
| `kcenon/network_system` | `vcpkg-ports/` | Needs removal (port-v0, SHA512=0) |
| `kcenon/pacs_system` | `vcpkg-ports/` | Needs removal (port-v0, SHA512=0) |

Open a PR in each repository to delete their `vcpkg-ports/` directory and
update their README to point to this canonical location.

## Relationship with vcpkg-registry

`kcenon/vcpkg-registry.git` provides a stable, versioned view of kcenon ports
for consumers who prefer not to use overlay ports.  It is **downstream** of
this directory: changes land here first, then propagate to the registry on
release via the automated `sync-vcpkg-registry` workflow.

```
monitoring_system/vcpkg-ports/   ← source of truth (development)
         │
         │  sync-vcpkg-registry.yml (automated on release)
         ▼
kcenon/vcpkg-registry.git        ← stable registry (production consumers)
```

Do not make direct edits to `kcenon/vcpkg-registry.git` port files.
All changes must flow through `monitoring_system/vcpkg-ports/` first.

## CI Validation

The `validate-vcpkg-chain` workflow (`.github/workflows/validate-vcpkg-chain.yml`)
runs on every PR that touches:

- `vcpkg-ports/**`
- `vcpkg.json`
- `vcpkg-configuration.json`
- `integration_tests/vcpkg_consumer/**`
- `scripts/validate_vcpkg_chain.sh`

The workflow installs the full kcenon dependency chain via the overlay ports
and builds the consumer integration test on three platforms.  A PR that breaks
any port will fail this check before merge.

## Related

- [VCPKG_OVERLAY_PORTS.md](VCPKG_OVERLAY_PORTS.md) — installation and usage guide
- [vcpkg-ports/README.md](../../vcpkg-ports/README.md) — port inventory
- [sync-vcpkg-registry.yml](../../.github/workflows/sync-vcpkg-registry.yml) — reusable registry sync workflow
- [on-release-sync-registry.yml](../../.github/workflows/on-release-sync-registry.yml) — caller workflow for monitoring_system
- [#533](https://github.com/kcenon/monitoring_system/issues/533) — issue tracking the centralization decision
- [#607](https://github.com/kcenon/monitoring_system/issues/607) — issue tracking the automation workflow

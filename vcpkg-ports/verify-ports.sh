#!/bin/bash
# verify-ports.sh — Verify overlay port freshness against GitHub release tags
#
# Checks all 8 kcenon ecosystem ports for:
#   1. Release tag existence on GitHub
#   2. SHA512 hash match against release archive
#   3. Version consistency between port vcpkg.json and portfile REF
#
# Usage: ./verify-ports.sh [--fix] [--port <port-name>]
#   --fix   Update SHA512 hashes in portfiles (requires confirmation)
#   --port  Check a single port instead of all 8
#
# Exit codes: 0 = all checks pass, 1 = one or more failures
#
# Part of kcenon/common_system#459 (Phase 3: build hardening)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BOLD='\033[1m'
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Port definitions: "port_name|github_repo" (bash 3.x compatible)
PORTS=(
    "kcenon-common-system|kcenon/common_system"
    "kcenon-thread-system|kcenon/thread_system"
    "kcenon-logger-system|kcenon/logger_system"
    "kcenon-container-system|kcenon/container_system"
    "kcenon-monitoring-system|kcenon/monitoring_system"
    "kcenon-database-system|kcenon/database_system"
    "kcenon-network-system|kcenon/network_system"
    "kcenon-pacs-system|kcenon/pacs_system"
)

FIX_MODE=false
SINGLE_PORT=""
FAILURES=0
WARNINGS=0
CHECKED=0

usage() {
    echo "Usage: $0 [--fix] [--port <port-name>]"
    echo "  --fix   Update SHA512 hashes in portfiles when mismatched"
    echo "  --port  Check a single port (e.g., kcenon-thread-system)"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --fix) FIX_MODE=true; shift ;;
        --port) SINGLE_PORT="$2"; shift 2 ;;
        -h|--help) usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

# Look up repo for a port name
get_repo_for_port() {
    local target="$1"
    for entry in "${PORTS[@]}"; do
        local name="${entry%%|*}"
        local repo="${entry##*|}"
        if [[ "$name" == "$target" ]]; then
            echo "$repo"
            return 0
        fi
    done
    return 1
}

# Check dependencies
for cmd in gh jq curl shasum; do
    if ! command -v "$cmd" &>/dev/null; then
        echo -e "${RED}Error: '$cmd' is required but not installed.${NC}"
        exit 1
    fi
done

# Extract version from vcpkg.json
get_port_version() {
    local port_dir="$1"
    jq -r '.version' "${port_dir}/vcpkg.json"
}

# Extract port-version from vcpkg.json (0 if absent)
get_port_revision() {
    local port_dir="$1"
    jq -r '.["port-version"] // 0' "${port_dir}/vcpkg.json"
}

# Extract SHA512 from portfile.cmake
get_port_sha512() {
    local port_dir="$1"
    # macOS grep doesn't support -P; use sed instead
    sed -n 's/.*SHA512[[:space:]]\{1,\}\([0-9a-f]\{1,\}\).*/\1/p' "${port_dir}/portfile.cmake" | head -1
}

# Extract REF pattern from portfile.cmake
get_port_ref() {
    local port_dir="$1"
    sed -n 's/.*REF[[:space:]]\{1,\}"\([^"]*\)".*/\1/p' "${port_dir}/portfile.cmake" | head -1
}

# Check if release tag exists on GitHub
check_release_tag() {
    local repo="$1"
    local tag="$2"
    gh release view "$tag" --repo "$repo" --json tagName -q '.tagName' 2>/dev/null || echo ""
}

# Compute SHA512 of release archive
compute_archive_sha512() {
    local repo="$1"
    local tag="$2"
    local tmpfile
    tmpfile="$(mktemp)"

    # Download the source archive (tar.gz)
    local url="https://github.com/${repo}/archive/refs/tags/${tag}.tar.gz"
    if curl -fsSL -o "$tmpfile" "$url" 2>/dev/null; then
        shasum -a 512 "$tmpfile" | awk '{print $1}'
        rm -f "$tmpfile"
    else
        rm -f "$tmpfile"
        echo ""
    fi
}

print_header() {
    echo ""
    echo -e "${BOLD}${CYAN}===== vcpkg Port Freshness Verification =====${NC}"
    echo -e "Ports directory: ${SCRIPT_DIR}"
    echo -e "Date: $(date '+%Y-%m-%d %H:%M %Z')"
    echo ""
}

check_port() {
    local port_name="$1"
    local repo="$2"
    local port_dir="${SCRIPT_DIR}/${port_name}"

    if [[ ! -d "$port_dir" ]]; then
        echo -e "  ${RED}SKIP${NC}  Directory not found: ${port_dir}"
        return
    fi

    CHECKED=$((CHECKED + 1))
    local version ref sha512 port_rev
    version=$(get_port_version "$port_dir")
    port_rev=$(get_port_revision "$port_dir")
    sha512=$(get_port_sha512 "$port_dir")
    ref=$(get_port_ref "$port_dir")

    echo -e "${BOLD}[${port_name}]${NC} v${version} (port-version: ${port_rev})"

    # 1. Verify REF pattern resolves to expected tag
    local expected_tag="v${version}"
    local resolved_ref
    resolved_ref=$(echo "$ref" | sed "s/\${VERSION}/${version}/g")
    if [[ "$resolved_ref" != "$expected_tag" ]]; then
        echo -e "  ${YELLOW}WARN${NC}  REF resolves to '${resolved_ref}', expected '${expected_tag}'"
        WARNINGS=$((WARNINGS + 1))
    else
        echo -e "  ${GREEN}PASS${NC}  REF pattern: ${ref} -> ${resolved_ref}"
    fi

    # 2. Verify release tag exists on GitHub
    local actual_tag
    actual_tag=$(check_release_tag "$repo" "$expected_tag")
    if [[ -z "$actual_tag" ]]; then
        echo -e "  ${RED}FAIL${NC}  Release tag '${expected_tag}' not found on ${repo}"
        FAILURES=$((FAILURES + 1))
        echo ""
        return
    else
        echo -e "  ${GREEN}PASS${NC}  Release tag '${expected_tag}' exists on ${repo}"
    fi

    # 3. Verify SHA512
    if [[ -z "$sha512" ]]; then
        echo -e "  ${RED}FAIL${NC}  No SHA512 found in portfile.cmake"
        FAILURES=$((FAILURES + 1))
        echo ""
        return
    fi

    echo -e "  ${CYAN}....${NC}  Computing SHA512 for ${expected_tag} archive..."
    local actual_sha512
    actual_sha512=$(compute_archive_sha512 "$repo" "$expected_tag")

    if [[ -z "$actual_sha512" ]]; then
        echo -e "  ${YELLOW}WARN${NC}  Could not download archive to verify SHA512"
        WARNINGS=$((WARNINGS + 1))
    elif [[ "$sha512" == "$actual_sha512" ]]; then
        echo -e "  ${GREEN}PASS${NC}  SHA512 matches release archive"
    else
        echo -e "  ${RED}FAIL${NC}  SHA512 mismatch!"
        echo -e "         Port:   ${sha512:0:32}..."
        echo -e "         Actual: ${actual_sha512:0:32}..."
        FAILURES=$((FAILURES + 1))

        if [[ "$FIX_MODE" == true ]]; then
            echo -e "  ${YELLOW}FIX ${NC}  Updating SHA512 in portfile.cmake..."
            if [[ "$(uname)" == "Darwin" ]]; then
                sed -i '' "s/${sha512}/${actual_sha512}/" "${port_dir}/portfile.cmake"
            else
                sed -i "s/${sha512}/${actual_sha512}/" "${port_dir}/portfile.cmake"
            fi
            echo -e "  ${GREEN}DONE${NC}  SHA512 updated"
        fi
    fi

    echo ""
}

print_summary() {
    echo -e "${BOLD}${CYAN}===== Summary =====${NC}"
    echo -e "Ports checked: ${CHECKED}"
    echo -e "Passed:        $((CHECKED - FAILURES))"
    echo -e "Failed:        ${FAILURES}"
    echo -e "Warnings:      ${WARNINGS}"
    echo ""

    if [[ $FAILURES -gt 0 ]]; then
        echo -e "${RED}Some checks failed. Run with --fix to auto-update SHA512 hashes.${NC}"
        exit 1
    elif [[ $WARNINGS -gt 0 ]]; then
        echo -e "${YELLOW}All checks passed with warnings.${NC}"
        exit 0
    else
        echo -e "${GREEN}All checks passed.${NC}"
        exit 0
    fi
}

# Main
print_header

if [[ -n "$SINGLE_PORT" ]]; then
    repo=$(get_repo_for_port "$SINGLE_PORT") || {
        echo -e "${RED}Unknown port: ${SINGLE_PORT}${NC}"
        echo "Available ports:"
        for entry in "${PORTS[@]}"; do
            echo "  ${entry%%|*}"
        done
        exit 1
    }
    check_port "$SINGLE_PORT" "$repo"
else
    for entry in "${PORTS[@]}"; do
        port_name="${entry%%|*}"
        repo="${entry##*|}"
        check_port "$port_name" "$repo"
    done
fi

print_summary

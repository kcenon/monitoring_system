# CI/CD Guide for Monitoring System

> **Version:** 0.1.0
> **Last Updated:** 2025-11-11
> **Audience:** Contributors, Maintainers

This guide covers the Continuous Integration and Continuous Deployment (CI/CD) pipeline for monitoring_system, including GitHub Actions workflows, local testing, and deployment procedures.

---

## Table of Contents

1. [Overview](#overview)
2. [GitHub Actions Workflows](#github-actions-workflows)
3. [Building Locally](#building-locally)
4. [Running Tests](#running-tests)
5. [Code Coverage](#code-coverage)
6. [Sanitizers](#sanitizers)
7. [Static Analysis](#static-analysis)
8. [Benchmarks](#benchmarks)
9. [Integration Tests](#integration-tests)
10. [Release Process](#release-process)
11. [Troubleshooting CI](#troubleshooting-ci)

---

## Overview

### CI/CD Pipeline Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Pull Request Created                     │
└───────────────────────┬─────────────────────────────────────┘
                        │
            ┌───────────┴───────────┐
            │                       │
      ┌─────▼─────┐         ┌──────▼──────┐
      │   ci.yml  │         │coverage.yml │
      │ Build &   │         │Code Coverage│
      │ Unit Test │         │   Report    │
      └─────┬─────┘         └──────┬──────┘
            │                      │
      ┌─────▼──────────┐    ┌─────▼──────────┐
      │ sanitizers.yml │    │integration-    │
      │ TSan/ASan/UBSan│    │tests.yml       │
      └─────┬──────────┘    └─────┬──────────┘
            │                     │
      ┌─────▼──────────────┐      │
      │ static-analysis.yml│      │
      │ Clang-Tidy/Cppcheck│      │
      └─────┬──────────────┘      │
            │                     │
            └──────────┬──────────┘
                       │
              ┌────────▼─────────┐
              │  All Checks Pass  │
              │  Ready to Merge   │
              └────────┬──────────┘
                       │
              ┌────────▼─────────┐
              │  Merge to main   │
              └────────┬──────────┘
                       │
              ┌────────▼──────────┐
              │  benchmarks.yml   │
              │  Performance Test │
              └────────┬───────────┘
                       │
              ┌────────▼──────────┐
              │ build-Doxygen.yaml│
              │  API Documentation│
              └────────────────────┘
```

### Workflow Triggers

| Workflow | Trigger | Runs On | Duration |
|----------|---------|---------|----------|
| ci.yml | PR, push to main | Ubuntu, macOS, Windows | ~10 min |
| coverage.yml | PR, push to main | Ubuntu | ~8 min |
| sanitizers.yml | PR, push to main | Ubuntu | ~15 min |
| static-analysis.yml | PR | Ubuntu | ~12 min |
| integration-tests.yml | PR, push to main | Ubuntu | ~20 min |
| benchmarks.yml | Push to main | Ubuntu | ~25 min |
| build-Doxygen.yaml | Push to main | Ubuntu | ~5 min |

---

## GitHub Actions Workflows

### 1. Main CI Workflow (`ci.yml`)

**Purpose:** Build and run unit tests on multiple platforms

**Configuration:**
```yaml
# .github/workflows/ci.yml
name: CI

on:
  pull_request:
  push:
    branches: [main]

jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
        compiler: [gcc-11, clang-14, msvc]
        build_type: [Debug, Release]
```

**What It Tests:**
- ✅ Compilation on Linux, macOS, Windows
- ✅ Unit tests (Google Test)
- ✅ CMake configuration
- ✅ Dependency resolution
- ✅ Installation targets

**Local Reproduction:**
```bash
# Ubuntu/Linux
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build
ctest --test-dir build

# Windows
cmake -B build -G "Visual Studio 16 2019"
cmake --build build --config Release
ctest --test-dir build -C Release
```

---

### 2. Code Coverage Workflow (`coverage.yml`)

**Purpose:** Measure test coverage and upload to Codecov

**Configuration:**
```yaml
# .github/workflows/coverage.yml
jobs:
  coverage:
    runs-on: ubuntu-latest
    steps:
      - name: Configure with coverage
        run: |
          cmake -B build \
            -DCMAKE_BUILD_TYPE=Debug \
            -DENABLE_COVERAGE=ON \
            -DBUILD_TESTS=ON
```

**Coverage Targets:**
- ✅ Line coverage: >85%
- ✅ Branch coverage: >75%
- ✅ Function coverage: >90%

**Local Coverage Generation:**
```bash
# Install gcov/lcov
sudo apt-get install gcov lcov

# Configure with coverage
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_COVERAGE=ON \
  -DCMAKE_CXX_FLAGS="--coverage -fprofile-arcs -ftest-coverage"

# Build and run tests
cmake --build build
ctest --test-dir build

# Generate coverage report
lcov --capture --directory build --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage_filtered.info
genhtml coverage_filtered.info --output-directory coverage_html

# View report
xdg-open coverage_html/index.html  # Linux
open coverage_html/index.html      # macOS
```

**Coverage Exclusions:**
```cpp
// LCOV_EXCL_START - Exclude from coverage
if (unlikely_error_condition) {
    // Defensive code not covered by tests
}
// LCOV_EXCL_STOP
```

---

### 3. Sanitizers Workflow (`sanitizers.yml`)

**Purpose:** Detect memory errors, data races, and undefined behavior

**Sanitizers Run:**

| Sanitizer | Detects | Overhead |
|-----------|---------|----------|
| **AddressSanitizer (ASan)** | Memory leaks, use-after-free, buffer overflows | ~2x |
| **ThreadSanitizer (TSan)** | Data races, deadlocks | ~5-15x |
| **UndefinedBehaviorSanitizer (UBSan)** | Integer overflow, null dereference, misaligned access | ~1.2x |
| **LeakSanitizer (LSan)** | Memory leaks | Minimal |

**Local Execution:**

**AddressSanitizer:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g"

cmake --build build
ASAN_OPTIONS=detect_leaks=1:check_initialization_order=1 \
  ./build/tests/monitoring_system_tests
```

**ThreadSanitizer:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -g"

cmake --build build
TSAN_OPTIONS=second_deadlock_stack=1:history_size=7 \
  ./build/tests/monitoring_system_tests
```

**UndefinedBehaviorSanitizer:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-sanitize-recover=all -g"

cmake --build build
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  ./build/tests/monitoring_system_tests
```

**Suppression Files:**

For known false positives:
```bash
# tsan.supp
race:^third_party/some_library.*

# asan.supp
leak:^third_party/some_library.*
```

```bash
TSAN_OPTIONS=suppressions=tsan.supp ./tests
```

---

### 4. Static Analysis Workflow (`static-analysis.yml`)

**Purpose:** Catch bugs and style issues without running code

**Tools Used:**

**Clang-Tidy:**
```bash
# Install
sudo apt-get install clang-tidy-14

# Run
cmake -B build \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_CXX_CLANG_TIDY="clang-tidy-14;-checks=*"

cmake --build build
```

**Configuration (`.clang-tidy`):**
```yaml
Checks: >
  -*,
  bugprone-*,
  cert-*,
  clang-analyzer-*,
  concurrency-*,
  cppcoreguidelines-*,
  modernize-*,
  performance-*,
  readability-*,
  -modernize-use-trailing-return-type
WarningsAsErrors: ''
CheckOptions:
  - key: readability-identifier-naming.ClassCase
    value: CamelCase
  - key: readability-identifier-naming.FunctionCase
    value: lower_case
  - key: readability-identifier-naming.VariableCase
    value: lower_case
```

**Cppcheck:**
```bash
# Install
sudo apt-get install cppcheck

# Run
cppcheck --enable=all \
  --suppress=missingIncludeSystem \
  --std=c++20 \
  --error-exitcode=1 \
  -I include/ \
  src/
```

**Local Static Analysis:**
```bash
# Full analysis
./scripts/run_static_analysis.sh

# Or manually:
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p build src/*.cpp include/*.hpp
cppcheck --project=build/compile_commands.json
```

---

### 5. Integration Tests Workflow (`integration-tests.yml`)

**Purpose:** Test system components working together

**Test Scenarios:**
- ✅ Multi-threaded metric collection
- ✅ Distributed tracing across boundaries
- ✅ Alert rule evaluation and notification
- ✅ Storage backend persistence
- ✅ Dashboard API endpoints

**Local Execution:**
```bash
# Build with integration tests
cmake -B build \
  -DBUILD_INTEGRATION_TESTS=ON \
  -DUSE_THREAD_SYSTEM=ON \
  -DBUILD_WITH_LOGGER_SYSTEM=ON

cmake --build build

# Run integration tests
./build/integration_tests/monitoring_integration_tests

# Run specific suite
./build/integration_tests/monitoring_integration_tests \
  --gtest_filter="AlertingIntegration.*"

# Run with verbose output
./build/integration_tests/monitoring_integration_tests --verbose
```

**Environment Variables:**
```bash
# Configure test environment
export MONITORING_TEST_DB_PATH=/tmp/test_metrics.db
export MONITORING_TEST_ALERT_ENDPOINT=http://localhost:9093
export MONITORING_LOG_LEVEL=DEBUG

./build/integration_tests/monitoring_integration_tests
```

---

### 6. Benchmarks Workflow (`benchmarks.yml`)

**Purpose:** Measure and track performance over time

**Benchmarks Run:**
- Metric recording throughput
- Query latency
- Storage backend performance
- Tracing overhead
- Alert evaluation speed

**Local Benchmark Execution:**
```bash
# Build with benchmarks
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_BENCHMARKS=ON

cmake --build build

# Run all benchmarks
./build/benchmarks/monitoring_benchmarks

# Run specific benchmark
./build/benchmarks/monitoring_benchmarks \
  --benchmark_filter="BM_RecordCounter"

# Save results for comparison
./build/benchmarks/monitoring_benchmarks \
  --benchmark_out=results.json \
  --benchmark_out_format=json

# Compare with baseline
./scripts/compare_benchmarks.py \
  --baseline=baseline.json \
  --current=results.json
```

**Benchmark Options:**
```bash
# Run for specific duration
--benchmark_min_time=5.0

# Control iterations
--benchmark_repetitions=10

# Filter by regex
--benchmark_filter="BM_Record.*"

# Output formats: json, csv, console
--benchmark_out_format=json
```

---

### 7. Documentation Build (`build-Doxygen.yaml`)

**Purpose:** Generate API documentation with Doxygen

**Local Documentation Build:**
```bash
# Install Doxygen
sudo apt-get install doxygen graphviz

# Generate docs
cmake -B build -DBUILD_DOCS=ON
cmake --build build --target docs

# View documentation
xdg-open build/docs/html/index.html  # Linux
open build/docs/html/index.html      # macOS
```

**Doxygen Configuration:**
```
# Doxyfile
PROJECT_NAME = "Monitoring System"
OUTPUT_DIRECTORY = docs
GENERATE_HTML = YES
GENERATE_LATEX = NO
EXTRACT_ALL = YES
EXTRACT_PRIVATE = NO
```

---

## Building Locally

### Standard Build

```bash
# Clone repository
git clone https://github.com/kcenon/monitoring_system.git
cd monitoring_system

# Configure
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON

# Build
cmake --build build --parallel $(nproc)

# Install (optional)
sudo cmake --install build
```

### Development Build

```bash
# Debug build with all development features
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTS=ON \
  -DBUILD_BENCHMARKS=ON \
  -DBUILD_INTEGRATION_TESTS=ON \
  -DENABLE_COVERAGE=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build build
```

### Platform-Specific Builds

**Linux:**
```bash
# GCC
cmake -B build -DCMAKE_CXX_COMPILER=g++-11
cmake --build build

# Clang
cmake -B build -DCMAKE_CXX_COMPILER=clang++-14
cmake --build build
```

**macOS:**
```bash
# Apple Clang
cmake -B build
cmake --build build

# Homebrew GCC
cmake -B build -DCMAKE_CXX_COMPILER=g++-13
cmake --build build
```

**Windows:**
```bash
# Visual Studio
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# MinGW
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

---

## Running Tests

### Unit Tests

```bash
# Run all tests
ctest --test-dir build --output-on-failure

# Run with parallel execution
ctest --test-dir build --parallel 4

# Run specific test
./build/tests/monitoring_system_tests \
  --gtest_filter="MetricsCollectorTest.RecordCounter"

# List all tests
./build/tests/monitoring_system_tests --gtest_list_tests

# Run with verbose output
./build/tests/monitoring_system_tests --verbose
```

### Test Categories

```bash
# Metrics tests
./build/tests/monitoring_system_tests \
  --gtest_filter="Metrics*"

# Alerting tests
./build/tests/monitoring_system_tests \
  --gtest_filter="Alert*"

# Tracing tests
./build/tests/monitoring_system_tests \
  --gtest_filter="Trac*"

# Storage tests
./build/tests/monitoring_system_tests \
  --gtest_filter="Storage*"
```

### Test Options

```bash
# Repeat tests
--gtest_repeat=100

# Shuffle test order
--gtest_shuffle

# Run until failure
--gtest_repeat=-1 --gtest_break_on_failure

# Generate XML report
--gtest_output=xml:test_results.xml
```

---

## Code Coverage

### Generate Coverage Report

```bash
# 1. Build with coverage
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_COVERAGE=ON \
  -DCMAKE_CXX_FLAGS="--coverage"

# 2. Run tests
cmake --build build
ctest --test-dir build

# 3. Generate report
lcov --capture --directory build \
  --output-file coverage.info

lcov --remove coverage.info \
  '/usr/*' '*/tests/*' '*/third_party/*' \
  --output-file coverage_filtered.info

# 4. Generate HTML
genhtml coverage_filtered.info \
  --output-directory coverage_html

# 5. View
xdg-open coverage_html/index.html
```

### Coverage Thresholds

Minimum required coverage:
- **Line Coverage:** 85%
- **Function Coverage:** 90%
- **Branch Coverage:** 75%

```bash
# Check thresholds
lcov --summary coverage_filtered.info

# Fail if below threshold
lcov --summary coverage_filtered.info | \
  grep -q "lines......: [89][0-9].[0-9]%" || exit 1
```

---

## Sanitizers

### Running All Sanitizers

```bash
#!/bin/bash
# run_sanitizers.sh

sanitizers=("address" "thread" "undefined" "leak")

for san in "${sanitizers[@]}"; do
    echo "Running ${san}Sanitizer..."

    cmake -B build_${san} \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=${san} -fno-omit-frame-pointer -g"

    cmake --build build_${san}

    if ! ./build_${san}/tests/monitoring_system_tests; then
        echo "❌ ${san}Sanitizer found issues"
        exit 1
    fi

    echo "✅ ${san}Sanitizer passed"
done
```

### Sanitizer Best Practices

**Common Suppressions:**
```bash
# tsan.supp
# Suppress third-party library data races
race:^third_party/.*

# asan.supp
# Suppress known leaks in system libraries
leak:^_ZN.*libpthread.*
```

**Environment Variables:**
```bash
# ASan
export ASAN_OPTIONS=\
"detect_leaks=1:\
check_initialization_order=1:\
strict_init_order=1:\
detect_stack_use_after_return=1"

# TSan
export TSAN_OPTIONS=\
"second_deadlock_stack=1:\
history_size=7:\
report_bugs=1"

# UBSan
export UBSAN_OPTIONS=\
"print_stacktrace=1:\
halt_on_error=1"
```

---

## Static Analysis

### Clang-Tidy Checks

```bash
# Run on all source files
clang-tidy -p build/compile_commands.json \
  src/*.cpp include/**/*.hpp

# Apply fixes automatically
clang-tidy -p build \
  --fix \
  --fix-errors \
  src/*.cpp

# Check specific file
clang-tidy -p build \
  src/monitoring_system.cpp
```

### Custom Checks

Add to `.clang-tidy`:
```yaml
Checks: >
  bugprone-*,
  cert-*,
  clang-analyzer-*,
  concurrency-*,
  cppcoreguidelines-*,
  modernize-*,
  performance-*,
  readability-*

CheckOptions:
  - key: readability-function-cognitive-complexity.Threshold
    value: 25
  - key: cppcoreguidelines-avoid-magic-numbers.IgnoredIntegerValues
    value: '0;1;2;3;4;5;'
```

---

## Benchmarks

### Running Benchmarks

```bash
# Build benchmarks
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_BENCHMARKS=ON

cmake --build build

# Run all benchmarks
./build/benchmarks/monitoring_benchmarks

# Run specific category
./build/benchmarks/monitoring_benchmarks \
  --benchmark_filter="BM_Metrics.*"
```

### Benchmark Comparison

```bash
# Save baseline
./build/benchmarks/monitoring_benchmarks \
  --benchmark_out=baseline.json \
  --benchmark_out_format=json

# Make changes, rebuild, run again
./build/benchmarks/monitoring_benchmarks \
  --benchmark_out=current.json \
  --benchmark_out_format=json

# Compare (requires Google Benchmark compare.py)
python3 third_party/benchmark/tools/compare.py \
  benchmarks baseline.json current.json
```

### Interpreting Results

```
Benchmark                Time    CPU  Iterations
---------------------------------------------------
BM_RecordCounter       80 ns   80 ns  8750000
BM_RecordGauge         85 ns   85 ns  8235294
BM_RecordHistogram    120 ns  120 ns  5833333
```

- **Time:** Wall-clock time
- **CPU:** CPU time (excludes I/O wait)
- **Iterations:** Number of times benchmark ran

---

## Integration Tests

### Test Environment Setup

```bash
# Set up test databases
mkdir -p /tmp/monitoring_test
export MONITORING_TEST_PATH=/tmp/monitoring_test

# Start test services (if needed)
docker-compose -f docker/test-compose.yml up -d
```

### Running Integration Tests

```bash
# Build
cmake -B build -DBUILD_INTEGRATION_TESTS=ON
cmake --build build

# Run
./build/integration_tests/monitoring_integration_tests

# Clean up
docker-compose -f docker/test-compose.yml down
rm -rf /tmp/monitoring_test
```

---

## Release Process

### Version Numbering

**Semantic Versioning:** MAJOR.MINOR.PATCH

- **MAJOR:** Breaking changes
- **MINOR:** New features (backward compatible)
- **PATCH:** Bug fixes

### Creating a Release

```bash
# 1. Update version
vim include/monitoring_system/version.hpp
# Update VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH

# 2. Update CHANGELOG.md
vim CHANGELOG.md
# Add release notes

# 3. Commit version bump
git add -A
git commit -m "chore: bump version to v1.2.0"

# 4. Create tag
git tag -a v1.2.0 -m "Release v1.2.0"

# 5. Push tag
git push origin v1.2.0

# 6. GitHub Actions automatically creates release
```

### Release Checklist

- [ ] All CI checks passing
- [ ] Benchmarks within acceptable range
- [ ] Documentation updated
- [ ] CHANGELOG.md updated
- [ ] Version numbers updated
- [ ] Migration guide (if needed)
- [ ] Release notes prepared

---

## Troubleshooting CI

### Common CI Failures

**Build Failure:**
```bash
# Reproduce locally
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --verbose
```

**Test Failure:**
```bash
# Run failing test locally
./build/tests/monitoring_system_tests \
  --gtest_filter="FailingTest.*" \
  --verbose
```

**Sanitizer Failure:**
```bash
# Run with same sanitizer
cmake -B build -DCMAKE_CXX_FLAGS="-fsanitize=address"
cmake --build build
./build/tests/monitoring_system_tests
```

**Coverage Drop:**
```bash
# Check what's uncovered
lcov --summary coverage.info
genhtml coverage.info -o coverage_html
# Open coverage_html/index.html
```

### CI Logs

Access CI logs:
1. Go to GitHub Actions tab
2. Select failing workflow run
3. Click on failed job
4. Expand failed step
5. Download logs if needed

---

## Additional Resources

### Scripts

| Script | Purpose |
|--------|---------|
| `scripts/run_all_checks.sh` | Run all local checks |
| `scripts/run_sanitizers.sh` | Run all sanitizers |
| `scripts/generate_coverage.sh` | Generate coverage report |
| `scripts/run_benchmarks.sh` | Run and compare benchmarks |

### Documentation

- **[CONTRIBUTING.md](../CONTRIBUTING.md)** - Contribution guidelines
- **[Testing Guide](../TESTING_GUIDE.md)** - Detailed testing documentation
- **[Troubleshooting](../TROUBLESHOOTING.md)** - Common issues

### Support

- **GitHub Issues**: [Report CI problems](https://github.com/kcenon/monitoring_system/issues)
- **GitHub Discussions**: [Ask questions](https://github.com/kcenon/monitoring_system/discussions)

---

**Last Updated:** 2025-11-11
**Next Review:** 2026-02-11

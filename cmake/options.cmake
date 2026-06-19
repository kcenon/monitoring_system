# options.cmake
# All option(...) declarations and feature toggles for monitoring_system.
#
# This file declares all build-time options used by the monitoring_system build.
# It is included before dependency detection so that BUILD_WITH_* toggles are
# available when the dependencies module evaluates them.

##################################################
# C++20 Module Support (Experimental)
##################################################
# C++20 modules require CMake 3.28+ and a compatible compiler.
# This is an opt-in feature for the transition period.
#
# To enable modules:
#   cmake -DMONITORING_ENABLE_MODULES=ON ..
#
# Requirements:
#   - CMake 3.28 or later
#   - GCC 14+, Clang 16+, or MSVC 19.34+
#
# See: https://github.com/kcenon/monitoring_system/issues/310
##################################################
option(MONITORING_ENABLE_MODULES "Enable C++20 module support (requires CMake 3.28+)" OFF)

if(MONITORING_ENABLE_MODULES)
    if(CMAKE_VERSION VERSION_LESS "3.28")
        message(FATAL_ERROR
            "C++20 modules require CMake 3.28 or later.\n"
            "Current CMake version: ${CMAKE_VERSION}\n"
            "Either upgrade CMake or disable modules with -DMONITORING_ENABLE_MODULES=OFF")
    endif()

    # Enable module scanning
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)
    message(STATUS "Monitoring System: C++20 modules ENABLED")
else()
    message(STATUS "Monitoring System: C++20 modules DISABLED (header-based build)")
endif()

# Build outputs
option(MONITORING_BUILD_TESTS "Build unit tests" ON)
option(MONITORING_BUILD_INTEGRATION_TESTS "Build integration tests" ON)
option(MONITORING_BUILD_EXAMPLES "Build example programs" ON)
option(MONITORING_BUILD_BENCHMARKS "Build benchmarks" OFF)

##################################################
# Dependency Configuration (Option A Structure)
#
# Tier 3: monitoring_system
#   Required: thread_system <- common_system
#   Optional: logger_system (runtime-bound via ILogger interface)
#   Optional: network_system (HTTP metrics export)
#
# Note: logger_system is now optional because monitoring_system uses
# common_system's ILogger interface for runtime binding.
# See Issue #213 for migration details.
##################################################

# common_system (mandatory when ON)
option(MONITORING_WITH_COMMON_SYSTEM "Enable common_system integration" ON)

# Required dependencies (Tier 2 and below)
option(MONITORING_WITH_THREAD_SYSTEM "Enable thread_system integration (REQUIRED)" ON)
option(MONITORING_WITH_LOGGER_SYSTEM "Enable logger_system integration (OPTIONAL - runtime binding via ILogger)" OFF)

# Optional integration (Tier 4 - kept optional to prevent circular dependency)
option(MONITORING_WITH_NETWORK_SYSTEM "Enable network_system integration for HTTP transport (OPTIONAL)" OFF)

# Optional gRPC support for OTLP trace export
option(MONITORING_WITH_GRPC "Enable gRPC support for OTLP trace export (OPTIONAL)" OFF)

# Sanitizer/coverage toggles
option(MONITORING_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(MONITORING_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(MONITORING_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(MONITORING_ENABLE_COVERAGE "Enable coverage reporting" OFF)

# Optional hardware monitoring plugin (battery, power, temperature, GPU)
# Disabled by default for server environments
option(MONITORING_BUILD_HARDWARE_PLUGIN "Build hardware monitoring plugin" OFF)

# Optional container monitoring plugin (Docker, Kubernetes, cgroups)
# Disabled by default for non-containerized environments
option(MONITORING_BUILD_CONTAINER_PLUGIN "Build container monitoring plugin" OFF)

# Optional integration notice (network_system)
if(MONITORING_WITH_NETWORK_SYSTEM)
    message(STATUS "MONITORING_WITH_NETWORK_SYSTEM is enabled - HTTP metrics export available")
    message(STATUS "Note: This creates an optional cross-tier dependency (Tier 3 -> Tier 4)")
endif()

# Note: Required dependency validation is performed during detection phase.
# If dependencies are not found, they will be automatically disabled with a warning.

# Export compile commands for tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MonitoringLegacyOptions.cmake
# Backward compatibility for legacy CMake option names
#
# This module handles migration from old option names to new MONITORING_* prefixed options.
# It can be safely removed once all dependent projects have migrated.

##################################################
# Legacy Option Mappings
##################################################

# BUILD_TESTS -> MONITORING_BUILD_TESTS
if(DEFINED BUILD_TESTS)
    message(DEPRECATION "BUILD_TESTS is deprecated. Use MONITORING_BUILD_TESTS instead.")
    if(BUILD_TESTS)
        set(MONITORING_BUILD_TESTS ON CACHE BOOL "Build unit tests" FORCE)
    else()
        set(MONITORING_BUILD_TESTS OFF CACHE BOOL "Build unit tests" FORCE)
    endif()
endif()

# BUILD_INTEGRATION_TESTS -> MONITORING_BUILD_INTEGRATION_TESTS
if(DEFINED BUILD_INTEGRATION_TESTS)
    message(DEPRECATION "BUILD_INTEGRATION_TESTS is deprecated. Use MONITORING_BUILD_INTEGRATION_TESTS instead.")
    if(BUILD_INTEGRATION_TESTS)
        set(MONITORING_BUILD_INTEGRATION_TESTS ON CACHE BOOL "Build integration tests" FORCE)
    else()
        set(MONITORING_BUILD_INTEGRATION_TESTS OFF CACHE BOOL "Build integration tests" FORCE)
    endif()
endif()

# BUILD_EXAMPLES -> MONITORING_BUILD_EXAMPLES
if(DEFINED MONITORING_FORCE_DISABLE_EXAMPLES AND MONITORING_FORCE_DISABLE_EXAMPLES)
    set(MONITORING_BUILD_EXAMPLES OFF CACHE BOOL "Build example programs" FORCE)
elseif(DEFINED BUILD_EXAMPLES)
    message(DEPRECATION "BUILD_EXAMPLES is deprecated. Use MONITORING_BUILD_EXAMPLES instead.")
    if(BUILD_EXAMPLES)
        set(MONITORING_BUILD_EXAMPLES ON CACHE BOOL "Build example programs" FORCE)
    else()
        set(MONITORING_BUILD_EXAMPLES OFF CACHE BOOL "Build example programs" FORCE)
    endif()
endif()

# BUILD_WITH_COMMON_SYSTEM -> MONITORING_WITH_COMMON_SYSTEM
# Note: Source code now uses KCENON_HAS_COMMON_SYSTEM for feature detection.
# The CMake option MONITORING_WITH_COMMON_SYSTEM controls whether to enable the integration,
# and KCENON_HAS_COMMON_SYSTEM=1 is defined as a compile definition when enabled.
if(DEFINED BUILD_WITH_COMMON_SYSTEM AND NOT DEFINED MONITORING_WITH_COMMON_SYSTEM)
    message(DEPRECATION "BUILD_WITH_COMMON_SYSTEM is deprecated. Use MONITORING_WITH_COMMON_SYSTEM instead. "
                        "Source code now uses KCENON_HAS_COMMON_SYSTEM for feature detection.")
    set(MONITORING_WITH_COMMON_SYSTEM ${BUILD_WITH_COMMON_SYSTEM} CACHE BOOL "Enable common_system integration" FORCE)
endif()

# ENABLE_ASAN -> MONITORING_ENABLE_ASAN
if(DEFINED ENABLE_ASAN AND NOT DEFINED MONITORING_ENABLE_ASAN)
    message(DEPRECATION "ENABLE_ASAN is deprecated. Use MONITORING_ENABLE_ASAN instead.")
    set(MONITORING_ENABLE_ASAN ${ENABLE_ASAN} CACHE BOOL "Enable AddressSanitizer" FORCE)
endif()

# ENABLE_TSAN -> MONITORING_ENABLE_TSAN
if(DEFINED ENABLE_TSAN AND NOT DEFINED MONITORING_ENABLE_TSAN)
    message(DEPRECATION "ENABLE_TSAN is deprecated. Use MONITORING_ENABLE_TSAN instead.")
    set(MONITORING_ENABLE_TSAN ${ENABLE_TSAN} CACHE BOOL "Enable ThreadSanitizer" FORCE)
endif()

# ENABLE_UBSAN -> MONITORING_ENABLE_UBSAN
if(DEFINED ENABLE_UBSAN AND NOT DEFINED MONITORING_ENABLE_UBSAN)
    message(DEPRECATION "ENABLE_UBSAN is deprecated. Use MONITORING_ENABLE_UBSAN instead.")
    set(MONITORING_ENABLE_UBSAN ${ENABLE_UBSAN} CACHE BOOL "Enable UndefinedBehaviorSanitizer" FORCE)
endif()

# ENABLE_COVERAGE -> MONITORING_ENABLE_COVERAGE
if(DEFINED ENABLE_COVERAGE AND NOT DEFINED MONITORING_ENABLE_COVERAGE)
    message(DEPRECATION "ENABLE_COVERAGE is deprecated. Use MONITORING_ENABLE_COVERAGE instead.")
    set(MONITORING_ENABLE_COVERAGE ${ENABLE_COVERAGE} CACHE BOOL "Enable coverage reporting" FORCE)
endif()

# compile_options.cmake
# Compile options, sanitizer flags, and SIMD detection for monitoring_system.
#
# This module defines target-property setup for the monitoring_system_interface
# library. The interface library carries compile options, definitions, and
# sanitizer link options outward to all consumers (monitoring_system,
# monitoring_system_modules, plugins, etc.).
#
# Helper functions exposed for downstream targets:
#   - apply_monitoring_simd_definitions(<target>)
#       Apply SIMD compile definitions (SIMD_AVX2_AVAILABLE / SIMD_NEON_AVAILABLE)
#       based on CMAKE_SYSTEM_PROCESSOR. The compile options are applied through
#       the interface library; this helper exists for any target that needs the
#       definitions independently.

##################################################
# monitoring_system_interface library
##################################################
# Interface library carrying compile options, include dirs, and definitions
# that propagate to all consumers.

add_library(monitoring_system_interface INTERFACE)
target_include_directories(monitoring_system_interface
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sources>
        $<INSTALL_INTERFACE:include>
)

# common_system integration is now mandatory (Phase 2.3)
message(STATUS "Monitoring System: Using common_system interfaces")

# Find common_system target with multiple possible names
# Priority: namespaced alias first, then direct target name
# This handles both find_package() and FetchContent scenarios
set(_COMMON_SYSTEM_TARGET "")
foreach(_candidate kcenon::common_system kcenon::common common_system common)
    if(TARGET ${_candidate})
        set(_COMMON_SYSTEM_TARGET ${_candidate})
        message(STATUS "Found common_system target: ${_candidate}")
        break()
    endif()
endforeach()

# Add common_system dependency
if(_COMMON_SYSTEM_TARGET)
    target_link_libraries(monitoring_system_interface INTERFACE ${_COMMON_SYSTEM_TARGET})
elseif(TARGET common_system AND NOT TARGET kcenon::common_system)
    # Create alias if the base target exists but namespaced alias doesn't
    # This can happen when common_system is added via FetchContent
    add_library(kcenon::common_system ALIAS common_system)
    target_link_libraries(monitoring_system_interface INTERFACE kcenon::common_system)
    message(STATUS "Created kcenon::common_system alias for common_system target")
elseif(DEFINED COMMON_SYSTEM_INCLUDE_DIR)
    # Fallback to header-only integration using include directories
    target_include_directories(monitoring_system_interface
        INTERFACE
            $<BUILD_INTERFACE:${COMMON_SYSTEM_INCLUDE_DIR}>
    )
    message(STATUS "Using common_system headers from: ${COMMON_SYSTEM_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "common_system is required but no suitable target or include directory was found.")
endif()

target_compile_definitions(monitoring_system_interface
    INTERFACE
        KCENON_HAS_COMMON_SYSTEM=1
        MONITORING_USING_COMMON_INTERFACES
)

# Set compile features
target_compile_features(monitoring_system_interface INTERFACE cxx_std_20)

# Add compile options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(monitoring_system_interface INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wdeprecated-declarations
        -Werror=return-type
        -Werror=uninitialized
        -Werror=unused-result
    )

    # Add SIMD support based on architecture
    include(CheckCXXCompilerFlag)

    # Detect architecture - only enable AVX2 on x86/x64
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i[3-6]86")
        check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
        if(COMPILER_SUPPORTS_AVX2)
            target_compile_options(monitoring_system_interface INTERFACE -mavx2)
            target_compile_definitions(monitoring_system_interface INTERFACE SIMD_AVX2_AVAILABLE)
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        # ARM64 uses NEON, which is enabled by default on AArch64
        target_compile_definitions(monitoring_system_interface INTERFACE SIMD_NEON_AVAILABLE)
    endif()
elseif(MSVC)
    target_compile_options(monitoring_system_interface INTERFACE
        /W4
        /WX
        /permissive-
        /Zc:__cplusplus
        /w14996  # Enable deprecated declarations warning
    )

    # Enable AVX2 on MSVC for x64 architecture only
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
        target_compile_options(monitoring_system_interface INTERFACE /arch:AVX2)
        target_compile_definitions(monitoring_system_interface INTERFACE SIMD_AVX2_AVAILABLE)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64")
        target_compile_definitions(monitoring_system_interface INTERFACE SIMD_NEON_AVAILABLE)
    endif()
endif()

##################################################
# Sanitizers
##################################################
# Sanitizer flags propagate via the interface library so consumers get the
# same instrumentation. These remain attached to monitoring_system_interface
# rather than being applied per-target, preserving the historical behaviour.

if(MONITORING_ENABLE_ASAN)
    target_compile_options(monitoring_system_interface INTERFACE -fsanitize=address)
    target_link_options(monitoring_system_interface INTERFACE -fsanitize=address)
endif()

if(MONITORING_ENABLE_TSAN)
    target_compile_options(monitoring_system_interface INTERFACE -fsanitize=thread)
    target_link_options(monitoring_system_interface INTERFACE -fsanitize=thread)
endif()

if(MONITORING_ENABLE_UBSAN)
    target_compile_options(monitoring_system_interface INTERFACE -fsanitize=undefined)
    target_link_options(monitoring_system_interface INTERFACE -fsanitize=undefined)
endif()

##################################################
# Helper Functions
##################################################

# Apply SIMD compile definitions to a target.
# The compile options (-mavx2, /arch:AVX2) and the same definitions are already
# carried by monitoring_system_interface; this helper exists for direct use on
# targets that don't link the interface library transitively.
function(apply_monitoring_simd_definitions TARGET_NAME)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i[3-6]86")
        target_compile_definitions(${TARGET_NAME} PRIVATE SIMD_AVX2_AVAILABLE)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        target_compile_definitions(${TARGET_NAME} PRIVATE SIMD_NEON_AVAILABLE)
    endif()
endfunction()

# kcenon-logger-system portfile
# High-performance C++20 async logging library with 4.34M msg/sec throughput

# Upstream does not annotate symbols with __declspec(dllexport), so DLLs are
# built without exports on Windows.  Force static linkage on all platforms.
set(VCPKG_LIBRARY_LINKAGE static)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/logger_system
    REF v0.1.2
    SHA512 668c03f997368831207c5865c1615b774a28a08bcd297e09a2bd7e207bf3e6024eab8ba25516dd16ad5b20c7156cdfb8a240770884a7fc4f17c1517f50019ba1
    HEAD_REF main
    PATCHES
        fix-unified-deps-target-names.patch
)

# Disable thread_system integration: upstream CMake does not link thread_system
# library properly in vcpkg mode (unresolved externals for thread_pool symbols).
# The logger falls back to its standalone executor which works correctly.
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTS=OFF
        -DBUILD_BENCHMARKS=OFF
        -DBUILD_SAMPLES=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DLOGGER_BUILD_INTEGRATION_TESTS=OFF
        -DLOGGER_ENABLE_COVERAGE=OFF
        -DLOGGER_USE_THREAD_SYSTEM=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME LoggerSystem
    CONFIG_PATH lib/cmake/LoggerSystem
)

# v0.1.2: upstream now emits install(EXPORT LoggerSystemTargets) when
# LOGGER_USE_THREAD_SYSTEM=OFF, so manual target generation is no longer needed.

# Fix include paths: upstream headers use kcenon/logger/ but vcpkg installs under logger_system/
file(GLOB_RECURSE _logger_headers "${CURRENT_PACKAGES_DIR}/include/logger_system/*.h")
foreach(_header IN LISTS _logger_headers)
    file(READ "${_header}" _content)
    if(_content MATCHES "kcenon/logger/")
        string(REPLACE "kcenon/logger/" "logger_system/" _content "${_content}")
        file(WRITE "${_header}" "${_content}")
    endif()
endforeach()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

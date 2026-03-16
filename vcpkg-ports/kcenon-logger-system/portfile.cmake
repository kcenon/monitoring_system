# kcenon-logger-system portfile
# High-performance C++20 async logging library with 4.34M msg/sec throughput

# Upstream does not annotate symbols with __declspec(dllexport), so DLLs are
# built without exports on Windows.  Force static linkage on all platforms.
set(VCPKG_LIBRARY_LINKAGE static)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/logger_system
    REF 99056eadee00ab33a4663e1663bdf1f3eacc8f3b
    SHA512 8d2fe38089dd760baaff764664097c5a7c9c6f4fe8f0048d4c5e6e6451a5f615c2ac20b91879235c81f08b8ff4efebb505dd8884692fc4a5c2a52676e5351b11
    HEAD_REF main
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
    PACKAGE_NAME logger_system
    CONFIG_PATH lib/cmake/logger_system
)

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

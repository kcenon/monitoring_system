# kcenon-logger-system portfile
# High-performance C++20 async logging library with 4.34M msg/sec throughput

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/logger_system
    REF v0.1.1
    SHA512 96df6c6213e618052999301de4f3b4a1aa5ccac42148ea1f02c408f68f38e13620c78b98ebca7abcf7d706907debf4e95766d4fca3b53d848cc90fd0c6e3ba61
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTS=OFF
        -DBUILD_BENCHMARKS=OFF
        -DBUILD_SAMPLES=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DLOGGER_BUILD_INTEGRATION_TESTS=OFF
        -DLOGGER_ENABLE_COVERAGE=OFF
        -DLOGGER_USE_THREAD_SYSTEM=ON
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME LoggerSystem
    CONFIG_PATH lib/cmake/LoggerSystem
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

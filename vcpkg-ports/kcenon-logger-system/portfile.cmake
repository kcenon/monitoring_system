# kcenon-logger-system portfile
# High-performance C++20 async logging library with 4.34M msg/sec throughput

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/logger_system
    REF "v${VERSION}"
    SHA512 90884ec4210e6ebeb534aed2eb9928cfc2a80d0ab7220f9ed9be63d32bcfc6154e6bd2960c7d11cb196c3dcb345650c6e182f8edc44867b4bcf0efeb1dfed0f2
    HEAD_REF main
)

# Enable thread_system integration: logger_system uses thread_system's optimized
# thread pool (work stealing, lock-free queues) for async log dispatch.
# Requires kcenon-thread-system to be installed (declared in vcpkg.json deps).
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTS=OFF
        -DBUILD_BENCHMARKS=OFF
        -DBUILD_SAMPLES=OFF
        -DLOGGER_BUILD_INTEGRATION_TESTS=OFF
        -DLOGGER_ENABLE_COVERAGE=OFF
        -DLOGGER_USE_THREAD_SYSTEM=ON
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME logger_system
    CONFIG_PATH lib/cmake/logger_system
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

# kcenon-monitoring-system portfile
# High-performance C++20 monitoring system with metrics, tracing, and alerting

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/monitoring_system
    REF 2d8802c2980b6b99acffc812fc588c377bedefa4
    SHA512 0  # TODO: Update with actual SHA512 hash
    HEAD_REF main
)

# Feature-based options
set(MONITORING_WITH_LOGGER OFF)
if("logging" IN_LIST FEATURES)
    set(MONITORING_WITH_LOGGER ON)
endif()

set(MONITORING_WITH_NETWORK OFF)
if("network" IN_LIST FEATURES)
    set(MONITORING_WITH_NETWORK ON)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DMONITORING_BUILD_TESTS=OFF
        -DMONITORING_BUILD_INTEGRATION_TESTS=OFF
        -DMONITORING_BUILD_EXAMPLES=OFF
        -DMONITORING_BUILD_BENCHMARKS=OFF
        -DMONITORING_WITH_COMMON_SYSTEM=ON
        -DMONITORING_WITH_THREAD_SYSTEM=ON
        -DMONITORING_WITH_LOGGER_SYSTEM=${MONITORING_WITH_LOGGER}
        -DMONITORING_WITH_NETWORK_SYSTEM=${MONITORING_WITH_NETWORK}
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME monitoring_system
    CONFIG_PATH lib/cmake/monitoring_system
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

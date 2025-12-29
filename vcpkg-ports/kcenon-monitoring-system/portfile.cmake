# kcenon-monitoring-system portfile
# High-performance C++20 monitoring system

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/monitoring_system
    REF "${VERSION}"
    SHA512 0  # TODO: Update with actual SHA512 hash after release
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTING=OFF
        -DBUILD_BENCHMARKS=OFF
        -DBUILD_EXAMPLES=OFF
    MAYBE_UNUSED_VARIABLES
        BUILD_TESTING
        BUILD_BENCHMARKS
        BUILD_EXAMPLES
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME monitoring_system
    CONFIG_PATH lib/cmake/monitoring_system
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

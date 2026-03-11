# kcenon-container-system portfile
# Advanced C++20 Container System with Thread-Safe Operations and Messaging Integration

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/container_system
    REF 123d7db3e523167de85f707600df0884e35a871f
    SHA512 0  # TODO: Update with actual SHA512 hash
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_WITH_COMMON_SYSTEM=ON
        -DBUILD_TESTS=OFF
        -DCONTAINER_BUILD_INTEGRATION_TESTS=OFF
        -DCONTAINER_BUILD_BENCHMARKS=OFF
        -DBUILD_DOCUMENTATION=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME ContainerSystem
    CONFIG_PATH lib/cmake/ContainerSystem
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

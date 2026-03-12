# kcenon-container-system portfile
# Advanced C++20 Container System with Thread-Safe Operations and Messaging Integration

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/container_system
    REF 123d7db3e523167de85f707600df0884e35a871f
    SHA512 7d9a9fd0bf89548bf4c76ac2664fb47f5541b8564637d26350deee10da2ffe20020d892d1cdb678a36af1569da4adb7db9ee6c4bda6ff891d37aa3e6dacb0dcc
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_WITH_COMMON_SYSTEM=ON
        -DCOMMON_SYSTEM_ROOT=${CURRENT_INSTALLED_DIR}
        -DBUILD_TESTS=OFF
        -DCONTAINER_BUILD_INTEGRATION_TESTS=OFF
        -DCONTAINER_BUILD_BENCHMARKS=OFF
        -DBUILD_DOCUMENTATION=OFF
        -DBUILD_SAMPLES=OFF
        -DBUILD_EXAMPLES=OFF
        -DCONTAINER_BUILD_SAMPLES=OFF
        -DCONTAINER_BUILD_EXAMPLES=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME ContainerSystem
    CONFIG_PATH lib/cmake/ContainerSystem
)

# Remove example and sample executables (not valid distribution targets)
file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/bin/examples"
    "${CURRENT_PACKAGES_DIR}/bin/samples"
    "${CURRENT_PACKAGES_DIR}/debug/bin/examples"
    "${CURRENT_PACKAGES_DIR}/debug/bin/samples"
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

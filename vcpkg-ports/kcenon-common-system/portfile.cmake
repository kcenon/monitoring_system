# kcenon-common-system portfile
# High-performance C++20 foundation library (header-only)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/common_system
    REF "${VERSION}"
    SHA512 0  # TODO: Update with actual SHA512 hash after release
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTING=OFF
        -DBUILD_BENCHMARKS=OFF
    MAYBE_UNUSED_VARIABLES
        BUILD_TESTING
        BUILD_BENCHMARKS
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME common_system
    CONFIG_PATH lib/cmake/common_system
)

# Header-only library - remove all debug content and empty lib directories
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

# kcenon-pacs-system portfile
# Modern C++20 PACS implementation built on the kcenon ecosystem

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/pacs_system
    REF 3844173e3334ea4d8bcb60d30338293319e0ec18
    SHA512 71d5c452642559886b89ef71f6cb4e2feb4c88b1656e2477949bad067963a36975585c1420dd157ef31d87ab2df6220b14ba5996c6a5ebc769d41fa7a2216220
    HEAD_REF main
    PATCHES
        fix-vcpkg-dependency-discovery.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DPACS_BUILD_TESTS=OFF
        -DPACS_BUILD_EXAMPLES=OFF
        -DPACS_BUILD_BENCHMARKS=OFF
        -DPACS_BUILD_SAMPLES=OFF
        -DPACS_WITH_COMMON_SYSTEM=ON
        -DPACS_WITH_CONTAINER_SYSTEM=ON
        # Network integration disabled: upstream include paths assume submodule layout
        # (kcenon/logger/ vs logger_system/, internal/ headers not installed)
        # Re-enable when upstream fixes vcpkg-compatible include layout
        -DPACS_WITH_NETWORK_SYSTEM=OFF
        -DPACS_BUILD_STORAGE=OFF
        -DPACS_WITH_AWS_SDK=OFF
        -DPACS_WITH_AZURE_SDK=OFF
        -DPACS_BUILD_MODULES=OFF
        -DPACS_WARNINGS_AS_ERRORS=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME pacs_system
    CONFIG_PATH lib/cmake/pacs_system
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

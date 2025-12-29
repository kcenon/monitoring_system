# kcenon-thread-system portfile
# High-performance C++20 multithreading framework

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/thread_system
    REF 80242646537034754121657f18649939cecd77c6
    SHA512 0  # TODO: Update with actual SHA512 hash
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_WITH_COMMON_SYSTEM=ON
        -DTHREAD_BUILD_INTEGRATION_TESTS=OFF
        -DBUILD_DOCUMENTATION=OFF
        -DTHREAD_ENABLE_LOCKFREE_QUEUE=ON
        -DTHREAD_ENABLE_WORK_STEALING=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME thread_system
    CONFIG_PATH lib/cmake/thread_system
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

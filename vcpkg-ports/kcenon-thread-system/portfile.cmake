# kcenon-thread-system portfile
# High-performance C++20 multithreading framework

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/thread_system
    REF v0.3.0
    SHA512 7a3336340ec24230d8a5c94c7a0c0c9b671f0e9c9f2e88f9d122f86799afa3c6af7a271eb8d92d5c1d419e7d0a0a0a936c18ef7af845f737816f6800f9f9c4a3
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

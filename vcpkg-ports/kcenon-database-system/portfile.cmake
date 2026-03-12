# kcenon-database-system portfile
# Pure, lightweight C++20 Core DAL library with multi-backend support

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/database_system
    REF 918b89836ff39eb4a03221d6da9331334b6e91c6
    SHA512 c4e931487735a39047c12c2002c7b8532995d2e33146789ca3d6894d76125980d3c1c2b9173df5cf40d395dfe3a6b6d64779a1e160c3ff0567966420b63f207f
    HEAD_REF main
    PATCHES
        fix-common-system-target.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DUSE_UNIT_TEST=OFF
        -DBUILD_DATABASE_SAMPLES=OFF
        -DUSE_POSTGRESQL=OFF
        -DUSE_SQLITE=OFF
        -DUSE_MONGODB=OFF
        -DUSE_REDIS=OFF
        -DUSE_THREAD_SYSTEM=OFF
        -DUSE_MONITORING_SYSTEM=OFF
        -DUSE_CONTAINER_SYSTEM=OFF
        -DDATABASE_BUILD_INTEGRATION_TESTS=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME DatabaseSystem
    CONFIG_PATH lib/cmake/DatabaseSystem
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

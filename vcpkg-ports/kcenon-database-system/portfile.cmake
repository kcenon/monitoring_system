# kcenon-database-system portfile
# Pure, lightweight C++20 Core DAL library with multi-backend support

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/database_system
    REF "v${VERSION}"
    SHA512 7207570689fad1ae2afc96b4b7c79be3690a174e6f7e9c704ec470fa71f74e18ab872becbbd4a15bb3ace5a7d5eb9d5d7a5125ccca49062abab7f61b2d06185f
    HEAD_REF main
)

# Feature-based backend selection
set(DB_USE_POSTGRESQL OFF)
if("postgresql" IN_LIST FEATURES)
    set(DB_USE_POSTGRESQL ON)
endif()

set(DB_USE_SQLITE OFF)
if("sqlite" IN_LIST FEATURES)
    set(DB_USE_SQLITE ON)
endif()

set(DB_USE_MONGODB OFF)
if("mongodb" IN_LIST FEATURES)
    set(DB_USE_MONGODB ON)
endif()

set(DB_USE_REDIS OFF)
if("redis" IN_LIST FEATURES)
    set(DB_USE_REDIS ON)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DUSE_POSTGRESQL=${DB_USE_POSTGRESQL}
        -DUSE_SQLITE=${DB_USE_SQLITE}
        -DUSE_MONGODB=${DB_USE_MONGODB}
        -DUSE_REDIS=${DB_USE_REDIS}
        -DUSE_UNIT_TEST=OFF
        -DBUILD_DATABASE_SAMPLES=OFF
        -DDATABASE_BUILD_BENCHMARKS=OFF
        -DDATABASE_BUILD_INTEGRATION_TESTS=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DUSE_THREAD_SYSTEM=OFF
        -DUSE_MONITORING_SYSTEM=OFF
        -DUSE_CONTAINER_SYSTEM=OFF
        -DBUILD_INTEGRATED_DATABASE=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME database_system
    CONFIG_PATH lib/cmake/database_system
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

# kcenon-database-system portfile
# Pure, lightweight C++20 Core DAL library with multi-backend support

# The integrated_database library exports no DLL symbols by design (static-init registration pattern)
set(VCPKG_POLICY_DLLS_WITHOUT_EXPORTS enabled)

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
    PACKAGE_NAME database_system
    CONFIG_PATH lib/cmake/DatabaseSystem
)

# Create snake_case config entry point for find_package(database_system)
# Upstream installs as DatabaseSystem; this wrapper standardizes the package name
file(WRITE "${CURRENT_PACKAGES_DIR}/share/database_system/database_system-config.cmake"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/DatabaseSystemConfig.cmake\")\n"
)
if(EXISTS "${CURRENT_PACKAGES_DIR}/share/database_system/DatabaseSystemConfigVersion.cmake")
    file(WRITE "${CURRENT_PACKAGES_DIR}/share/database_system/database_system-config-version.cmake"
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/DatabaseSystemConfigVersion.cmake\")\n"
    )
endif()

# Fix: integrated_database uses static-init registration pattern with zero source
# files when all backends are disabled. MSVC produces no .lib for empty targets,
# but CMake install(EXPORT) still references it. Create stub .lib (empty COFF
# archive) so the imported target file-existence check passes.
if(VCPKG_TARGET_IS_WINDOWS)
    foreach(_libdir "${CURRENT_PACKAGES_DIR}/lib" "${CURRENT_PACKAGES_DIR}/debug/lib")
        set(_lib "${_libdir}/integrated_database.lib")
        if(NOT EXISTS "${_lib}")
            file(MAKE_DIRECTORY "${_libdir}")
            file(WRITE "${_lib}" "!<arch>\n")
        endif()
    endforeach()
endif()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

# kcenon-network-system portfile
# Modern C++20 async network library with TCP/UDP, HTTP/1.1, WebSocket, and TLS 1.3

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/network_system
    REF c0c907bebc5fab24457f1dd66d06308acc90b4d4
    SHA512 b0cf3a84ee5b83f3f3c40c86e7f427d48f97f32b3fd9da9dfe5d2429513e2d72bf2fd6487ed208e8a37f6217e52ca5fde2770b3d83beea1d5743b3af2fab1808
    HEAD_REF main
    PATCHES
        fix-common-system-target.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTS=OFF
        -DBUILD_SAMPLES=OFF
        -DBUILD_WEBSOCKET_SUPPORT=ON
        -DBUILD_MESSAGING_BRIDGE=OFF
        -DNETWORK_BUILD_BENCHMARKS=OFF
        -DNETWORK_BUILD_INTEGRATION_TESTS=OFF
        -DNETWORK_BUILD_MODULES=OFF
        -DBUILD_WITH_COMMON_SYSTEM=ON
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME network_system
    CONFIG_PATH lib/cmake/NetworkSystem
)

# Fix upstream: NetworkSystemTargets links ZLIB::ZLIB and OpenSSL::SSL
# but config omits find_dependency(ZLIB) and find_dependency(OpenSSL)
vcpkg_replace_string(
    "${CURRENT_PACKAGES_DIR}/share/network_system/NetworkSystemConfig.cmake"
    "find_dependency(asio CONFIG REQUIRED)"
    "find_dependency(OpenSSL REQUIRED)\nfind_dependency(ZLIB REQUIRED)\nfind_dependency(asio CONFIG REQUIRED)"
)

# Create snake_case config entry point for find_package(network_system)
# Upstream installs as NetworkSystem; this wrapper standardizes the package name
file(WRITE "${CURRENT_PACKAGES_DIR}/share/network_system/network_system-config.cmake"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/NetworkSystemConfig.cmake\")\n"
)
if(EXISTS "${CURRENT_PACKAGES_DIR}/share/network_system/NetworkSystemConfigVersion.cmake")
    file(WRITE "${CURRENT_PACKAGES_DIR}/share/network_system/network_system-config-version.cmake"
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/NetworkSystemConfigVersion.cmake\")\n"
    )
endif()

# Remove empty directories that cause vcpkg post-build validation warnings
file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/include/kcenon/network/core"
    "${CURRENT_PACKAGES_DIR}/include/kcenon/network/experimental"
    "${CURRENT_PACKAGES_DIR}/include/kcenon/network/http"
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

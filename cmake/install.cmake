# install.cmake
# Install rules and package configuration for monitoring_system.
#
# Uses a tiered install degradation scheme:
#   Tier 1: Headers (ALWAYS install)
#   Tier 2: Library targets - install always; with EXPORT when MONITORING_CAN_INSTALL is TRUE,
#           without EXPORT (Tier 2 fallback) otherwise.
#   Tier 3: install(EXPORT) + export(EXPORT) - conditional on all PUBLIC deps being IMPORTED.
#   Tier 4: Config + version files (ALWAYS install)
#
# The MONITORING_CAN_INSTALL variable is set in targets.cmake based on whether
# all PUBLIC dependencies (thread_system, logger_system, network_system) are
# IMPORTED targets. add_subdirectory builds (CI sibling-checkout path) flip
# this to FALSE; find_package builds keep it TRUE.

include(GNUInstallDirs)

# --- Tier 1: Headers (ALWAYS) ---
install(DIRECTORY include/kcenon/monitoring
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kcenon
    FILES_MATCHING PATTERN "*.h"
)

# Adapter headers are also collected via the install(DIRECTORY ...) call above,
# but this explicit install is retained for the COMPONENT Development tag.
install(FILES
    ${MONITORING_ADAPTER_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kcenon/monitoring/adapters
    COMPONENT Development
)

# --- Tier 2+3: Targets + conditional EXPORT ---
if(MONITORING_CAN_INSTALL)
    # Tier 2: Library targets with EXPORT set
    install(TARGETS monitoring_system monitoring_system_interface
        EXPORT monitoring_system_targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Tier 3: Export sets (requires all PUBLIC deps to be IMPORTED)
    install(EXPORT monitoring_system_targets
        FILE monitoring_system-targets.cmake
        NAMESPACE monitoring_system::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/monitoring_system
    )

    export(EXPORT monitoring_system_targets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/monitoring_system-targets.cmake"
        NAMESPACE monitoring_system::
    )
else()
    # Tier 2 fallback: Library targets without EXPORT
    install(TARGETS monitoring_system monitoring_system_interface
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    message(STATUS "monitoring_system: install(EXPORT) skipped — non-IMPORTED deps: "
                   "${_MONITORING_NON_IMPORTED_DEPS}. Headers and libraries are installed.")
endif()

# --- Tier 4: Config + version files (ALWAYS) ---
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/monitoring_system-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/monitoring_system-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/monitoring_system
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/monitoring_system-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/monitoring_system-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/monitoring_system-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/monitoring_system
)

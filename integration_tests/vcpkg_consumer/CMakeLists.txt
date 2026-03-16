cmake_minimum_required(VERSION 3.20)
project(vcpkg_chain_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Tier 0: Foundation ──────────────────────────────────────────────────────

find_package(common_system CONFIG REQUIRED)
message(STATUS "Found common_system: ${common_system_DIR}")

# ─── Tier 1: Threading & Containers ──────────────────────────────────────────

find_package(thread_system CONFIG REQUIRED)
message(STATUS "Found thread_system: ${thread_system_DIR}")

find_package(container_system CONFIG REQUIRED)
message(STATUS "Found container_system: ${container_system_DIR}")

# ─── Tier 2: Logging ─────────────────────────────────────────────────────────

find_package(logger_system CONFIG REQUIRED)
message(STATUS "Found logger_system: ${logger_system_DIR}")

# ─── Tier 3: Monitoring & Database ───────────────────────────────────────────

find_package(monitoring_system CONFIG REQUIRED)
message(STATUS "Found monitoring_system: ${monitoring_system_DIR}")

find_package(database_system CONFIG REQUIRED)
message(STATUS "Found database_system: ${database_system_DIR}")

# ─── Tier 4: Networking ──────────────────────────────────────────────────────

find_package(network_system CONFIG REQUIRED)
message(STATUS "Found network_system: ${network_system_DIR}")

# ─── Tier 5: PACS ────────────────────────────────────────────────────────────

find_package(pacs_system CONFIG REQUIRED)
message(STATUS "Found pacs_system: ${pacs_system_DIR}")

# ─── Test Consumer Binary ────────────────────────────────────────────────────

add_executable(vcpkg_chain_consumer main.cpp)

target_link_libraries(vcpkg_chain_consumer PRIVATE
    kcenon::common_system
    thread_system::ThreadSystem
    ContainerSystem::container
    LoggerSystem::LoggerSystem
    monitoring_system::monitoring_system
    DatabaseSystem::database
    NetworkSystem::NetworkSystem
    kcenon::pacs::core
)

target_compile_definitions(vcpkg_chain_consumer PRIVATE
    HAS_COMMON_SYSTEM
    HAS_THREAD_SYSTEM
    HAS_CONTAINER_SYSTEM
    HAS_LOGGER_SYSTEM
    HAS_MONITORING_SYSTEM
    HAS_DATABASE_SYSTEM
    HAS_NETWORK_SYSTEM
    HAS_PACS_SYSTEM
)

# ─── Summary ─────────────────────────────────────────────────────────────────

message(STATUS "")
message(STATUS "vcpkg chain consumer: all 8 ports found and linked")
message(STATUS "")

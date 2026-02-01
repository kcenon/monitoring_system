# Example Plugin for Dynamic Loading

cmake_minimum_required(VERSION 3.20)
project(example_plugin VERSION 1.0.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build the plugin as a shared library
add_library(example_plugin SHARED
    example_plugin.cpp
)

# Include directories
target_include_directories(example_plugin
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Platform-specific settings
if(UNIX)
    # Set -fPIC for position-independent code
    set_target_properties(example_plugin PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )

    # Set output name to libexample_plugin.so (Linux) or libexample_plugin.dylib (macOS)
    if(APPLE)
        set_target_properties(example_plugin PROPERTIES
            SUFFIX ".dylib"
        )
    else()
        set_target_properties(example_plugin PROPERTIES
            SUFFIX ".so"
        )
    endif()
elseif(WIN32)
    # Windows DLL export settings
    set_target_properties(example_plugin PROPERTIES
        SUFFIX ".dll"
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endif()

# Enable symbol visibility for plugin exports
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(example_plugin PRIVATE
        -fvisibility=hidden
    )
endif()

# Install plugin to examples directory
install(TARGETS example_plugin
    LIBRARY DESTINATION examples/plugin_example
    RUNTIME DESTINATION examples/plugin_example
)

message(STATUS "Example Plugin: Building shared library for dynamic loading")

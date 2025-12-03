# MonitoringCompatibility.cmake
# C++17/C++20 feature detection and compatibility layer for monitoring_system

##################################################
# Feature Detection Functions
##################################################

# Test for std::jthread availability
function(check_std_jthread)
    include(CheckCXXSourceCompiles)
    
    set(CMAKE_REQUIRED_FLAGS "-std=c++20")
    check_cxx_source_compiles("
        #include <thread>
        #include <stop_token>
        int main() {
            std::jthread t([](){});
            t.request_stop();
            return 0;
        }
    " HAS_STD_JTHREAD)
    
    if(HAS_STD_JTHREAD)
        message(STATUS "std::jthread is available")
        add_definitions(-DMONITORING_HAS_STD_JTHREAD)
    else()
        message(STATUS "std::jthread not available - using std::thread with manual stop management")
    endif()
endfunction()

# Test for concepts availability
function(check_concepts)
    include(CheckCXXSourceCompiles)
    
    set(CMAKE_REQUIRED_FLAGS "-std=c++20")
    check_cxx_source_compiles("
        #include <concepts>
        template<typename T>
        concept Numeric = std::integral<T> || std::floating_point<T>;
        template<Numeric T>
        T process(T value) { return value * 2; }
        int main() {
            return process(42);
        }
    " HAS_CONCEPTS)
    
    if(HAS_CONCEPTS)
        message(STATUS "C++20 concepts are available")
        add_definitions(-DMONITORING_HAS_CONCEPTS)
    else()
        message(STATUS "C++20 concepts not available - using SFINAE fallback")
    endif()
endfunction()

# Test for std::span availability
function(check_std_span)
    include(CheckCXXSourceCompiles)
    
    set(CMAKE_REQUIRED_FLAGS "-std=c++20")
    check_cxx_source_compiles("
        #include <span>
        #include <array>
        int main() {
            std::array<int, 5> arr = {1, 2, 3, 4, 5};
            std::span<int> s(arr);
            return s.size();
        }
    " HAS_STD_SPAN)
    
    if(HAS_STD_SPAN)
        message(STATUS "std::span is available")
        add_definitions(-DMONITORING_HAS_STD_SPAN)
    else()
        message(STATUS "std::span not available - using pointer/size pairs")
    endif()
endfunction()

# Test for atomic wait/notify operations
function(check_atomic_wait)
    include(CheckCXXSourceCompiles)
    
    set(CMAKE_REQUIRED_FLAGS "-std=c++20")
    check_cxx_source_compiles("
        #include <atomic>
        int main() {
            std::atomic<int> flag{0};
            flag.wait(0);
            flag.notify_one();
            return 0;
        }
    " HAS_ATOMIC_WAIT)
    
    if(HAS_ATOMIC_WAIT)
        message(STATUS "Atomic wait/notify operations are available")
        add_definitions(-DMONITORING_HAS_ATOMIC_WAIT)
    else()
        message(STATUS "Atomic wait/notify not available - using condition variables")
    endif()
endfunction()

# Test for std::barrier availability
function(check_std_barrier)
    include(CheckCXXSourceCompiles)
    
    set(CMAKE_REQUIRED_FLAGS "-std=c++20")
    check_cxx_source_compiles("
        #include <barrier>
        int main() {
            std::barrier sync_point(2);
            sync_point.arrive_and_wait();
            return 0;
        }
    " HAS_STD_BARRIER)
    
    if(HAS_STD_BARRIER)
        message(STATUS "std::barrier is available")
        add_definitions(-DMONITORING_HAS_STD_BARRIER)
    else()
        message(STATUS "std::barrier not available - using condition variable implementation")
    endif()
endfunction()

##################################################
# Main Compatibility Check Function
##################################################

function(configure_monitoring_compatibility)
    message(STATUS "========================================")
    message(STATUS "Monitoring System - Feature Detection:")
    message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
    
    if(CMAKE_CXX_STANDARD EQUAL 17)
        message(STATUS "  Mode: C++17 Compatibility")
        add_definitions(-DMONITORING_CPP17_MODE)
        
        # In C++17 mode, disable all C++20 features but still check formatting
        message(STATUS "  - std::jthread: DISABLED (using std::thread)")
        message(STATUS "  - concepts: DISABLED (using SFINAE)")
        message(STATUS "  - std::span: DISABLED (using pointer/size)")
        message(STATUS "  - atomic wait/notify: DISABLED (using condition variables)")
        message(STATUS "  - std::barrier: DISABLED (using custom implementation)")
        
        # Check formatting support even in C++17 mode
        check_formatting_support()
        
    else()
        message(STATUS "  Mode: C++20 Enhanced")
        add_definitions(-DMONITORING_CPP20_MODE)
        
        # Test each C++20 feature
        check_std_jthread()
        check_concepts()
        check_std_span()
        check_atomic_wait()
        check_std_barrier()
        check_formatting_support()
    endif()
    
    message(STATUS "========================================")
endfunction()

##################################################
# Formatting Library Management
##################################################

# Test for std::format availability (C++20 required)
function(check_formatting_support)
    include(CheckCXXSourceCompiles)

    # C++20 std::format is required - no fallback to external libraries
    set(CMAKE_REQUIRED_FLAGS "-std=c++20")
    check_cxx_source_compiles("
        #include <format>
        int main() {
            auto s = std::format(\"Performance: {:.2f}%\", 95.5);
            return 0;
        }
    " HAS_STD_FORMAT)

    if(HAS_STD_FORMAT)
        message(STATUS "✅ Using C++20 std::format for monitoring")
        add_definitions(-DMONITORING_USE_STD_FORMAT)
        set(MONITORING_FORMAT_BACKEND "std::format" CACHE INTERNAL "Monitoring format backend")
        set(MONITORING_FORMAT_BACKEND "std::format" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "std::format is required. Please use a C++20 compliant compiler:\n"
            "  - GCC 13+ (full support) or GCC 11+ (partial)\n"
            "  - Clang 14+ with libc++\n"
            "  - MSVC 2022 (19.29+)\n"
            "  - Apple Clang 15+")
    endif()
endfunction()

# Configure std::format for monitoring target (C++20 required)
function(setup_monitoring_formatting TARGET_NAME)
    message(STATUS "========================================")
    message(STATUS "Setting up formatting for ${TARGET_NAME}")
    message(STATUS "  Backend: std::format (C++20)")
    target_compile_definitions(${TARGET_NAME} PRIVATE MONITORING_USE_STD_FORMAT)
    message(STATUS "========================================")
endfunction()

##################################################
# Performance Feature Management
##################################################

function(setup_performance_features)
    if(CMAKE_CXX_STANDARD EQUAL 17)
        # Use C++17 compatible performance features
        message(STATUS "Configuring C++17 compatible performance monitoring")
        add_definitions(-DMONITORING_USE_LEGACY_PERFORMANCE)
    else()
        # Use C++20 enhanced performance features
        message(STATUS "Configuring C++20 enhanced performance monitoring")
        add_definitions(-DMONITORING_USE_ENHANCED_PERFORMANCE)
    endif()
endfunction()

##################################################
# Thread Safety Feature Management
##################################################

function(setup_threading_features)
    if(CMAKE_CXX_STANDARD EQUAL 17)
        # Use traditional threading with manual management
        message(STATUS "Using traditional thread management (C++17)")
        add_definitions(-DMONITORING_USE_TRADITIONAL_THREADING)
    else()
        # Use modern threading with jthread and stop tokens
        message(STATUS "Using modern thread management with jthread (C++20)")
        add_definitions(-DMONITORING_USE_MODERN_THREADING)
    endif()
endfunction()
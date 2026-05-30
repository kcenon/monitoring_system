# fuzz/CMakeLists.txt
# libFuzzer harnesses for monitoring_system's untrusted-input parsing surfaces.
#
# Enabled with -DBUILD_FUZZERS=ON. Requires a Clang toolchain with libFuzzer
# support (-fsanitize=fuzzer). AddressSanitizer is layered on top to catch
# memory-safety issues exercised under fuzzing.
#
#   cmake -B build -G Ninja -DBUILD_FUZZERS=ON \
#         -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
#         -DCMAKE_BUILD_TYPE=RelWithDebInfo
#   cmake --build build --target protobuf_wire_fuzzer

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING
        "BUILD_FUZZERS=ON requires Clang for libFuzzer support; "
        "current compiler is '${CMAKE_CXX_COMPILER_ID}'. Skipping fuzz targets.")
    return()
endif()

set(MONITORING_FUZZER_FLAGS -fsanitize=fuzzer,address -g -O1)

# Each fuzz target is header-only against the parsing surface it exercises, so
# it does not need to link the full monitoring_system static library. It only
# needs the public include tree (and common_system headers pulled in
# transitively by the result/error types).
set(MONITORING_FUZZ_TARGETS
    protobuf_wire_fuzzer
)

foreach(fuzz_target ${MONITORING_FUZZ_TARGETS})
    add_executable(${fuzz_target} ${CMAKE_CURRENT_SOURCE_DIR}/${fuzz_target}.cpp)

    target_include_directories(${fuzz_target}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
    )

    if(TARGET monitoring_system_interface)
        target_link_libraries(${fuzz_target} PRIVATE monitoring_system_interface)
    endif()
    if(TARGET kcenon::common_system)
        target_link_libraries(${fuzz_target} PRIVATE kcenon::common_system)
    endif()

    target_compile_features(${fuzz_target} PRIVATE cxx_std_20)
    target_compile_options(${fuzz_target} PRIVATE ${MONITORING_FUZZER_FLAGS})
    target_link_options(${fuzz_target} PRIVATE ${MONITORING_FUZZER_FLAGS})

    set_target_properties(${fuzz_target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz
    )
endforeach()

message(STATUS "monitoring_system: fuzz targets enabled (${MONITORING_FUZZ_TARGETS})")

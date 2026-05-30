/**
 * @file protobuf_wire_fuzzer.cpp
 * @brief libFuzzer harness for the internal protobuf wire-format decoder.
 *
 * Targets kcenon::monitoring::protobuf_wire's decode primitives
 * (decode_varint / decode_tag / decode_length_delimited), which are the
 * monitoring_system's lowest-level untrusted-input parsing surface: they are
 * used to deserialize Jaeger api_v2 and Zipkin proto3 span messages received
 * over the wire by the trace exporters. Malformed or adversarial bytes must be
 * rejected gracefully (std::nullopt) without out-of-bounds reads, overflow, or
 * other undefined behavior.
 *
 * The header is fully inline / header-only, so this harness needs no link
 * against the monitoring_system library.
 *
 * Build via the BUILD_FUZZERS CMake option with a fuzzer-capable Clang:
 *   cmake -B build -DBUILD_FUZZERS=ON \
 *         -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
 *         -DCMAKE_BUILD_TYPE=RelWithDebInfo
 *   cmake --build build --target protobuf_wire_fuzzer
 *   ./build/fuzz/protobuf_wire_fuzzer fuzz/corpus/protobuf_wire
 */
#include <kcenon/monitoring/exporters/internal/protobuf_wire.h>

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    namespace pw = kcenon::monitoring::protobuf_wire;

    // Walk the buffer as a sequence of protobuf fields, exactly as a real
    // message decoder would, exercising every decode primitive. The loop must
    // terminate on any malformed field; the decoders signal that by returning
    // std::nullopt and not advancing past the buffer end.
    std::size_t offset = 0;
    while (offset < size) {
        const std::size_t before = offset;

        auto tag = pw::decode_tag(data, size, offset);
        if (!tag.has_value()) {
            break;  // truncated/invalid tag
        }

        switch (tag->second) {
            case pw::wire_type::varint: {
                auto v = pw::decode_varint(data, size, offset);
                if (!v.has_value()) {
                    offset = size;  // stop: truncated varint
                }
                break;
            }
            case pw::wire_type::length_delimited: {
                auto ld = pw::decode_length_delimited(data, size, offset);
                if (!ld.has_value()) {
                    offset = size;  // stop: bad length-delimited field
                }
                break;
            }
            case pw::wire_type::fixed64: {
                if (offset + 8 > size) {
                    offset = size;
                } else {
                    offset += 8;
                }
                break;
            }
            case pw::wire_type::fixed32: {
                if (offset + 4 > size) {
                    offset = size;
                } else {
                    offset += 4;
                }
                break;
            }
            default:
                // Unknown wire type: cannot safely skip; stop.
                offset = size;
                break;
        }

        // Guard against any decoder that fails to make forward progress, which
        // would otherwise spin forever on a crafted input.
        if (offset == before) {
            break;
        }
    }

    return 0;
}

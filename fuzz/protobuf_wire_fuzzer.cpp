/**
 * @file protobuf_wire_fuzzer.cpp
 * @brief libFuzzer harness for the internal protobuf wire-format decoder.
 *
 * Targets kcenon::monitoring::protobuf_wire's decode primitives via the
 * `reader` class (decode_tag / reader::read_varint / reader::read_fixed64 /
 * reader::read_fixed32 / reader::read_length_delimited), which are the
 * monitoring_system's lowest-level untrusted-input parsing surface: they are
 * used to deserialize Jaeger api_v2 and Zipkin proto3 span messages received
 * over the wire by the trace exporters. Malformed or adversarial bytes must be
 * rejected gracefully (false / std::nullopt) without out-of-bounds reads,
 * overflow, or other undefined behavior.
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

    pw::reader r(data, size);

    // Walk the buffer as a sequence of protobuf fields, exactly as a real
    // message decoder would, exercising every decode primitive. The loop must
    // terminate on any malformed field; the decoders signal that by returning
    // false / std::nullopt without advancing past the buffer end.
    while (!r.eof()) {
        const std::size_t before = r.position();

        std::uint32_t field_number = 0;
        pw::wire_type wt{};
        if (!pw::decode_tag(r, field_number, wt)) {
            break;  // truncated/invalid tag
        }

        switch (wt) {
            case pw::wire_type::varint:
                if (!r.read_varint().has_value()) {
                    return 0;  // truncated varint
                }
                break;
            case pw::wire_type::fixed64:
                if (!r.read_fixed64().has_value()) {
                    return 0;  // truncated fixed64
                }
                break;
            case pw::wire_type::fixed32:
                if (!r.read_fixed32().has_value()) {
                    return 0;  // truncated fixed32
                }
                break;
            case pw::wire_type::length_delimited: {
                const std::uint8_t* ptr = nullptr;
                std::size_t len = 0;
                if (!r.read_length_delimited(&ptr, &len)) {
                    return 0;  // bad length-delimited field
                }
                break;
            }
            default:
                // Unknown/unsupported wire type (e.g. deprecated groups):
                // cannot safely skip; stop.
                return 0;
        }

        // Guard against any decoder that fails to make forward progress, which
        // would otherwise spin forever on a crafted input.
        if (r.position() == before) {
            break;
        }
    }

    return 0;
}

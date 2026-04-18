#pragma once

// BSD 3-Clause License
// Copyright (c) 2025
// See the LICENSE file in the project root for full license information.

/**
 * @file jaeger_proto.h
 * @brief Serialization/deserialization of Jaeger api_v2 model.proto messages.
 *
 * Implements the minimal subset of `jaeger.api_v2` required to POST spans
 * to a Jaeger collector. Field numbers match jaegertracing/jaeger-idl
 * `proto/api_v2/model.proto`:
 *   KeyValue:  key=1, v_type=2, v_str=3, v_bool=4, v_int64=5, v_float64=6, v_binary=7
 *   Log:       timestamp=1, fields=2
 *   SpanRef:   trace_id=1, span_id=2, ref_type=3
 *   Process:   service_name=1, tags=2
 *   Span:      trace_id=1, span_id=2, operation_name=3, references=4, flags=5,
 *              start_time=6, duration=7, tags=8, logs=9, process=10
 *   Batch:     spans=1, process=2
 *
 * google.protobuf.Timestamp: seconds=1 (int64), nanos=2 (int32)
 * google.protobuf.Duration:  seconds=1 (int64), nanos=2 (int32)
 */

#include "protobuf_wire.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace kcenon { namespace monitoring { namespace jaeger_proto {

enum class value_type : std::int32_t {
    string_type  = 0,
    bool_type    = 1,
    int64_type   = 2,
    float64_type = 3,
    binary_type  = 4,
};

struct key_value {
    std::string key;
    value_type v_type{value_type::string_type};
    std::string v_str;
    bool v_bool{false};
    std::int64_t v_int64{0};
    double v_float64{0.0};
    std::vector<std::uint8_t> v_binary;
};

struct process {
    std::string service_name;
    std::vector<key_value> tags;
};

struct span_ref {
    std::vector<std::uint8_t> trace_id;
    std::vector<std::uint8_t> span_id;
    std::int32_t ref_type{0};  // 0 = CHILD_OF, 1 = FOLLOWS_FROM
};

struct span {
    std::vector<std::uint8_t> trace_id;   // 16 bytes on the wire
    std::vector<std::uint8_t> span_id;    // 8 bytes on the wire
    std::string operation_name;
    std::vector<span_ref> references;
    std::uint32_t flags{0};
    std::int64_t start_time_seconds{0};
    std::int32_t start_time_nanos{0};
    std::int64_t duration_seconds{0};
    std::int32_t duration_nanos{0};
    std::vector<key_value> tags;
    // Logs omitted from this minimal encoder (not produced by the current
    // span data model). The decoder skips unknown fields safely.
    process proc;
};

struct batch {
    std::vector<span> spans;
    process proc;
    bool has_process{false};
};

// ---------------------------------------------------------------------------
// Encoders
// ---------------------------------------------------------------------------

inline std::vector<std::uint8_t> encode_timestamp(std::int64_t seconds,
                                                  std::int32_t nanos) {
    std::vector<std::uint8_t> out;
    protobuf_wire::encode_uint64_field(out, 1, static_cast<std::uint64_t>(seconds));
    protobuf_wire::encode_uint64_field(out, 2, static_cast<std::uint64_t>(nanos));
    return out;
}

inline std::vector<std::uint8_t> encode_duration(std::int64_t seconds,
                                                 std::int32_t nanos) {
    std::vector<std::uint8_t> out;
    protobuf_wire::encode_uint64_field(out, 1, static_cast<std::uint64_t>(seconds));
    protobuf_wire::encode_uint64_field(out, 2, static_cast<std::uint64_t>(nanos));
    return out;
}

inline std::vector<std::uint8_t> encode_key_value(const key_value& kv) {
    std::vector<std::uint8_t> out;
    protobuf_wire::encode_string_field(out, 1, kv.key);
    protobuf_wire::encode_enum_field(out, 2, static_cast<std::int32_t>(kv.v_type));
    protobuf_wire::encode_string_field(out, 3, kv.v_str);
    protobuf_wire::encode_bool_field(out, 4, kv.v_bool);
    if (kv.v_int64 != 0) {
        protobuf_wire::encode_tag(out, 5, protobuf_wire::wire_type::varint);
        protobuf_wire::encode_varint(out, static_cast<std::uint64_t>(kv.v_int64));
    }
    if (kv.v_float64 != 0.0) {
        std::uint64_t bits;
        static_assert(sizeof(bits) == sizeof(kv.v_float64),
                      "double must be 64 bits");
        std::memcpy(&bits, &kv.v_float64, sizeof(bits));
        protobuf_wire::encode_tag(out, 6, protobuf_wire::wire_type::fixed64);
        protobuf_wire::encode_fixed64(out, bits);
    }
    protobuf_wire::encode_bytes_field(out, 7, kv.v_binary);
    return out;
}

inline std::vector<std::uint8_t> encode_process(const process& p) {
    std::vector<std::uint8_t> out;
    protobuf_wire::encode_string_field(out, 1, p.service_name);
    for (const auto& tag : p.tags) {
        auto serialized = encode_key_value(tag);
        protobuf_wire::encode_message_field(out, 2, serialized);
    }
    return out;
}

inline std::vector<std::uint8_t> encode_span_ref(const span_ref& ref) {
    std::vector<std::uint8_t> out;
    protobuf_wire::encode_bytes_field(out, 1, ref.trace_id);
    protobuf_wire::encode_bytes_field(out, 2, ref.span_id);
    protobuf_wire::encode_enum_field(out, 3, ref.ref_type);
    return out;
}

inline std::vector<std::uint8_t> encode_span(const span& s) {
    std::vector<std::uint8_t> out;
    protobuf_wire::encode_bytes_field(out, 1, s.trace_id);
    protobuf_wire::encode_bytes_field(out, 2, s.span_id);
    protobuf_wire::encode_string_field(out, 3, s.operation_name);
    for (const auto& ref : s.references) {
        auto serialized = encode_span_ref(ref);
        protobuf_wire::encode_message_field(out, 4, serialized);
    }
    protobuf_wire::encode_uint64_field(out, 5, s.flags);
    if (s.start_time_seconds != 0 || s.start_time_nanos != 0) {
        auto ts = encode_timestamp(s.start_time_seconds, s.start_time_nanos);
        protobuf_wire::encode_message_field(out, 6, ts);
    }
    if (s.duration_seconds != 0 || s.duration_nanos != 0) {
        auto du = encode_duration(s.duration_seconds, s.duration_nanos);
        protobuf_wire::encode_message_field(out, 7, du);
    }
    for (const auto& tag : s.tags) {
        auto serialized = encode_key_value(tag);
        protobuf_wire::encode_message_field(out, 8, serialized);
    }
    if (!s.proc.service_name.empty() || !s.proc.tags.empty()) {
        auto serialized = encode_process(s.proc);
        protobuf_wire::encode_message_field(out, 10, serialized);
    }
    return out;
}

inline std::vector<std::uint8_t> encode_batch(const batch& b) {
    std::vector<std::uint8_t> out;
    for (const auto& s : b.spans) {
        auto serialized = encode_span(s);
        protobuf_wire::encode_message_field(out, 1, serialized);
    }
    if (b.has_process) {
        auto serialized = encode_process(b.proc);
        protobuf_wire::encode_message_field(out, 2, serialized);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Decoders (used by round-trip tests)
// ---------------------------------------------------------------------------

inline bool decode_key_value(const std::uint8_t* data, std::size_t size,
                             key_value& out) {
    protobuf_wire::reader r(data, size);
    while (!r.eof()) {
        std::uint32_t field_number;
        protobuf_wire::wire_type wt;
        if (!protobuf_wire::decode_tag(r, field_number, wt)) return false;
        switch (field_number) {
            case 1:
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                if (!r.read_string(out.key)) return false;
                break;
            case 2: {
                if (wt != protobuf_wire::wire_type::varint) return false;
                auto v = r.read_varint();
                if (!v) return false;
                out.v_type = static_cast<value_type>(*v);
                break;
            }
            case 3:
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                if (!r.read_string(out.v_str)) return false;
                break;
            case 4: {
                if (wt != protobuf_wire::wire_type::varint) return false;
                auto v = r.read_varint();
                if (!v) return false;
                out.v_bool = (*v != 0);
                break;
            }
            case 5: {
                if (wt != protobuf_wire::wire_type::varint) return false;
                auto v = r.read_varint();
                if (!v) return false;
                out.v_int64 = static_cast<std::int64_t>(*v);
                break;
            }
            case 6: {
                if (wt != protobuf_wire::wire_type::fixed64) return false;
                auto v = r.read_fixed64();
                if (!v) return false;
                std::uint64_t bits = *v;
                std::memcpy(&out.v_float64, &bits, sizeof(out.v_float64));
                break;
            }
            case 7:
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                if (!r.read_bytes(out.v_binary)) return false;
                break;
            default:
                if (!r.skip_field(wt)) return false;
        }
    }
    return true;
}

inline bool decode_process(const std::uint8_t* data, std::size_t size,
                           process& out) {
    protobuf_wire::reader r(data, size);
    while (!r.eof()) {
        std::uint32_t field_number;
        protobuf_wire::wire_type wt;
        if (!protobuf_wire::decode_tag(r, field_number, wt)) return false;
        switch (field_number) {
            case 1:
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                if (!r.read_string(out.service_name)) return false;
                break;
            case 2: {
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                const std::uint8_t* ptr; std::size_t len;
                if (!r.read_length_delimited(&ptr, &len)) return false;
                key_value kv;
                if (!decode_key_value(ptr, len, kv)) return false;
                out.tags.push_back(std::move(kv));
                break;
            }
            default:
                if (!r.skip_field(wt)) return false;
        }
    }
    return true;
}

inline bool decode_timestamp(const std::uint8_t* data, std::size_t size,
                             std::int64_t& seconds, std::int32_t& nanos) {
    protobuf_wire::reader r(data, size);
    seconds = 0; nanos = 0;
    while (!r.eof()) {
        std::uint32_t field_number;
        protobuf_wire::wire_type wt;
        if (!protobuf_wire::decode_tag(r, field_number, wt)) return false;
        if (wt != protobuf_wire::wire_type::varint) {
            if (!r.skip_field(wt)) return false;
            continue;
        }
        auto v = r.read_varint();
        if (!v) return false;
        if (field_number == 1) seconds = static_cast<std::int64_t>(*v);
        else if (field_number == 2) nanos = static_cast<std::int32_t>(*v);
    }
    return true;
}

inline bool decode_span_ref(const std::uint8_t* data, std::size_t size,
                            span_ref& out) {
    protobuf_wire::reader r(data, size);
    while (!r.eof()) {
        std::uint32_t field_number;
        protobuf_wire::wire_type wt;
        if (!protobuf_wire::decode_tag(r, field_number, wt)) return false;
        switch (field_number) {
            case 1:
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                if (!r.read_bytes(out.trace_id)) return false;
                break;
            case 2:
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                if (!r.read_bytes(out.span_id)) return false;
                break;
            case 3: {
                if (wt != protobuf_wire::wire_type::varint) return false;
                auto v = r.read_varint();
                if (!v) return false;
                out.ref_type = static_cast<std::int32_t>(*v);
                break;
            }
            default:
                if (!r.skip_field(wt)) return false;
        }
    }
    return true;
}

inline bool decode_span(const std::uint8_t* data, std::size_t size,
                        span& out) {
    protobuf_wire::reader r(data, size);
    while (!r.eof()) {
        std::uint32_t field_number;
        protobuf_wire::wire_type wt;
        if (!protobuf_wire::decode_tag(r, field_number, wt)) return false;
        switch (field_number) {
            case 1:
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                if (!r.read_bytes(out.trace_id)) return false;
                break;
            case 2:
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                if (!r.read_bytes(out.span_id)) return false;
                break;
            case 3:
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                if (!r.read_string(out.operation_name)) return false;
                break;
            case 4: {
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                const std::uint8_t* ptr; std::size_t len;
                if (!r.read_length_delimited(&ptr, &len)) return false;
                span_ref ref;
                if (!decode_span_ref(ptr, len, ref)) return false;
                out.references.push_back(std::move(ref));
                break;
            }
            case 5: {
                if (wt != protobuf_wire::wire_type::varint) return false;
                auto v = r.read_varint();
                if (!v) return false;
                out.flags = static_cast<std::uint32_t>(*v);
                break;
            }
            case 6: {
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                const std::uint8_t* ptr; std::size_t len;
                if (!r.read_length_delimited(&ptr, &len)) return false;
                if (!decode_timestamp(ptr, len,
                                      out.start_time_seconds,
                                      out.start_time_nanos))
                    return false;
                break;
            }
            case 7: {
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                const std::uint8_t* ptr; std::size_t len;
                if (!r.read_length_delimited(&ptr, &len)) return false;
                if (!decode_timestamp(ptr, len,
                                      out.duration_seconds,
                                      out.duration_nanos))
                    return false;
                break;
            }
            case 8: {
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                const std::uint8_t* ptr; std::size_t len;
                if (!r.read_length_delimited(&ptr, &len)) return false;
                key_value kv;
                if (!decode_key_value(ptr, len, kv)) return false;
                out.tags.push_back(std::move(kv));
                break;
            }
            case 10: {
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                const std::uint8_t* ptr; std::size_t len;
                if (!r.read_length_delimited(&ptr, &len)) return false;
                if (!decode_process(ptr, len, out.proc)) return false;
                break;
            }
            default:
                if (!r.skip_field(wt)) return false;
        }
    }
    return true;
}

inline bool decode_batch(const std::uint8_t* data, std::size_t size,
                         batch& out) {
    protobuf_wire::reader r(data, size);
    while (!r.eof()) {
        std::uint32_t field_number;
        protobuf_wire::wire_type wt;
        if (!protobuf_wire::decode_tag(r, field_number, wt)) return false;
        switch (field_number) {
            case 1: {
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                const std::uint8_t* ptr; std::size_t len;
                if (!r.read_length_delimited(&ptr, &len)) return false;
                span s;
                if (!decode_span(ptr, len, s)) return false;
                out.spans.push_back(std::move(s));
                break;
            }
            case 2: {
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                const std::uint8_t* ptr; std::size_t len;
                if (!r.read_length_delimited(&ptr, &len)) return false;
                if (!decode_process(ptr, len, out.proc)) return false;
                out.has_process = true;
                break;
            }
            default:
                if (!r.skip_field(wt)) return false;
        }
    }
    return true;
}

}}}  // namespace kcenon::monitoring::jaeger_proto

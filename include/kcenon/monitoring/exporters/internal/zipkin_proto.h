#pragma once

// BSD 3-Clause License
// Copyright (c) 2025
// See the LICENSE file in the project root for full license information.

/**
 * @file zipkin_proto.h
 * @brief Serialization/deserialization of Zipkin `zipkin.proto` messages.
 *
 * Implements the subset of `zipkin.proto3` required for POST /api/v2/spans.
 * Field numbers match openzipkin/zipkin-api `zipkin.proto`:
 *   Span:        trace_id=1, parent_id=2, id=3, kind=4, name=5, timestamp=6,
 *                duration=7, local_endpoint=8, remote_endpoint=9,
 *                annotations=10, tags=11, debug=12, shared=13
 *   Endpoint:    service_name=1, ipv4=2, ipv6=3, port=4
 *   Annotation:  timestamp=1, value=2
 *   ListOfSpans: spans=1
 *
 * Note: `timestamp` is encoded as fixed64 per spec; `duration` is varint uint64.
 */

#include "protobuf_wire.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace kcenon { namespace monitoring { namespace zipkin_proto {

enum class span_kind : std::int32_t {
    unspecified = 0,
    client      = 1,
    server      = 2,
    producer    = 3,
    consumer    = 4,
};

/** @brief Convert a textual Zipkin kind (e.g. "CLIENT") to its enum value. */
inline span_kind parse_kind(const std::string& value) {
    std::string upper;
    upper.reserve(value.size());
    for (char c : value) {
        if (c >= 'a' && c <= 'z') upper.push_back(static_cast<char>(c - 'a' + 'A'));
        else upper.push_back(c);
    }
    if (upper == "CLIENT") return span_kind::client;
    if (upper == "SERVER") return span_kind::server;
    if (upper == "PRODUCER") return span_kind::producer;
    if (upper == "CONSUMER") return span_kind::consumer;
    return span_kind::unspecified;
}

struct endpoint {
    std::string service_name;
    std::vector<std::uint8_t> ipv4;
    std::vector<std::uint8_t> ipv6;
    std::int32_t port{0};

    bool empty() const {
        return service_name.empty() && ipv4.empty() && ipv6.empty() && port == 0;
    }
};

struct annotation {
    std::uint64_t timestamp{0};  // epoch microseconds
    std::string value;
};

struct span {
    std::vector<std::uint8_t> trace_id;   // 8 or 16 bytes
    std::vector<std::uint8_t> parent_id;  // 8 bytes (may be empty)
    std::vector<std::uint8_t> id;         // 8 bytes
    span_kind kind{span_kind::unspecified};
    std::string name;
    std::uint64_t timestamp{0};   // epoch microseconds
    std::uint64_t duration{0};    // microseconds
    endpoint local_endpoint;
    endpoint remote_endpoint;
    std::vector<annotation> annotations;
    std::vector<std::pair<std::string, std::string>> tags;  // proto map<string,string>
    bool debug{false};
    bool shared{false};
};

struct list_of_spans {
    std::vector<span> spans;
};

// ---------------------------------------------------------------------------
// Encoders
// ---------------------------------------------------------------------------

inline std::vector<std::uint8_t> encode_endpoint(const endpoint& ep) {
    std::vector<std::uint8_t> out;
    protobuf_wire::encode_string_field(out, 1, ep.service_name);
    protobuf_wire::encode_bytes_field(out, 2, ep.ipv4);
    protobuf_wire::encode_bytes_field(out, 3, ep.ipv6);
    protobuf_wire::encode_uint64_field(out, 4, static_cast<std::uint64_t>(ep.port));
    return out;
}

inline std::vector<std::uint8_t> encode_annotation(const annotation& ann) {
    std::vector<std::uint8_t> out;
    if (ann.timestamp != 0) {
        protobuf_wire::encode_tag(out, 1, protobuf_wire::wire_type::fixed64);
        protobuf_wire::encode_fixed64(out, ann.timestamp);
    }
    protobuf_wire::encode_string_field(out, 2, ann.value);
    return out;
}

/**
 * @brief Encode a single entry of a `map<string,string>` field.
 *
 * Protobuf encodes map fields as a repeated synthetic message with field 1 =
 * key and field 2 = value.
 */
inline std::vector<std::uint8_t> encode_string_map_entry(const std::string& key,
                                                        const std::string& value) {
    std::vector<std::uint8_t> out;
    protobuf_wire::encode_string_field(out, 1, key);
    protobuf_wire::encode_string_field(out, 2, value);
    return out;
}

inline std::vector<std::uint8_t> encode_span(const span& s) {
    std::vector<std::uint8_t> out;
    protobuf_wire::encode_bytes_field(out, 1, s.trace_id);
    protobuf_wire::encode_bytes_field(out, 2, s.parent_id);
    protobuf_wire::encode_bytes_field(out, 3, s.id);
    protobuf_wire::encode_enum_field(out, 4, static_cast<std::int32_t>(s.kind));
    protobuf_wire::encode_string_field(out, 5, s.name);
    protobuf_wire::encode_fixed64_field(out, 6, s.timestamp);
    protobuf_wire::encode_uint64_field(out, 7, s.duration);
    if (!s.local_endpoint.empty()) {
        auto serialized = encode_endpoint(s.local_endpoint);
        protobuf_wire::encode_message_field(out, 8, serialized);
    }
    if (!s.remote_endpoint.empty()) {
        auto serialized = encode_endpoint(s.remote_endpoint);
        protobuf_wire::encode_message_field(out, 9, serialized);
    }
    for (const auto& ann : s.annotations) {
        auto serialized = encode_annotation(ann);
        protobuf_wire::encode_message_field(out, 10, serialized);
    }
    for (const auto& [key, value] : s.tags) {
        auto entry = encode_string_map_entry(key, value);
        protobuf_wire::encode_message_field(out, 11, entry);
    }
    protobuf_wire::encode_bool_field(out, 12, s.debug);
    protobuf_wire::encode_bool_field(out, 13, s.shared);
    return out;
}

inline std::vector<std::uint8_t> encode_list_of_spans(const list_of_spans& list) {
    std::vector<std::uint8_t> out;
    for (const auto& s : list.spans) {
        auto serialized = encode_span(s);
        protobuf_wire::encode_message_field(out, 1, serialized);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Decoders (used by round-trip tests)
// ---------------------------------------------------------------------------

inline bool decode_endpoint(const std::uint8_t* data, std::size_t size,
                            endpoint& out) {
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
            case 2:
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                if (!r.read_bytes(out.ipv4)) return false;
                break;
            case 3:
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                if (!r.read_bytes(out.ipv6)) return false;
                break;
            case 4: {
                if (wt != protobuf_wire::wire_type::varint) return false;
                auto v = r.read_varint();
                if (!v) return false;
                out.port = static_cast<std::int32_t>(*v);
                break;
            }
            default:
                if (!r.skip_field(wt)) return false;
        }
    }
    return true;
}

inline bool decode_annotation(const std::uint8_t* data, std::size_t size,
                              annotation& out) {
    protobuf_wire::reader r(data, size);
    while (!r.eof()) {
        std::uint32_t field_number;
        protobuf_wire::wire_type wt;
        if (!protobuf_wire::decode_tag(r, field_number, wt)) return false;
        switch (field_number) {
            case 1: {
                if (wt != protobuf_wire::wire_type::fixed64) return false;
                auto v = r.read_fixed64();
                if (!v) return false;
                out.timestamp = *v;
                break;
            }
            case 2:
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                if (!r.read_string(out.value)) return false;
                break;
            default:
                if (!r.skip_field(wt)) return false;
        }
    }
    return true;
}

inline bool decode_map_entry(const std::uint8_t* data, std::size_t size,
                             std::string& key, std::string& value) {
    protobuf_wire::reader r(data, size);
    while (!r.eof()) {
        std::uint32_t field_number;
        protobuf_wire::wire_type wt;
        if (!protobuf_wire::decode_tag(r, field_number, wt)) return false;
        if (wt != protobuf_wire::wire_type::length_delimited) {
            if (!r.skip_field(wt)) return false;
            continue;
        }
        if (field_number == 1) {
            if (!r.read_string(key)) return false;
        } else if (field_number == 2) {
            if (!r.read_string(value)) return false;
        } else {
            const std::uint8_t* ptr; std::size_t len;
            if (!r.read_length_delimited(&ptr, &len)) return false;
        }
    }
    return true;
}

inline bool decode_span(const std::uint8_t* data, std::size_t size, span& out) {
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
                if (!r.read_bytes(out.parent_id)) return false;
                break;
            case 3:
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                if (!r.read_bytes(out.id)) return false;
                break;
            case 4: {
                if (wt != protobuf_wire::wire_type::varint) return false;
                auto v = r.read_varint();
                if (!v) return false;
                out.kind = static_cast<span_kind>(*v);
                break;
            }
            case 5:
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                if (!r.read_string(out.name)) return false;
                break;
            case 6: {
                if (wt != protobuf_wire::wire_type::fixed64) return false;
                auto v = r.read_fixed64();
                if (!v) return false;
                out.timestamp = *v;
                break;
            }
            case 7: {
                if (wt != protobuf_wire::wire_type::varint) return false;
                auto v = r.read_varint();
                if (!v) return false;
                out.duration = *v;
                break;
            }
            case 8: {
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                const std::uint8_t* ptr; std::size_t len;
                if (!r.read_length_delimited(&ptr, &len)) return false;
                if (!decode_endpoint(ptr, len, out.local_endpoint)) return false;
                break;
            }
            case 9: {
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                const std::uint8_t* ptr; std::size_t len;
                if (!r.read_length_delimited(&ptr, &len)) return false;
                if (!decode_endpoint(ptr, len, out.remote_endpoint)) return false;
                break;
            }
            case 10: {
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                const std::uint8_t* ptr; std::size_t len;
                if (!r.read_length_delimited(&ptr, &len)) return false;
                annotation ann;
                if (!decode_annotation(ptr, len, ann)) return false;
                out.annotations.push_back(std::move(ann));
                break;
            }
            case 11: {
                if (wt != protobuf_wire::wire_type::length_delimited) return false;
                const std::uint8_t* ptr; std::size_t len;
                if (!r.read_length_delimited(&ptr, &len)) return false;
                std::string key, value;
                if (!decode_map_entry(ptr, len, key, value)) return false;
                out.tags.emplace_back(std::move(key), std::move(value));
                break;
            }
            case 12: {
                if (wt != protobuf_wire::wire_type::varint) return false;
                auto v = r.read_varint();
                if (!v) return false;
                out.debug = (*v != 0);
                break;
            }
            case 13: {
                if (wt != protobuf_wire::wire_type::varint) return false;
                auto v = r.read_varint();
                if (!v) return false;
                out.shared = (*v != 0);
                break;
            }
            default:
                if (!r.skip_field(wt)) return false;
        }
    }
    return true;
}

inline bool decode_list_of_spans(const std::uint8_t* data, std::size_t size,
                                 list_of_spans& out) {
    protobuf_wire::reader r(data, size);
    while (!r.eof()) {
        std::uint32_t field_number;
        protobuf_wire::wire_type wt;
        if (!protobuf_wire::decode_tag(r, field_number, wt)) return false;
        if (field_number == 1 && wt == protobuf_wire::wire_type::length_delimited) {
            const std::uint8_t* ptr; std::size_t len;
            if (!r.read_length_delimited(&ptr, &len)) return false;
            span s;
            if (!decode_span(ptr, len, s)) return false;
            out.spans.push_back(std::move(s));
        } else {
            if (!r.skip_field(wt)) return false;
        }
    }
    return true;
}

}}}  // namespace kcenon::monitoring::zipkin_proto

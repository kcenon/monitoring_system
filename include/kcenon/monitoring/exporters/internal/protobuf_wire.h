#pragma once

// BSD 3-Clause License
// Copyright (c) 2025
// See the LICENSE file in the project root for full license information.

/**
 * @file protobuf_wire.h
 * @brief Zero-dependency protobuf wire-format encoder and decoder primitives.
 *
 * Implements just enough of the protobuf3 wire format (varint, fixed64, length-
 * delimited) to serialize and deserialize Jaeger api_v2 and Zipkin proto3 span
 * messages without pulling in the full protobuf runtime. Only the wire types
 * actually exercised by the Jaeger / Zipkin span schemas are implemented.
 *
 * References:
 * - https://protobuf.dev/programming-guides/encoding/
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace kcenon { namespace monitoring { namespace protobuf_wire {

enum class wire_type : std::uint8_t {
    varint           = 0,  ///< int32/int64/uint32/uint64/sint*/bool/enum
    fixed64          = 1,  ///< fixed64/sfixed64/double
    length_delimited = 2,  ///< string/bytes/embedded messages/packed repeated
    fixed32          = 5   ///< fixed32/sfixed32/float
};

/**
 * @brief Encode an unsigned varint into the buffer.
 */
inline void encode_varint(std::vector<std::uint8_t>& out, std::uint64_t value) {
    while (value >= 0x80) {
        out.push_back(static_cast<std::uint8_t>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    out.push_back(static_cast<std::uint8_t>(value));
}

/**
 * @brief Encode a tag (field_number << 3 | wire_type) as a varint.
 */
inline void encode_tag(std::vector<std::uint8_t>& out,
                       std::uint32_t field_number,
                       wire_type wt) {
    encode_varint(out,
                  (static_cast<std::uint64_t>(field_number) << 3) |
                      static_cast<std::uint64_t>(wt));
}

/**
 * @brief Encode a fixed64 little-endian value (used for Zipkin timestamps).
 */
inline void encode_fixed64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

/**
 * @brief Encode a length-delimited byte sequence.
 */
inline void encode_length_delimited(std::vector<std::uint8_t>& out,
                                    const std::uint8_t* data,
                                    std::size_t size) {
    encode_varint(out, static_cast<std::uint64_t>(size));
    out.insert(out.end(), data, data + size);
}

/** @brief Encode a string field. */
inline void encode_string_field(std::vector<std::uint8_t>& out,
                                std::uint32_t field_number,
                                const std::string& value) {
    if (value.empty()) {
        return;  // proto3 defaults: skip empty string
    }
    encode_tag(out, field_number, wire_type::length_delimited);
    encode_length_delimited(
        out,
        reinterpret_cast<const std::uint8_t*>(value.data()),
        value.size());
}

/** @brief Encode a bytes field. */
inline void encode_bytes_field(std::vector<std::uint8_t>& out,
                               std::uint32_t field_number,
                               const std::vector<std::uint8_t>& value) {
    if (value.empty()) {
        return;  // proto3 defaults: skip empty bytes
    }
    encode_tag(out, field_number, wire_type::length_delimited);
    encode_length_delimited(out, value.data(), value.size());
}

/** @brief Encode a uint32 / uint64 / int64 varint field (skips zero). */
inline void encode_uint64_field(std::vector<std::uint8_t>& out,
                                std::uint32_t field_number,
                                std::uint64_t value) {
    if (value == 0) {
        return;
    }
    encode_tag(out, field_number, wire_type::varint);
    encode_varint(out, value);
}

/** @brief Encode a uint32 field that is not allowed to be skipped even if zero. */
inline void encode_uint64_field_always(std::vector<std::uint8_t>& out,
                                       std::uint32_t field_number,
                                       std::uint64_t value) {
    encode_tag(out, field_number, wire_type::varint);
    encode_varint(out, value);
}

/** @brief Encode an enum field (always written when nonzero). */
inline void encode_enum_field(std::vector<std::uint8_t>& out,
                              std::uint32_t field_number,
                              std::int32_t value) {
    if (value == 0) {
        return;
    }
    encode_tag(out, field_number, wire_type::varint);
    encode_varint(out, static_cast<std::uint64_t>(value));
}

/** @brief Encode a bool field (proto3 skips false). */
inline void encode_bool_field(std::vector<std::uint8_t>& out,
                              std::uint32_t field_number,
                              bool value) {
    if (!value) {
        return;
    }
    encode_tag(out, field_number, wire_type::varint);
    encode_varint(out, 1);
}

/** @brief Encode a fixed64 field. */
inline void encode_fixed64_field(std::vector<std::uint8_t>& out,
                                 std::uint32_t field_number,
                                 std::uint64_t value) {
    if (value == 0) {
        return;
    }
    encode_tag(out, field_number, wire_type::fixed64);
    encode_fixed64(out, value);
}

/** @brief Encode an embedded message field given its pre-serialized bytes. */
inline void encode_message_field(std::vector<std::uint8_t>& out,
                                 std::uint32_t field_number,
                                 const std::vector<std::uint8_t>& serialized) {
    encode_tag(out, field_number, wire_type::length_delimited);
    encode_length_delimited(out, serialized.data(), serialized.size());
}

// ---------------------------------------------------------------------------
// Decoder
// ---------------------------------------------------------------------------

/**
 * @brief Minimal protobuf wire reader used for round-trip tests.
 */
class reader {
public:
    reader(const std::uint8_t* data, std::size_t size)
        : data_(data), size_(size), pos_(0) {}

    bool eof() const { return pos_ >= size_; }
    std::size_t position() const { return pos_; }
    std::size_t size() const { return size_; }

    std::optional<std::uint64_t> read_varint() {
        std::uint64_t result = 0;
        int shift = 0;
        while (pos_ < size_) {
            std::uint8_t byte = data_[pos_++];
            result |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0) {
                return result;
            }
            shift += 7;
            if (shift > 63) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::optional<std::uint64_t> read_fixed64() {
        if (pos_ + 8 > size_) {
            return std::nullopt;
        }
        std::uint64_t result = 0;
        for (int i = 0; i < 8; ++i) {
            result |= static_cast<std::uint64_t>(data_[pos_++]) << (i * 8);
        }
        return result;
    }

    std::optional<std::uint32_t> read_fixed32() {
        if (pos_ + 4 > size_) {
            return std::nullopt;
        }
        std::uint32_t result = 0;
        for (int i = 0; i < 4; ++i) {
            result |= static_cast<std::uint32_t>(data_[pos_++]) << (i * 8);
        }
        return result;
    }

    /**
     * @brief Read a length-delimited payload. Returns pointer into the
     *        underlying buffer and the length. The pointer is valid for the
     *        lifetime of the wrapped buffer.
     */
    bool read_length_delimited(const std::uint8_t** out_ptr,
                               std::size_t* out_len) {
        auto len = read_varint();
        if (!len) return false;
        if (pos_ + *len > size_) return false;
        *out_ptr = data_ + pos_;
        *out_len = static_cast<std::size_t>(*len);
        pos_ += *len;
        return true;
    }

    bool read_string(std::string& out) {
        const std::uint8_t* ptr = nullptr;
        std::size_t len = 0;
        if (!read_length_delimited(&ptr, &len)) return false;
        out.assign(reinterpret_cast<const char*>(ptr), len);
        return true;
    }

    bool read_bytes(std::vector<std::uint8_t>& out) {
        const std::uint8_t* ptr = nullptr;
        std::size_t len = 0;
        if (!read_length_delimited(&ptr, &len)) return false;
        out.assign(ptr, ptr + len);
        return true;
    }

    /** @brief Skip a field whose wire type is given. */
    bool skip_field(wire_type wt) {
        switch (wt) {
            case wire_type::varint:
                return read_varint().has_value();
            case wire_type::fixed64:
                return read_fixed64().has_value();
            case wire_type::length_delimited: {
                const std::uint8_t* ptr;
                std::size_t len;
                return read_length_delimited(&ptr, &len);
            }
            case wire_type::fixed32:
                return read_fixed32().has_value();
        }
        return false;
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_;
};

/**
 * @brief Decode a tag into (field_number, wire_type).
 */
inline bool decode_tag(reader& r,
                       std::uint32_t& field_number,
                       wire_type& wt) {
    auto tag = r.read_varint();
    if (!tag) return false;
    field_number = static_cast<std::uint32_t>(*tag >> 3);
    wt = static_cast<wire_type>(*tag & 0x07);
    return true;
}

// ---------------------------------------------------------------------------
// Hex helpers for Jaeger/Zipkin trace and span IDs
// ---------------------------------------------------------------------------

/**
 * @brief Decode a hexadecimal string into bytes. Odd-length strings are
 *        zero-padded on the left; non-hex characters yield an empty vector.
 */
inline std::vector<std::uint8_t> hex_to_bytes(const std::string& hex) {
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };
    std::string s = hex;
    if (s.size() % 2 == 1) {
        s.insert(s.begin(), '0');
    }
    std::vector<std::uint8_t> out;
    out.reserve(s.size() / 2);
    for (std::size_t i = 0; i < s.size(); i += 2) {
        int hi = nibble(s[i]);
        int lo = nibble(s[i + 1]);
        if (hi < 0 || lo < 0) {
            return {};  // invalid; caller treats empty as absent
        }
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return out;
}

/**
 * @brief Encode raw bytes as a lowercase hex string.
 */
inline std::string bytes_to_hex(const std::vector<std::uint8_t>& bytes) {
    static const char hex_chars[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (std::uint8_t b : bytes) {
        out.push_back(hex_chars[(b >> 4) & 0x0F]);
        out.push_back(hex_chars[b & 0x0F]);
    }
    return out;
}

/**
 * @brief Left-pad bytes to a target width. Used to normalize 8-byte trace IDs
 *        to Jaeger's 16-byte on-wire width and similar cases.
 */
inline std::vector<std::uint8_t> left_pad(const std::vector<std::uint8_t>& in,
                                          std::size_t width) {
    if (in.size() >= width) return in;
    std::vector<std::uint8_t> out(width, 0);
    std::memcpy(out.data() + (width - in.size()), in.data(), in.size());
    return out;
}

}}}  // namespace kcenon::monitoring::protobuf_wire

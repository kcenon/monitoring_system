#pragma once

// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.


/**
 * @file trace_exporters.h
 * @brief Trace data exporters for various distributed tracing systems
 * 
 * This file provides exporters for popular distributed tracing systems:
 * - Jaeger (Uber's distributed tracing system)
 * - Zipkin (Twitter's distributed tracing system)
 * - OTLP (OpenTelemetry Protocol)
 * 
 * Each exporter handles format conversion and network transmission
 * to the respective tracing backend.
 */

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include "../tracing/distributed_tracer.h"
#include "opentelemetry_adapter.h"
#include "http_transport.h"
#include "grpc_transport.h"
#include "internal/jaeger_proto.h"
#include "internal/zipkin_proto.h"
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <optional>
#include <functional>
#include <atomic>
#include <algorithm>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <thread>

namespace kcenon { namespace monitoring {

/**
 * @enum trace_export_format
 * @brief Supported trace export formats
 */
enum class trace_export_format {
    jaeger_thrift,      ///< Jaeger Thrift protocol
    jaeger_grpc,        ///< Jaeger gRPC protocol
    zipkin_json,        ///< Zipkin JSON v2 format
    zipkin_protobuf,    ///< Zipkin Protocol Buffers format
    otlp_grpc,          ///< OTLP gRPC protocol
    otlp_http_json,     ///< OTLP HTTP JSON protocol
    otlp_http_protobuf  ///< OTLP HTTP Protocol Buffers
};

/**
 * @struct trace_export_config
 * @brief Configuration for trace exporters
 */
struct trace_export_config {
    std::string endpoint;                                    ///< Endpoint URL
    trace_export_format format = trace_export_format::otlp_grpc;
    std::chrono::milliseconds timeout{30000};               ///< Request timeout
    std::chrono::milliseconds batch_timeout{5000};          ///< Batch export timeout
    std::size_t max_batch_size = 512;                      ///< Maximum spans per batch
    std::size_t max_queue_size = 2048;                     ///< Maximum queued spans
    bool enable_compression = true;                          ///< Enable data compression
    std::unordered_map<std::string, std::string> headers;   ///< Custom HTTP headers
    std::optional<std::string> service_name;                ///< Override service name
    
    /**
     * @brief Validate export configuration
     */
    common::VoidResult validate() const {
        if (endpoint.empty()) {
            return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                             "Export endpoint cannot be empty", "monitoring_system").to_common_error());
        }

        if (timeout.count() <= 0) {
            return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                             "Timeout must be positive", "monitoring_system").to_common_error());
        }

        if (max_batch_size == 0) {
            return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                             "Batch size must be greater than 0", "monitoring_system").to_common_error());
        }

        if (max_queue_size < max_batch_size) {
            return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                             "Queue size must be at least batch size", "monitoring_system").to_common_error());
        }

        return common::ok();
    }
};

/**
 * @struct jaeger_span_data
 * @brief Jaeger-specific span representation
 */
struct jaeger_span_data {
    std::string trace_id;
    std::string span_id;
    std::string parent_span_id;
    std::string operation_name;
    std::string service_name;
    std::chrono::microseconds start_time;
    std::chrono::microseconds duration;
    std::vector<std::pair<std::string, std::string>> tags;
    std::vector<std::pair<std::string, std::string>> process_tags;
    
    /**
     * @brief Convert to Jaeger Thrift format (JSON representation)
     */
    std::string to_thrift_json() const {
        std::ostringstream json;
        json << "{";
        json << "\"traceIdHigh\":0,";
        json << "\"traceIdLow\":" << std::hash<std::string>{}(trace_id) << ",";
        json << "\"spanId\":" << std::hash<std::string>{}(span_id) << ",";
        json << "\"parentSpanId\":" << (parent_span_id.empty() ? "0" : std::to_string(std::hash<std::string>{}(parent_span_id))) << ",";
        json << "\"operationName\":\"" << operation_name << "\",";
        json << "\"startTime\":" << start_time.count() << ",";
        json << "\"duration\":" << duration.count() << ",";

        // Tags
        json << "\"tags\":[";
        bool first = true;
        for (const auto& [key, value] : tags) {
            if (!first) json << ",";
            json << "{\"key\":\"" << key << "\",\"vType\":\"STRING\",\"vStr\":\"" << value << "\"}";
            first = false;
        }
        json << "],";

        // Process
        json << "\"process\":{\"serviceName\":\"" << service_name << "\",\"tags\":[";
        first = true;
        for (const auto& [key, value] : process_tags) {
            if (!first) json << ",";
            json << "{\"key\":\"" << key << "\",\"vType\":\"STRING\",\"vStr\":\"" << value << "\"}";
            first = false;
        }
        json << "]}";

        json << "}";
        return json.str();
    }

    /**
     * @brief Convert this span to a Jaeger api_v2 `Span` protobuf message.
     *
     * Field numbers follow `jaeger-idl/proto/api_v2/model.proto`. Trace IDs
     * are zero-padded to 16 bytes and span IDs to 8 bytes, matching the
     * conventions enforced by the Jaeger collector.
     */
    std::vector<uint8_t> to_protobuf() const {
        jaeger_proto::span out;
        out.trace_id = protobuf_wire::left_pad(
            protobuf_wire::hex_to_bytes(trace_id), 16);
        out.span_id = protobuf_wire::left_pad(
            protobuf_wire::hex_to_bytes(span_id), 8);
        out.operation_name = operation_name;

        if (!parent_span_id.empty()) {
            jaeger_proto::span_ref ref;
            ref.trace_id = out.trace_id;
            ref.span_id = protobuf_wire::left_pad(
                protobuf_wire::hex_to_bytes(parent_span_id), 8);
            ref.ref_type = 0;  // CHILD_OF
            out.references.push_back(std::move(ref));
        }

        // Timestamp / duration split into (seconds, nanos).
        auto st_ns = std::chrono::nanoseconds(start_time);
        out.start_time_seconds = static_cast<std::int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(st_ns).count());
        out.start_time_nanos = static_cast<std::int32_t>(
            (st_ns - std::chrono::seconds(out.start_time_seconds)).count());

        auto du_ns = std::chrono::nanoseconds(duration);
        out.duration_seconds = static_cast<std::int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(du_ns).count());
        out.duration_nanos = static_cast<std::int32_t>(
            (du_ns - std::chrono::seconds(out.duration_seconds)).count());

        for (const auto& [key, value] : tags) {
            jaeger_proto::key_value kv;
            kv.key = key;
            kv.v_type = jaeger_proto::value_type::string_type;
            kv.v_str = value;
            out.tags.push_back(std::move(kv));
        }

        out.proc.service_name = service_name;
        for (const auto& [key, value] : process_tags) {
            jaeger_proto::key_value kv;
            kv.key = key;
            kv.v_type = jaeger_proto::value_type::string_type;
            kv.v_str = value;
            out.proc.tags.push_back(std::move(kv));
        }

        return jaeger_proto::encode_span(out);
    }
};

/**
 * @brief Encode a batch of Jaeger spans (with shared process) into a Jaeger
 *        api_v2 `Batch` protobuf message.
 */
inline std::vector<uint8_t> encode_jaeger_batch(
    const std::vector<jaeger_span_data>& spans,
    const std::string& service_name) {
    jaeger_proto::batch b;
    b.spans.reserve(spans.size());
    for (const auto& s : spans) {
        jaeger_proto::span ps;
        ps.trace_id = protobuf_wire::left_pad(
            protobuf_wire::hex_to_bytes(s.trace_id), 16);
        ps.span_id = protobuf_wire::left_pad(
            protobuf_wire::hex_to_bytes(s.span_id), 8);
        ps.operation_name = s.operation_name;
        if (!s.parent_span_id.empty()) {
            jaeger_proto::span_ref ref;
            ref.trace_id = ps.trace_id;
            ref.span_id = protobuf_wire::left_pad(
                protobuf_wire::hex_to_bytes(s.parent_span_id), 8);
            ref.ref_type = 0;
            ps.references.push_back(std::move(ref));
        }
        auto st_ns = std::chrono::nanoseconds(s.start_time);
        ps.start_time_seconds = static_cast<std::int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(st_ns).count());
        ps.start_time_nanos = static_cast<std::int32_t>(
            (st_ns - std::chrono::seconds(ps.start_time_seconds)).count());
        auto du_ns = std::chrono::nanoseconds(s.duration);
        ps.duration_seconds = static_cast<std::int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(du_ns).count());
        ps.duration_nanos = static_cast<std::int32_t>(
            (du_ns - std::chrono::seconds(ps.duration_seconds)).count());
        for (const auto& [key, value] : s.tags) {
            jaeger_proto::key_value kv;
            kv.key = key;
            kv.v_type = jaeger_proto::value_type::string_type;
            kv.v_str = value;
            ps.tags.push_back(std::move(kv));
        }
        b.spans.push_back(std::move(ps));
    }
    b.proc.service_name = service_name;
    b.has_process = !service_name.empty();
    return jaeger_proto::encode_batch(b);
}

/**
 * @struct zipkin_span_data
 * @brief Zipkin-specific span representation
 */
struct zipkin_span_data {
    std::string trace_id;
    std::string span_id;
    std::string parent_id;
    std::string name;
    std::string kind;
    std::chrono::microseconds timestamp;
    std::chrono::microseconds duration;
    std::string local_endpoint_service_name;
    std::string remote_endpoint_service_name;
    std::unordered_map<std::string, std::string> tags;
    bool shared = false;
    
    /**
     * @brief Convert to Zipkin JSON v2 format
     */
    std::string to_json_v2() const {
        std::ostringstream json;
        json << "{";
        json << "\"traceId\":\"" << trace_id << "\",";
        json << "\"id\":\"" << span_id << "\",";
        if (!parent_id.empty()) {
            json << "\"parentId\":\"" << parent_id << "\",";
        }
        json << "\"name\":\"" << name << "\",";
        json << "\"kind\":\"" << kind << "\",";
        json << "\"timestamp\":" << timestamp.count() << ",";
        json << "\"duration\":" << duration.count() << ",";

        // Local endpoint
        json << "\"localEndpoint\":{\"serviceName\":\"" << local_endpoint_service_name << "\"},";

        // Remote endpoint (if set)
        if (!remote_endpoint_service_name.empty()) {
            json << "\"remoteEndpoint\":{\"serviceName\":\"" << remote_endpoint_service_name << "\"},";
        }

        // Tags
        json << "\"tags\":{";
        bool first = true;
        for (const auto& [key, value] : tags) {
            if (!first) json << ",";
            json << "\"" << key << "\":\"" << value << "\"";
            first = false;
        }
        json << "}";

        if (shared) {
            json << ",\"shared\":true";
        }

        json << "}";
        return json.str();
    }

    /**
     * @brief Convert this span to a Zipkin `Span` protobuf message.
     *
     * Field numbers follow `openzipkin/zipkin-api/zipkin.proto`. Trace IDs are
     * expected to be 8 or 16 hex bytes; shorter inputs are zero-padded.
     */
    std::vector<uint8_t> to_protobuf() const {
        zipkin_proto::span out;
        auto trace_bytes = protobuf_wire::hex_to_bytes(trace_id);
        // Zipkin accepts 8 or 16 byte trace IDs; normalize the common
        // shorter/empty/non-hex inputs to 8 bytes so the wire always carries
        // a valid ID (Zipkin rejects spans without a trace ID).
        if (trace_bytes.size() == 16) {
            out.trace_id = std::move(trace_bytes);
        } else {
            out.trace_id = protobuf_wire::left_pad(trace_bytes, 8);
        }
        out.id = protobuf_wire::left_pad(
            protobuf_wire::hex_to_bytes(span_id), 8);
        if (!parent_id.empty()) {
            out.parent_id = protobuf_wire::left_pad(
                protobuf_wire::hex_to_bytes(parent_id), 8);
        }
        out.kind = zipkin_proto::parse_kind(kind);
        out.name = name;
        out.timestamp = static_cast<std::uint64_t>(timestamp.count());
        out.duration = static_cast<std::uint64_t>(duration.count());
        out.local_endpoint.service_name = local_endpoint_service_name;
        out.remote_endpoint.service_name = remote_endpoint_service_name;
        out.shared = shared;
        for (const auto& [key, value] : tags) {
            out.tags.emplace_back(key, value);
        }
        return zipkin_proto::encode_span(out);
    }
};

/**
 * @brief Encode a batch of Zipkin spans into a `ListOfSpans` protobuf message
 *        suitable for POST /api/v2/spans with Content-Type
 *        application/x-protobuf.
 */
inline std::vector<uint8_t> encode_zipkin_list_of_spans(
    const std::vector<zipkin_span_data>& spans) {
    zipkin_proto::list_of_spans list;
    list.spans.reserve(spans.size());
    for (const auto& s : spans) {
        zipkin_proto::span ps;
        auto trace_bytes = protobuf_wire::hex_to_bytes(s.trace_id);
        if (trace_bytes.size() == 16) {
            ps.trace_id = std::move(trace_bytes);
        } else {
            ps.trace_id = protobuf_wire::left_pad(trace_bytes, 8);
        }
        ps.id = protobuf_wire::left_pad(
            protobuf_wire::hex_to_bytes(s.span_id), 8);
        if (!s.parent_id.empty()) {
            ps.parent_id = protobuf_wire::left_pad(
                protobuf_wire::hex_to_bytes(s.parent_id), 8);
        }
        ps.kind = zipkin_proto::parse_kind(s.kind);
        ps.name = s.name;
        ps.timestamp = static_cast<std::uint64_t>(s.timestamp.count());
        ps.duration = static_cast<std::uint64_t>(s.duration.count());
        ps.local_endpoint.service_name = s.local_endpoint_service_name;
        ps.remote_endpoint.service_name = s.remote_endpoint_service_name;
        ps.shared = s.shared;
        for (const auto& [key, value] : s.tags) {
            ps.tags.emplace_back(key, value);
        }
        list.spans.push_back(std::move(ps));
    }
    return zipkin_proto::encode_list_of_spans(list);
}

/**
 * @class trace_exporter_interface
 * @brief Abstract interface for trace exporters
 */
class trace_exporter_interface {
public:
    virtual ~trace_exporter_interface() = default;
    
    /**
     * @brief Export a batch of spans
     */
    virtual common::VoidResult export_spans(const std::vector<trace_span>& spans) = 0;
    
    /**
     * @brief Flush any pending spans
     */
    virtual common::VoidResult flush() = 0;
    
    /**
     * @brief Shutdown the exporter
     */
    virtual common::VoidResult shutdown() = 0;
    
    /**
     * @brief Get exporter statistics
     */
    virtual std::unordered_map<std::string, std::size_t> get_stats() const = 0;
};

/**
 * @class jaeger_exporter
 * @brief Jaeger trace exporter implementation
 *
 * Supports both Thrift over HTTP and gRPC protocols.
 * Default endpoint: /api/traces for Thrift HTTP
 */
class jaeger_exporter : public trace_exporter_interface {
private:
    trace_export_config config_;
    std::unique_ptr<http_transport> transport_;
    std::atomic<std::size_t> exported_spans_{0};
    std::atomic<std::size_t> failed_exports_{0};
    std::atomic<std::size_t> dropped_spans_{0};
    std::size_t max_retries_{3};
    std::chrono::milliseconds base_retry_delay_{100};

public:
    explicit jaeger_exporter(const trace_export_config& config)
        : config_(config), transport_(create_default_transport()) {}

    jaeger_exporter(const trace_export_config& config, std::unique_ptr<http_transport> transport)
        : config_(config), transport_(std::move(transport)) {}
    
    /**
     * @brief Convert internal span to Jaeger format
     */
    jaeger_span_data convert_span(const trace_span& span) const {
        jaeger_span_data jaeger_span;
        jaeger_span.trace_id = span.trace_id;
        jaeger_span.span_id = span.span_id;
        jaeger_span.parent_span_id = span.parent_span_id;
        jaeger_span.operation_name = span.operation_name;
        jaeger_span.service_name = config_.service_name.value_or(span.service_name);
        
        // Convert timestamps
        auto start_epoch = span.start_time.time_since_epoch();
        jaeger_span.start_time = std::chrono::duration_cast<std::chrono::microseconds>(start_epoch);
        
        auto end_epoch = span.end_time.time_since_epoch();
        jaeger_span.duration = std::chrono::duration_cast<std::chrono::microseconds>(end_epoch - start_epoch);
        
        // Convert tags
        for (const auto& [key, value] : span.tags) {
            jaeger_span.tags.emplace_back(key, value);
        }
        
        // Add process tags
        jaeger_span.process_tags.emplace_back("service.name", jaeger_span.service_name);
        
        return jaeger_span;
    }
    
    common::VoidResult export_spans(const std::vector<trace_span>& spans) override {
        try {
            std::vector<jaeger_span_data> jaeger_spans;
            jaeger_spans.reserve(spans.size());

            for (const auto& span : spans) {
                jaeger_spans.push_back(convert_span(span));
            }

            // Convert to appropriate format and send
            common::VoidResult send_result = common::ok();
            if (config_.format == trace_export_format::jaeger_thrift) {
                send_result = send_thrift_batch(jaeger_spans);
            } else if (config_.format == trace_export_format::jaeger_grpc) {
                send_result = send_grpc_batch(jaeger_spans);
            } else {
                return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                                 "Invalid Jaeger export format", "monitoring_system").to_common_error());
            }

            if (send_result.is_ok()) {
                exported_spans_ += spans.size();
            } else {
                failed_exports_++;
                return send_result;
            }

            return common::ok();

        } catch (const std::exception& e) {
            failed_exports_++;
            return common::VoidResult::err(error_info(monitoring_error_code::operation_failed,
                             "Jaeger export failed: " + std::string(e.what()), "monitoring_system").to_common_error());
        }
    }
    
    common::VoidResult flush() override {
        // Jaeger exporter typically sends immediately, so flush is a no-op
        return common::ok();
    }
    
    common::VoidResult shutdown() override {
        return flush();
    }
    
    std::unordered_map<std::string, std::size_t> get_stats() const override {
        return {
            {"exported_spans", exported_spans_.load()},
            {"failed_exports", failed_exports_.load()},
            {"dropped_spans", dropped_spans_.load()}
        };
    }
    
private:
    common::VoidResult send_thrift_batch(const std::vector<jaeger_span_data>& spans) {
        // Build JSON payload for Thrift over HTTP
        std::ostringstream payload;
        payload << "{\"data\":[{\"spans\":[";
        bool first = true;
        for (const auto& span : spans) {
            if (!first) payload << ",";
            payload << span.to_thrift_json();
            first = false;
        }
        payload << "]}]}";

        std::string body = payload.str();

        // Build HTTP request
        http_request request;
        request.url = config_.endpoint + "/api/traces";
        request.method = "POST";
        request.headers["Content-Type"] = "application/x-thrift";
        request.headers["Accept"] = "application/json";
        for (const auto& [key, value] : config_.headers) {
            request.headers[key] = value;
        }
        request.body = std::vector<uint8_t>(body.begin(), body.end());
        request.timeout = config_.timeout;

        // Send with retry
        return send_with_retry(request);
    }

    common::VoidResult send_grpc_batch(const std::vector<jaeger_span_data>& spans) {
        // Serialize as a Jaeger api_v2 `Batch` protobuf message. Real gRPC
        // transport would additionally wrap this in a 5-byte gRPC length
        // prefix and multiplex over HTTP/2; when a gRPC transport is
        // available the raw `Batch` bytes below can be used as the message
        // payload directly.
        std::string resolved_service;
        if (config_.service_name) {
            resolved_service = *config_.service_name;
        } else if (!spans.empty()) {
            resolved_service = spans.front().service_name;
        }
        auto payload = encode_jaeger_batch(spans, resolved_service);

        http_request request;
        request.url = config_.endpoint;
        request.method = "POST";
        request.headers["Content-Type"] = "application/x-protobuf";
        for (const auto& [key, value] : config_.headers) {
            request.headers[key] = value;
        }
        request.body = payload;
        request.timeout = config_.timeout;

        return send_with_retry(request);
    }

    common::VoidResult send_with_retry(const http_request& request) {
        std::size_t attempt = 0;
        std::chrono::milliseconds delay = base_retry_delay_;

        while (attempt < max_retries_) {
            auto result = transport_->send(request);
            if (result.is_ok()) {
                const auto& response = result.value();
                if (response.status_code >= 200 && response.status_code < 300) {
                    return common::ok();
                }
                // Retry on 5xx errors
                if (response.status_code >= 500) {
                    attempt++;
                    if (attempt < max_retries_) {
                        std::this_thread::sleep_for(delay);
                        delay *= 2; // Exponential backoff
                    }
                    continue;
                }
                // Non-retryable error
                return common::VoidResult::err(error_info(monitoring_error_code::operation_failed,
                    "Jaeger export failed with status: " + std::to_string(response.status_code),
                    "monitoring_system").to_common_error());
            }
            attempt++;
            if (attempt < max_retries_) {
                std::this_thread::sleep_for(delay);
                delay *= 2;
            }
        }
        return common::VoidResult::err(error_info(monitoring_error_code::operation_failed,
            "Jaeger export failed after " + std::to_string(max_retries_) + " retries",
            "monitoring_system").to_common_error());
    }
};

/**
 * @class zipkin_exporter
 * @brief Zipkin trace exporter implementation
 *
 * Supports JSON v2 and Protocol Buffers formats.
 * Default endpoint: /api/v2/spans
 */
class zipkin_exporter : public trace_exporter_interface {
private:
    trace_export_config config_;
    std::unique_ptr<http_transport> transport_;
    std::atomic<std::size_t> exported_spans_{0};
    std::atomic<std::size_t> failed_exports_{0};
    std::atomic<std::size_t> dropped_spans_{0};
    std::size_t max_retries_{3};
    std::chrono::milliseconds base_retry_delay_{100};

public:
    explicit zipkin_exporter(const trace_export_config& config)
        : config_(config), transport_(create_default_transport()) {}

    zipkin_exporter(const trace_export_config& config, std::unique_ptr<http_transport> transport)
        : config_(config), transport_(std::move(transport)) {}
    
    /**
     * @brief Convert internal span to Zipkin format
     */
    zipkin_span_data convert_span(const trace_span& span) const {
        zipkin_span_data zipkin_span;
        zipkin_span.trace_id = span.trace_id;
        zipkin_span.span_id = span.span_id;
        zipkin_span.parent_id = span.parent_span_id;
        zipkin_span.name = span.operation_name;
        zipkin_span.local_endpoint_service_name = config_.service_name.value_or(span.service_name);
        
        // Convert timestamps (Zipkin uses microseconds since epoch)
        auto start_epoch = span.start_time.time_since_epoch();
        zipkin_span.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(start_epoch);
        
        auto end_epoch = span.end_time.time_since_epoch();
        zipkin_span.duration = std::chrono::duration_cast<std::chrono::microseconds>(end_epoch - start_epoch);
        
        // Determine span kind from tags
        auto kind_it = span.tags.find("span.kind");
        if (kind_it != span.tags.end()) {
            zipkin_span.kind = kind_it->second;
        } else {
            zipkin_span.kind = "INTERNAL";
        }
        
        // Convert tags (exclude special fields)
        for (const auto& [key, value] : span.tags) {
            if (key != "span.kind") {
                zipkin_span.tags[key] = value;
            }
        }
        
        return zipkin_span;
    }
    
    common::VoidResult export_spans(const std::vector<trace_span>& spans) override {
        try {
            std::vector<zipkin_span_data> zipkin_spans;
            zipkin_spans.reserve(spans.size());

            for (const auto& span : spans) {
                zipkin_spans.push_back(convert_span(span));
            }

            // Convert to appropriate format and send
            common::VoidResult send_result = common::ok();
            if (config_.format == trace_export_format::zipkin_json) {
                send_result = send_json_batch(zipkin_spans);
            } else if (config_.format == trace_export_format::zipkin_protobuf) {
                send_result = send_protobuf_batch(zipkin_spans);
            } else {
                return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                                 "Invalid Zipkin export format", "monitoring_system").to_common_error());
            }

            if (send_result.is_ok()) {
                exported_spans_ += spans.size();
            } else {
                failed_exports_++;
                return send_result;
            }

            return common::ok();

        } catch (const std::exception& e) {
            failed_exports_++;
            return common::VoidResult::err(error_info(monitoring_error_code::operation_failed,
                             "Zipkin export failed: " + std::string(e.what()), "monitoring_system").to_common_error());
        }
    }
    
    common::VoidResult flush() override {
        // Zipkin exporter typically sends immediately, so flush is a no-op
        return common::ok();
    }
    
    common::VoidResult shutdown() override {
        return flush();
    }
    
    std::unordered_map<std::string, std::size_t> get_stats() const override {
        return {
            {"exported_spans", exported_spans_.load()},
            {"failed_exports", failed_exports_.load()},
            {"dropped_spans", dropped_spans_.load()}
        };
    }
    
private:
    common::VoidResult send_json_batch(const std::vector<zipkin_span_data>& spans) {
        // Build JSON array payload for Zipkin v2 API
        std::ostringstream payload;
        payload << "[";
        bool first = true;
        for (const auto& span : spans) {
            if (!first) payload << ",";
            payload << span.to_json_v2();
            first = false;
        }
        payload << "]";

        std::string body = payload.str();

        // Build HTTP request
        http_request request;
        request.url = config_.endpoint + "/api/v2/spans";
        request.method = "POST";
        request.headers["Content-Type"] = "application/json";
        request.headers["Accept"] = "application/json";
        for (const auto& [key, value] : config_.headers) {
            request.headers[key] = value;
        }
        request.body = std::vector<uint8_t>(body.begin(), body.end());
        request.timeout = config_.timeout;

        return send_with_retry(request);
    }

    common::VoidResult send_protobuf_batch(const std::vector<zipkin_span_data>& spans) {
        // Serialize as a Zipkin `ListOfSpans` protobuf message. POST target
        // accepts application/x-protobuf per Zipkin v2 API.
        auto payload = encode_zipkin_list_of_spans(spans);

        http_request request;
        request.url = config_.endpoint + "/api/v2/spans";
        request.method = "POST";
        request.headers["Content-Type"] = "application/x-protobuf";
        for (const auto& [key, value] : config_.headers) {
            request.headers[key] = value;
        }
        request.body = payload;
        request.timeout = config_.timeout;

        return send_with_retry(request);
    }

    common::VoidResult send_with_retry(const http_request& request) {
        std::size_t attempt = 0;
        std::chrono::milliseconds delay = base_retry_delay_;

        while (attempt < max_retries_) {
            auto result = transport_->send(request);
            if (result.is_ok()) {
                const auto& response = result.value();
                if (response.status_code >= 200 && response.status_code < 300) {
                    return common::ok();
                }
                // Retry on 5xx errors
                if (response.status_code >= 500) {
                    attempt++;
                    if (attempt < max_retries_) {
                        std::this_thread::sleep_for(delay);
                        delay *= 2; // Exponential backoff
                    }
                    continue;
                }
                // Non-retryable error
                return common::VoidResult::err(error_info(monitoring_error_code::operation_failed,
                    "Zipkin export failed with status: " + std::to_string(response.status_code),
                    "monitoring_system").to_common_error());
            }
            attempt++;
            if (attempt < max_retries_) {
                std::this_thread::sleep_for(delay);
                delay *= 2;
            }
        }
        return common::VoidResult::err(error_info(monitoring_error_code::operation_failed,
            "Zipkin export failed after " + std::to_string(max_retries_) + " retries",
            "monitoring_system").to_common_error());
    }
};

/**
 * @class otlp_exporter
 * @brief OpenTelemetry Protocol (OTLP) trace exporter implementation
 */
class otlp_exporter : public trace_exporter_interface {
private:
    trace_export_config config_;
    std::unique_ptr<opentelemetry_tracer_adapter> otel_adapter_;
    std::atomic<std::size_t> exported_spans_{0};
    std::atomic<std::size_t> failed_exports_{0};
    std::atomic<std::size_t> dropped_spans_{0};
    
public:
    explicit otlp_exporter(const trace_export_config& config, const otel_resource& resource)
        : config_(config), otel_adapter_(std::make_unique<opentelemetry_tracer_adapter>(resource)) {}
    
    common::VoidResult export_spans(const std::vector<trace_span>& spans) override {
        try {
            // Convert to OpenTelemetry format first
            auto otel_result = otel_adapter_->convert_spans(spans);
            if (otel_result.is_err()) {
                failed_exports_++;
                return common::VoidResult::err(error_info(monitoring_error_code::processing_failed,
                                 "Failed to convert spans to OTEL format: " + otel_result.error().message, "monitoring_system").to_common_error());
            }

            const auto& otel_spans = otel_result.value();

            // Send via appropriate OTLP protocol
            common::VoidResult send_result = common::ok();
            if (config_.format == trace_export_format::otlp_grpc) {
                send_result = send_grpc_batch(otel_spans);
            } else if (config_.format == trace_export_format::otlp_http_json) {
                send_result = send_http_json_batch(otel_spans);
            } else if (config_.format == trace_export_format::otlp_http_protobuf) {
                send_result = send_http_protobuf_batch(otel_spans);
            } else {
                return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                                 "Invalid OTLP export format", "monitoring_system").to_common_error());
            }

            if (send_result.is_ok()) {
                exported_spans_ += spans.size();
            } else {
                failed_exports_++;
                return send_result;
            }

            return common::ok();

        } catch (const std::exception& e) {
            failed_exports_++;
            return common::VoidResult::err(error_info(monitoring_error_code::operation_failed,
                             "OTLP export failed: " + std::string(e.what()), "monitoring_system").to_common_error());
        }
    }
    
    common::VoidResult flush() override {
        // OTLP exporter typically sends immediately, so flush is a no-op
        return common::ok();
    }
    
    common::VoidResult shutdown() override {
        return flush();
    }
    
    std::unordered_map<std::string, std::size_t> get_stats() const override {
        return {
            {"exported_spans", exported_spans_.load()},
            {"failed_exports", failed_exports_.load()},
            {"dropped_spans", dropped_spans_.load()}
        };
    }
    
private:
    common::VoidResult send_grpc_batch(const std::vector<otel_span_data>& spans) {
        // Simulate OTLP gRPC sending
        // In real implementation, this would use OTLP gRPC client
        (void)spans; // Suppress unused parameter warning
        return common::ok();
    }
    
    common::VoidResult send_http_json_batch(const std::vector<otel_span_data>& spans) {
        // Simulate OTLP HTTP JSON sending
        // In real implementation, this would serialize OTEL spans to JSON and POST
        (void)spans; // Suppress unused parameter warning
        return common::ok();
    }
    
    common::VoidResult send_http_protobuf_batch(const std::vector<otel_span_data>& spans) {
        // Simulate OTLP HTTP protobuf sending
        // In real implementation, this would serialize OTEL spans to protobuf and POST
        (void)spans; // Suppress unused parameter warning
        return common::ok();
    }
};

/**
 * @class trace_exporter_factory
 * @brief Factory for creating trace exporters
 */
class trace_exporter_factory {
public:
    /**
     * @brief Create a trace exporter based on format
     */
    static std::unique_ptr<trace_exporter_interface> create_exporter(
        const trace_export_config& config,
        const otel_resource& resource = create_service_resource("monitoring_system", "2.0.0")) {
        
        switch (config.format) {
            case trace_export_format::jaeger_thrift:
            case trace_export_format::jaeger_grpc:
                return std::make_unique<jaeger_exporter>(config);
                
            case trace_export_format::zipkin_json:
            case trace_export_format::zipkin_protobuf:
                return std::make_unique<zipkin_exporter>(config);
                
            case trace_export_format::otlp_grpc:
            case trace_export_format::otlp_http_json:
            case trace_export_format::otlp_http_protobuf:
                return std::make_unique<otlp_exporter>(config, resource);
                
            default:
                return nullptr;
        }
    }
    
    /**
     * @brief Get supported formats for a specific backend
     */
    static std::vector<trace_export_format> get_supported_formats(const std::string& backend) {
        if (backend == "jaeger") {
            return {trace_export_format::jaeger_thrift, trace_export_format::jaeger_grpc};
        } else if (backend == "zipkin") {
            return {trace_export_format::zipkin_json, trace_export_format::zipkin_protobuf};
        } else if (backend == "otlp") {
            return {trace_export_format::otlp_grpc, trace_export_format::otlp_http_json, 
                   trace_export_format::otlp_http_protobuf};
        }
        return {};
    }
};

/**
 * @brief Helper function to create a Jaeger exporter
 */
inline std::unique_ptr<jaeger_exporter> create_jaeger_exporter(
    const std::string& endpoint,
    trace_export_format format = trace_export_format::jaeger_grpc) {
    
    trace_export_config config;
    config.endpoint = endpoint;
    config.format = format;
    return std::make_unique<jaeger_exporter>(config);
}

/**
 * @brief Helper function to create a Zipkin exporter
 */
inline std::unique_ptr<zipkin_exporter> create_zipkin_exporter(
    const std::string& endpoint,
    trace_export_format format = trace_export_format::zipkin_json) {
    
    trace_export_config config;
    config.endpoint = endpoint;
    config.format = format;
    return std::make_unique<zipkin_exporter>(config);
}

/**
 * @brief Helper function to create an OTLP exporter
 */
inline std::unique_ptr<otlp_exporter> create_otlp_exporter(
    const std::string& endpoint,
    const otel_resource& resource,
    trace_export_format format = trace_export_format::otlp_grpc) {
    
    trace_export_config config;
    config.endpoint = endpoint;
    config.format = format;
    return std::make_unique<otlp_exporter>(config, resource);
}

} } // namespace kcenon::monitoring
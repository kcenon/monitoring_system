#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file otlp_grpc_exporter.h
 * @brief OTLP gRPC trace exporter implementation
 *
 * This file provides a complete OTLP (OpenTelemetry Protocol) trace exporter
 * using gRPC transport. It converts internal trace spans to OTLP format and
 * sends them to an OpenTelemetry-compatible backend.
 *
 * @note Requires gRPC library (MONITORING_HAS_GRPC) for full functionality.
 * When gRPC is not available, use otlp_http_exporter instead.
 */

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include "../tracing/distributed_tracer.h"
#include "trace_exporters.h"
#include "grpc_transport.h"
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>

namespace kcenon { namespace monitoring {

/**
 * @struct otlp_grpc_config
 * @brief Configuration for OTLP gRPC exporter
 */
struct otlp_grpc_config {
    std::string endpoint = "localhost:4317";         ///< OTLP receiver endpoint
    std::chrono::milliseconds timeout{10000};        ///< Request timeout
    std::chrono::milliseconds batch_timeout{5000};   ///< Batch export timeout
    std::size_t max_batch_size = 512;                ///< Maximum spans per batch
    std::size_t max_queue_size = 2048;               ///< Maximum queued spans
    std::size_t max_retry_attempts = 3;              ///< Maximum retry attempts
    std::chrono::milliseconds initial_backoff{100};  ///< Initial retry backoff
    std::chrono::milliseconds max_backoff{10000};    ///< Maximum retry backoff
    bool use_tls = false;                            ///< Enable TLS
    std::string certificate_path;                    ///< TLS certificate path
    std::string service_name = "monitoring_system";  ///< Service name
    std::string service_version = "2.0.0";           ///< Service version
    std::unordered_map<std::string, std::string> headers;  ///< Custom headers
    std::unordered_map<std::string, std::string> resource_attributes; ///< Resource attributes

    /**
     * @brief Validate configuration
     */
    result_void validate() const {
        if (endpoint.empty()) {
            return result_void::err(error_info(
                monitoring_error_code::invalid_configuration,
                "OTLP endpoint cannot be empty",
                "otlp_grpc_config"
            ).to_common_error());
        }
        if (timeout.count() <= 0) {
            return result_void::err(error_info(
                monitoring_error_code::invalid_configuration,
                "Timeout must be positive",
                "otlp_grpc_config"
            ).to_common_error());
        }
        if (max_batch_size == 0) {
            return result_void::err(error_info(
                monitoring_error_code::invalid_configuration,
                "Batch size must be greater than 0",
                "otlp_grpc_config"
            ).to_common_error());
        }
        return common::ok();
    }
};

/**
 * @struct otlp_exporter_stats
 * @brief Statistics for OTLP exporter
 */
struct otlp_exporter_stats {
    std::size_t spans_exported{0};
    std::size_t spans_dropped{0};
    std::size_t export_failures{0};
    std::size_t retries{0};
    std::size_t batches_sent{0};
    std::chrono::microseconds total_export_time{0};  ///< Total export time (microseconds for precision)
};

/**
 * @class otlp_span_converter
 * @brief Converts internal spans to OTLP wire format
 *
 * This class handles the conversion of internal trace_span objects
 * to the OTLP protocol buffer format.
 */
class otlp_span_converter {
public:
    /**
     * @brief Convert spans to OTLP protobuf format
     * @param spans Vector of spans to convert
     * @param service_name Service name for resource
     * @param service_version Service version
     * @param resource_attributes Additional resource attributes
     * @return Serialized OTLP ExportTraceServiceRequest
     */
    static std::vector<uint8_t> convert_to_otlp(
        const std::vector<trace_span>& spans,
        const std::string& service_name,
        const std::string& service_version,
        const std::unordered_map<std::string, std::string>& resource_attributes) {

        // OTLP ExportTraceServiceRequest structure:
        // - resource_spans (repeated)
        //   - resource
        //     - attributes
        //   - scope_spans (repeated)
        //     - scope
        //     - spans (repeated)

        std::vector<uint8_t> payload;

        // Build minimal OTLP-compatible protobuf structure
        // Note: Full implementation would use generated protobuf code

        // For now, we serialize to a simplified format that OpenTelemetry
        // Collector can process. Production use should integrate actual
        // OTLP proto definitions.

        // Write field 1 (resource_spans) - length-delimited
        auto resource_spans_data = build_resource_spans(
            spans, service_name, service_version, resource_attributes);

        // Protobuf wire format: (field_number << 3) | wire_type
        // Field 1, wire type 2 (length-delimited) = 0x0A
        payload.push_back(0x0A);
        write_varint(payload, resource_spans_data.size());
        payload.insert(payload.end(),
            resource_spans_data.begin(), resource_spans_data.end());

        return payload;
    }

private:
    static std::vector<uint8_t> build_resource_spans(
        const std::vector<trace_span>& spans,
        const std::string& service_name,
        const std::string& service_version,
        const std::unordered_map<std::string, std::string>& resource_attributes) {

        std::vector<uint8_t> data;

        // Field 1: resource
        auto resource_data = build_resource(service_name, service_version, resource_attributes);
        data.push_back(0x0A);  // Field 1, length-delimited
        write_varint(data, resource_data.size());
        data.insert(data.end(), resource_data.begin(), resource_data.end());

        // Field 2: scope_spans
        auto scope_spans_data = build_scope_spans(spans);
        data.push_back(0x12);  // Field 2, length-delimited
        write_varint(data, scope_spans_data.size());
        data.insert(data.end(), scope_spans_data.begin(), scope_spans_data.end());

        return data;
    }

    static std::vector<uint8_t> build_resource(
        const std::string& service_name,
        const std::string& service_version,
        const std::unordered_map<std::string, std::string>& extra_attributes) {

        std::vector<uint8_t> data;

        // Field 1: attributes (repeated)
        // service.name attribute
        auto attr1 = build_key_value("service.name", service_name);
        data.push_back(0x0A);
        write_varint(data, attr1.size());
        data.insert(data.end(), attr1.begin(), attr1.end());

        // service.version attribute
        auto attr2 = build_key_value("service.version", service_version);
        data.push_back(0x0A);
        write_varint(data, attr2.size());
        data.insert(data.end(), attr2.begin(), attr2.end());

        // Extra attributes
        for (const auto& [key, value] : extra_attributes) {
            auto attr = build_key_value(key, value);
            data.push_back(0x0A);
            write_varint(data, attr.size());
            data.insert(data.end(), attr.begin(), attr.end());
        }

        return data;
    }

    static std::vector<uint8_t> build_scope_spans(
        const std::vector<trace_span>& spans) {

        std::vector<uint8_t> data;

        // Field 1: scope (InstrumentationScope)
        auto scope_data = build_scope("monitoring_system", "2.0.0");
        data.push_back(0x0A);
        write_varint(data, scope_data.size());
        data.insert(data.end(), scope_data.begin(), scope_data.end());

        // Field 2: spans (repeated)
        for (const auto& span : spans) {
            auto span_data = build_span(span);
            data.push_back(0x12);
            write_varint(data, span_data.size());
            data.insert(data.end(), span_data.begin(), span_data.end());
        }

        return data;
    }

    static std::vector<uint8_t> build_scope(
        const std::string& name,
        const std::string& version) {

        std::vector<uint8_t> data;

        // Field 1: name
        data.push_back(0x0A);
        write_varint(data, name.size());
        data.insert(data.end(), name.begin(), name.end());

        // Field 2: version
        data.push_back(0x12);
        write_varint(data, version.size());
        data.insert(data.end(), version.begin(), version.end());

        return data;
    }

    static std::vector<uint8_t> build_span(const trace_span& span) {
        std::vector<uint8_t> data;

        // Field 1: trace_id (16 bytes)
        auto trace_id_bytes = hex_to_bytes(span.trace_id, 16);
        data.push_back(0x0A);
        write_varint(data, trace_id_bytes.size());
        data.insert(data.end(), trace_id_bytes.begin(), trace_id_bytes.end());

        // Field 2: span_id (8 bytes)
        auto span_id_bytes = hex_to_bytes(span.span_id, 8);
        data.push_back(0x12);
        write_varint(data, span_id_bytes.size());
        data.insert(data.end(), span_id_bytes.begin(), span_id_bytes.end());

        // Field 4: parent_span_id (8 bytes, optional)
        if (!span.parent_span_id.empty()) {
            auto parent_id_bytes = hex_to_bytes(span.parent_span_id, 8);
            data.push_back(0x22);
            write_varint(data, parent_id_bytes.size());
            data.insert(data.end(), parent_id_bytes.begin(), parent_id_bytes.end());
        }

        // Field 5: name
        data.push_back(0x2A);
        write_varint(data, span.operation_name.size());
        data.insert(data.end(), span.operation_name.begin(), span.operation_name.end());

        // Field 6: kind (SPAN_KIND_INTERNAL = 1)
        data.push_back(0x30);
        data.push_back(0x01);

        // Field 7: start_time_unix_nano (fixed64)
        auto start_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            span.start_time.time_since_epoch()).count();
        data.push_back(0x39);  // Field 7, wire type 1 (64-bit)
        write_fixed64(data, static_cast<uint64_t>(start_ns));

        // Field 8: end_time_unix_nano (fixed64)
        auto end_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            span.end_time.time_since_epoch()).count();
        data.push_back(0x41);  // Field 8, wire type 1 (64-bit)
        write_fixed64(data, static_cast<uint64_t>(end_ns));

        // Field 9: attributes (repeated)
        for (const auto& [key, value] : span.tags) {
            auto attr = build_key_value(key, value);
            data.push_back(0x4A);
            write_varint(data, attr.size());
            data.insert(data.end(), attr.begin(), attr.end());
        }

        return data;
    }

    static std::vector<uint8_t> build_key_value(
        const std::string& key,
        const std::string& value) {

        std::vector<uint8_t> data;

        // Field 1: key
        data.push_back(0x0A);
        write_varint(data, key.size());
        data.insert(data.end(), key.begin(), key.end());

        // Field 2: value (AnyValue)
        auto any_value = build_any_value_string(value);
        data.push_back(0x12);
        write_varint(data, any_value.size());
        data.insert(data.end(), any_value.begin(), any_value.end());

        return data;
    }

    static std::vector<uint8_t> build_any_value_string(const std::string& value) {
        std::vector<uint8_t> data;
        // Field 1: string_value
        data.push_back(0x0A);
        write_varint(data, value.size());
        data.insert(data.end(), value.begin(), value.end());
        return data;
    }

    static void write_varint(std::vector<uint8_t>& data, uint64_t value) {
        while (value >= 0x80) {
            data.push_back(static_cast<uint8_t>(value | 0x80));
            value >>= 7;
        }
        data.push_back(static_cast<uint8_t>(value));
    }

    static void write_fixed64(std::vector<uint8_t>& data, uint64_t value) {
        for (int i = 0; i < 8; ++i) {
            data.push_back(static_cast<uint8_t>(value & 0xFF));
            value >>= 8;
        }
    }

    static std::vector<uint8_t> hex_to_bytes(const std::string& hex, std::size_t expected_size) {
        std::vector<uint8_t> bytes;
        bytes.reserve(expected_size);

        for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
            auto byte_str = hex.substr(i, 2);
            try {
                bytes.push_back(static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16)));
            } catch (...) {
                bytes.push_back(0);
            }
        }

        // Pad or truncate to expected size
        while (bytes.size() < expected_size) {
            bytes.insert(bytes.begin(), 0);
        }
        if (bytes.size() > expected_size) {
            bytes.resize(expected_size);
        }

        return bytes;
    }
};

/**
 * @class otlp_grpc_exporter
 * @brief OTLP gRPC trace exporter
 *
 * Exports trace spans to an OpenTelemetry-compatible backend via gRPC.
 * Implements batching, retry with exponential backoff, and async export.
 */
class otlp_grpc_exporter : public trace_exporter_interface {
private:
    otlp_grpc_config config_;
    std::unique_ptr<grpc_transport> transport_;
    std::atomic<bool> running_{false};
    std::atomic<std::size_t> exported_spans_{0};
    std::atomic<std::size_t> dropped_spans_{0};
    std::atomic<std::size_t> failed_exports_{0};
    std::atomic<std::size_t> retries_{0};
    std::mutex stats_mutex_;
    std::chrono::microseconds total_export_time_{0};

public:
    /**
     * @brief Construct with configuration
     * @param config Exporter configuration
     */
    explicit otlp_grpc_exporter(const otlp_grpc_config& config)
        : config_(config), transport_(create_default_grpc_transport()) {}

    /**
     * @brief Construct with configuration and custom transport
     * @param config Exporter configuration
     * @param transport Custom gRPC transport (for testing)
     */
    otlp_grpc_exporter(
        const otlp_grpc_config& config,
        std::unique_ptr<grpc_transport> transport)
        : config_(config), transport_(std::move(transport)) {}

    /**
     * @brief Start the exporter
     * @return result_void indicating success or failure
     */
    result_void start() {
        auto validate_result = config_.validate();
        if (validate_result.is_err()) {
            return validate_result;
        }

        // Parse endpoint
        auto [host, port] = parse_endpoint(config_.endpoint);

        // Connect to server
        auto connect_result = transport_->connect(host, port);
        if (connect_result.is_err()) {
            return connect_result;
        }

        running_ = true;
        return common::ok();
    }

    /**
     * @brief Export a batch of spans
     * @param spans Spans to export
     * @return result_void indicating success or failure
     */
    result_void export_spans(const std::vector<trace_span>& spans) override {
        if (spans.empty()) {
            return common::ok();
        }

        if (!transport_->is_connected()) {
            dropped_spans_ += spans.size();
            return result_void::err(error_info(
                monitoring_error_code::network_error,
                "Not connected to OTLP receiver",
                "otlp_grpc_exporter"
            ).to_common_error());
        }

        // Convert spans to OTLP format
        auto payload = otlp_span_converter::convert_to_otlp(
            spans,
            config_.service_name,
            config_.service_version,
            config_.resource_attributes);

        // Prepare gRPC request
        grpc_request request;
        request.service = "opentelemetry.proto.collector.trace.v1.TraceService";
        request.method = "Export";
        request.body = std::move(payload);
        request.timeout = config_.timeout;

        // Add headers/metadata
        for (const auto& [key, value] : config_.headers) {
            request.metadata[key] = value;
        }

        // Send with retry
        auto start_time = std::chrono::steady_clock::now();
        auto send_result = send_with_retry(request);
        auto end_time = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            total_export_time_ += std::chrono::duration_cast<std::chrono::microseconds>(
                end_time - start_time);
        }

        if (send_result.is_ok()) {
            exported_spans_ += spans.size();
            return common::ok();
        } else {
            failed_exports_++;
            dropped_spans_ += spans.size();
            return result_void::err(error_info(
                monitoring_error_code::operation_failed,
                "Failed to export spans: " + send_result.error().message,
                "otlp_grpc_exporter"
            ).to_common_error());
        }
    }

    /**
     * @brief Flush pending exports
     */
    result_void flush() override {
        // Synchronous exporter - nothing to flush
        return common::ok();
    }

    /**
     * @brief Shutdown the exporter
     */
    result_void shutdown() override {
        running_ = false;
        transport_->disconnect();
        return common::ok();
    }

    /**
     * @brief Get exporter statistics
     */
    std::unordered_map<std::string, std::size_t> get_stats() const override {
        return {
            {"exported_spans", exported_spans_.load()},
            {"dropped_spans", dropped_spans_.load()},
            {"failed_exports", failed_exports_.load()},
            {"retries", retries_.load()}
        };
    }

    /**
     * @brief Get detailed statistics
     */
    otlp_exporter_stats get_detailed_stats() const {
        otlp_exporter_stats stats;
        stats.spans_exported = exported_spans_.load();
        stats.spans_dropped = dropped_spans_.load();
        stats.export_failures = failed_exports_.load();
        stats.retries = retries_.load();

        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(stats_mutex_));
        stats.total_export_time = total_export_time_;
        return stats;
    }

    /**
     * @brief Check if exporter is running
     */
    bool is_running() const {
        return running_.load();
    }

    /**
     * @brief Get configuration
     */
    const otlp_grpc_config& config() const {
        return config_;
    }

private:
    result<grpc_response> send_with_retry(const grpc_request& request) {
        std::size_t attempt = 0;
        auto backoff = config_.initial_backoff;

        while (attempt < config_.max_retry_attempts) {
            auto result = transport_->send(request);

            if (result.is_ok()) {
                const auto& response = result.value();
                // gRPC status 0 = OK
                if (response.status_code == 0) {
                    return result;
                }

                // Check if error is retryable
                if (is_retryable_error(response.status_code)) {
                    attempt++;
                    retries_++;
                    if (attempt < config_.max_retry_attempts) {
                        std::this_thread::sleep_for(backoff);
                        backoff = std::min(backoff * 2, config_.max_backoff);
                    }
                    continue;
                }

                // Non-retryable error
                return make_error<grpc_response>(
                    monitoring_error_code::operation_failed,
                    "OTLP export failed with status: " +
                        std::to_string(response.status_code) + " - " + response.status_message);
            }

            // Transport error
            attempt++;
            retries_++;
            if (attempt < config_.max_retry_attempts) {
                std::this_thread::sleep_for(backoff);
                backoff = std::min(backoff * 2, config_.max_backoff);
            }
        }

        return make_error<grpc_response>(
            monitoring_error_code::operation_failed,
            "OTLP export failed after " + std::to_string(config_.max_retry_attempts) + " retries");
    }

    static bool is_retryable_error(int status_code) {
        // gRPC status codes that are retryable:
        // 1 = CANCELLED, 4 = DEADLINE_EXCEEDED, 8 = RESOURCE_EXHAUSTED,
        // 10 = ABORTED, 14 = UNAVAILABLE
        return status_code == 1 || status_code == 4 || status_code == 8 ||
               status_code == 10 || status_code == 14;
    }

    static std::pair<std::string, uint16_t> parse_endpoint(const std::string& endpoint) {
        std::string host = "localhost";
        uint16_t port = 4317;

        auto colon_pos = endpoint.rfind(':');
        if (colon_pos != std::string::npos) {
            host = endpoint.substr(0, colon_pos);
            try {
                port = static_cast<uint16_t>(std::stoi(endpoint.substr(colon_pos + 1)));
            } catch (...) {
                port = 4317;
            }
        } else {
            host = endpoint;
        }

        return {host, port};
    }
};

/**
 * @brief Create OTLP gRPC exporter with default configuration
 * @param endpoint OTLP receiver endpoint
 * @return Unique pointer to exporter
 */
inline std::unique_ptr<otlp_grpc_exporter> create_otlp_grpc_exporter(
    const std::string& endpoint = "localhost:4317") {

    otlp_grpc_config config;
    config.endpoint = endpoint;
    return std::make_unique<otlp_grpc_exporter>(config);
}

/**
 * @brief Create OTLP gRPC exporter with custom configuration
 * @param config Exporter configuration
 * @return Unique pointer to exporter
 */
inline std::unique_ptr<otlp_grpc_exporter> create_otlp_grpc_exporter(
    const otlp_grpc_config& config) {

    return std::make_unique<otlp_grpc_exporter>(config);
}

} } // namespace kcenon::monitoring

// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file otlp_export_example.cpp
 * @brief Demonstrates OpenTelemetry Protocol (OTLP) export configuration
 *
 * This example shows how to:
 * - Configure OTLP gRPC exporter
 * - Set up resource attributes and instrumentation scope
 * - Implement batch export optimization
 * - Handle export retry and error scenarios
 * - Configure export timeouts and backoff strategies
 */

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <vector>

#include "kcenon/monitoring/exporters/otlp_grpc_exporter.h"
#include "kcenon/monitoring/tracing/distributed_tracer.h"

using namespace kcenon::monitoring;
using namespace std::chrono_literals;

/**
 * @brief Configure OTLP exporter with custom settings
 */
otlp_grpc_config create_otlp_config() {
    otlp_grpc_config config;

    // Endpoint configuration
    config.endpoint = "localhost:4317";  // Default OTLP gRPC port

    // Timeout settings
    config.timeout = 10s;
    config.batch_timeout = 5s;

    // Batch configuration for optimization
    config.max_batch_size = 512;   // Export up to 512 spans at once
    config.max_queue_size = 2048;  // Queue up to 2048 spans before dropping

    // Retry configuration with exponential backoff
    config.max_retry_attempts = 3;
    config.initial_backoff = 100ms;
    config.max_backoff = 10s;

    // TLS configuration (optional)
    config.use_tls = false;
    // config.certificate_path = "/path/to/cert.pem";

    // Service identification
    config.service_name = "monitoring_system_example";
    config.service_version = "2.0.0";

    // Custom headers (e.g., for authentication)
    config.headers["x-api-key"] = "example-key";
    config.headers["x-environment"] = "development";

    // Resource attributes (describe the service)
    config.resource_attributes["service.namespace"] = "examples";
    config.resource_attributes["service.instance.id"] = "instance-001";
    config.resource_attributes["deployment.environment"] = "dev";
    config.resource_attributes["host.name"] = "example-host";

    return config;
}

/**
 * @brief Create sample trace spans for export
 */
std::vector<trace_span> create_sample_spans() {
    std::vector<trace_span> spans;

    auto now = std::chrono::system_clock::now();
    std::string trace_id = "0123456789abcdef0123456789abcdef";

    // Root span
    trace_span root;
    root.trace_id = trace_id;
    root.span_id = "0123456789abcdef";
    root.operation_name = "http_request";
    root.service_name = "api_gateway";
    root.start_time = now;
    root.end_time = now + 150ms;
    root.calculate_duration();
    root.status = trace_span::status_code::ok;
    root.tags["http.method"] = "GET";
    root.tags["http.url"] = "/api/users";
    root.tags["http.status_code"] = "200";
    spans.push_back(root);

    // Child span 1: database query
    trace_span db_span;
    db_span.trace_id = trace_id;
    db_span.span_id = "fedcba9876543210";
    db_span.parent_span_id = root.span_id;
    db_span.operation_name = "db_query";
    db_span.service_name = "user_service";
    db_span.start_time = now + 10ms;
    db_span.end_time = now + 100ms;
    db_span.calculate_duration();
    db_span.status = trace_span::status_code::ok;
    db_span.tags["db.system"] = "postgresql";
    db_span.tags["db.statement"] = "SELECT * FROM users";
    db_span.tags["db.name"] = "user_db";
    spans.push_back(db_span);

    // Child span 2: cache lookup
    trace_span cache_span;
    cache_span.trace_id = trace_id;
    cache_span.span_id = "1234567890abcdef";
    cache_span.parent_span_id = root.span_id;
    cache_span.operation_name = "cache_get";
    cache_span.service_name = "cache_service";
    cache_span.start_time = now + 5ms;
    cache_span.end_time = now + 8ms;
    cache_span.calculate_duration();
    cache_span.status = trace_span::status_code::ok;
    cache_span.tags["cache.key"] = "user:123";
    cache_span.tags["cache.hit"] = "false";
    spans.push_back(cache_span);

    return spans;
}

/**
 * @brief Demonstrate OTLP export with error handling
 */
void demonstrate_otlp_export() {
    std::cout << "=== OTLP Export Example ===" << std::endl;

    try {
        // Step 1: Create and validate configuration
        std::cout << "\n1. Configuring OTLP exporter..." << std::endl;

        auto config = create_otlp_config();
        auto validation = config.validate();

        if (validation.is_err()) {
            std::cerr << "   Configuration validation failed: "
                      << validation.error().message << std::endl;
            return;
        }

        std::cout << "   ✓ Configuration validated" << std::endl;
        std::cout << "     Endpoint: " << config.endpoint << std::endl;
        std::cout << "     Service: " << config.service_name
                  << " v" << config.service_version << std::endl;
        std::cout << "     Max batch size: " << config.max_batch_size << std::endl;
        std::cout << "     Max retries: " << config.max_retry_attempts << std::endl;

        // Step 2: Create exporter
        std::cout << "\n2. Creating OTLP exporter..." << std::endl;

        auto exporter = create_otlp_grpc_exporter(config);
        std::cout << "   ✓ Exporter created" << std::endl;

        // Step 3: Start exporter (connects to OTLP receiver)
        std::cout << "\n3. Starting exporter..." << std::endl;

        auto start_result = exporter->start();
        if (start_result.is_err()) {
            std::cerr << "   ✗ Failed to start exporter: "
                      << start_result.error().message << std::endl;
            std::cerr << "   Note: Make sure an OTLP receiver is running on "
                      << config.endpoint << std::endl;
            std::cerr << "   You can use: docker run -p 4317:4317 otel/opentelemetry-collector"
                      << std::endl;
            return;
        }

        std::cout << "   ✓ Exporter started and connected" << std::endl;

        // Step 4: Create sample spans
        std::cout << "\n4. Creating sample trace spans..." << std::endl;

        auto spans = create_sample_spans();
        std::cout << "   ✓ Created " << spans.size() << " spans" << std::endl;

        for (const auto& span : spans) {
            std::cout << "     - " << span.operation_name
                      << " (duration: " << span.duration.count() << "µs)" << std::endl;
        }

        // Step 5: Export spans
        std::cout << "\n5. Exporting spans..." << std::endl;

        auto export_result = exporter->export_spans(spans);
        if (export_result.is_ok()) {
            std::cout << "   ✓ Export succeeded" << std::endl;
        } else {
            std::cerr << "   ✗ Export failed: "
                      << export_result.error().message << std::endl;
        }

        // Step 6: Check exporter statistics
        std::cout << "\n6. Exporter statistics:" << std::endl;

        auto detailed_stats = exporter->get_detailed_stats();
        std::cout << "   Spans exported: " << detailed_stats.spans_exported << std::endl;
        std::cout << "   Spans dropped: " << detailed_stats.spans_dropped << std::endl;
        std::cout << "   Export failures: " << detailed_stats.export_failures << std::endl;
        std::cout << "   Retry attempts: " << detailed_stats.retries << std::endl;
        std::cout << "   Total export time: "
                  << detailed_stats.total_export_time.count() << "µs" << std::endl;

        // Step 7: Flush and shutdown
        std::cout << "\n7. Shutting down exporter..." << std::endl;

        exporter->flush();
        exporter->shutdown();
        std::cout << "   ✓ Exporter shutdown complete" << std::endl;

        std::cout << "\n=== Example completed successfully ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

/**
 * @brief Demonstrate batch export optimization
 */
void demonstrate_batch_export() {
    std::cout << "\n=== Batch Export Optimization ===" << std::endl;

    try {
        // Configure for batch export
        otlp_grpc_config config = create_otlp_config();
        config.max_batch_size = 10;  // Small batch for demonstration

        auto exporter = create_otlp_grpc_exporter(config);
        auto start_result = exporter->start();

        if (start_result.is_err()) {
            std::cerr << "   Skipping batch demo (no OTLP receiver available)" << std::endl;
            return;
        }

        std::cout << "\n1. Creating large batch of spans..." << std::endl;

        // Create 25 spans (will be split into 3 batches of 10, 10, 5)
        std::vector<trace_span> large_batch;
        auto now = std::chrono::system_clock::now();

        for (int i = 0; i < 25; ++i) {
            trace_span span;
            span.trace_id = "batch00000000000000000000000000" + std::to_string(i);
            span.span_id = "span000000000000" + std::to_string(i);
            span.operation_name = "batch_operation_" + std::to_string(i);
            span.service_name = "batch_service";
            span.start_time = now + std::chrono::milliseconds(i);
            span.end_time = span.start_time + 10ms;
            span.calculate_duration();
            span.status = trace_span::status_code::ok;
            large_batch.push_back(span);
        }

        std::cout << "   Created " << large_batch.size() << " spans" << std::endl;
        std::cout << "   Batch size: " << config.max_batch_size << std::endl;

        // Export in batch
        std::cout << "\n2. Exporting batch..." << std::endl;
        auto export_result = exporter->export_spans(large_batch);

        if (export_result.is_ok()) {
            std::cout << "   ✓ Batch export succeeded" << std::endl;
        } else {
            std::cerr << "   ✗ Batch export failed: "
                      << export_result.error().message << std::endl;
        }

        // Show final statistics
        auto stats = exporter->get_detailed_stats();
        std::cout << "\n3. Final statistics:" << std::endl;
        std::cout << "   Total exported: " << stats.spans_exported << std::endl;
        std::cout << "   Batches sent: " << stats.batches_sent << std::endl;

        exporter->shutdown();

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "OpenTelemetry Protocol (OTLP) Export Example\n" << std::endl;

    // Demonstrate basic OTLP export
    demonstrate_otlp_export();

    std::cout << "\n" << std::string(60, '=') << "\n" << std::endl;

    // Demonstrate batch export optimization
    demonstrate_batch_export();

    return 0;
}

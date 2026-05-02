// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.


#include <gtest/gtest.h>
#include <kcenon/monitoring/exporters/trace_exporters.h>
// Note: distributed_tracer.h does not exist in include directory
// #include <kcenon/monitoring/tracing/distributed_tracer.h>
#include <kcenon/monitoring/exporters/opentelemetry_adapter.h>
#include <thread>
#include <chrono>

using namespace kcenon::monitoring;

class TraceExportersTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test spans
        test_spans_ = create_test_spans();
        
        // Create OTEL resource
        otel_resource_ = create_service_resource("test_service", "1.0.0", "test_namespace");
    }
    
    std::vector<trace_span> create_test_spans() {
        std::vector<trace_span> spans;
        
        // Root span
        trace_span root_span;
        root_span.trace_id = "trace123";
        root_span.span_id = "span001";
        root_span.operation_name = "http_request";
        root_span.service_name = "web_service";
        root_span.start_time = std::chrono::system_clock::now();
        root_span.end_time = root_span.start_time + std::chrono::milliseconds(100);
        root_span.tags["http.method"] = "GET";
        root_span.tags["http.url"] = "/api/users";
        root_span.tags["span.kind"] = "server";
        spans.push_back(root_span);
        
        // Child span
        trace_span child_span;
        child_span.trace_id = "trace123";
        child_span.span_id = "span002";
        child_span.parent_span_id = "span001";
        child_span.operation_name = "database_query";
        child_span.service_name = "db_service";
        child_span.start_time = root_span.start_time + std::chrono::milliseconds(10);
        child_span.end_time = root_span.start_time + std::chrono::milliseconds(80);
        child_span.tags["db.statement"] = "SELECT * FROM users WHERE id = ?";
        child_span.tags["db.type"] = "postgresql";
        child_span.tags["span.kind"] = "client";
        spans.push_back(child_span);
        
        return spans;
    }
    
    std::vector<trace_span> test_spans_;
    otel_resource otel_resource_;
};

TEST_F(TraceExportersTest, TraceExportConfigValidation) {
    // Valid configuration
    trace_export_config valid_config;
    valid_config.endpoint = "http://jaeger:14268/api/traces";
    valid_config.format = trace_export_format::jaeger_thrift;
    valid_config.timeout = std::chrono::milliseconds(5000);
    valid_config.max_batch_size = 100;
    valid_config.max_queue_size = 1000;

    auto validation = valid_config.validate();
    EXPECT_TRUE(validation.is_ok());

    // Invalid endpoint
    trace_export_config invalid_endpoint;
    invalid_endpoint.endpoint = "";
    auto endpoint_validation = invalid_endpoint.validate();
    EXPECT_TRUE(endpoint_validation.is_err());
    EXPECT_EQ(endpoint_validation.error().code, static_cast<int>(monitoring_error_code::invalid_configuration));

    // Invalid timeout
    trace_export_config invalid_timeout;
    invalid_timeout.endpoint = "http://test";
    invalid_timeout.timeout = std::chrono::milliseconds(0);
    auto timeout_validation = invalid_timeout.validate();
    EXPECT_TRUE(timeout_validation.is_err());

    // Invalid batch size
    trace_export_config invalid_batch;
    invalid_batch.endpoint = "http://test";
    invalid_batch.max_batch_size = 0;
    auto batch_validation = invalid_batch.validate();
    EXPECT_TRUE(batch_validation.is_err());

    // Invalid queue size
    trace_export_config invalid_queue;
    invalid_queue.endpoint = "http://test";
    invalid_queue.max_batch_size = 100;
    invalid_queue.max_queue_size = 50;
    auto queue_validation = invalid_queue.validate();
    EXPECT_TRUE(queue_validation.is_err());
}

TEST_F(TraceExportersTest, JaegerSpanConversion) {
    trace_export_config config;
    config.endpoint = "http://jaeger:14268/api/traces";
    config.format = trace_export_format::jaeger_thrift;
    config.service_name = "test_service";
    
    jaeger_exporter exporter(config);
    
    const auto& span = test_spans_[0];
    auto jaeger_span = exporter.convert_span(span);
    
    EXPECT_EQ(jaeger_span.trace_id, span.trace_id);
    EXPECT_EQ(jaeger_span.span_id, span.span_id);
    EXPECT_EQ(jaeger_span.operation_name, span.operation_name);
    EXPECT_EQ(jaeger_span.service_name, "test_service"); // Override from config
    
    // Check tags conversion
    bool found_http_method = false;
    bool found_http_url = false;
    for (const auto& [key, value] : jaeger_span.tags) {
        if (key == "http.method" && value == "GET") {
            found_http_method = true;
        }
        if (key == "http.url" && value == "/api/users") {
            found_http_url = true;
        }
    }
    EXPECT_TRUE(found_http_method);
    EXPECT_TRUE(found_http_url);
    
    // Check process tags
    bool found_service_name = false;
    for (const auto& [key, value] : jaeger_span.process_tags) {
        if (key == "service.name" && value == "test_service") {
            found_service_name = true;
        }
    }
    EXPECT_TRUE(found_service_name);
}

TEST_F(TraceExportersTest, JaegerExporterBasicFunctionality) {
    trace_export_config config;
    config.endpoint = "http://jaeger:14268/api/traces";
    config.format = trace_export_format::jaeger_thrift;

    jaeger_exporter exporter(config);

    // Export spans — stub transport returns error (no real HTTP client)
    auto export_result = exporter.export_spans(test_spans_);
    EXPECT_TRUE(export_result.is_err());

    // Check statistics — export failed so failed_exports should increment
    auto stats = exporter.get_stats();
    EXPECT_EQ(stats["failed_exports"], 1);

    // Test flush and shutdown (should still work)
    auto flush_result = exporter.flush();
    EXPECT_TRUE(flush_result.is_ok());

    auto shutdown_result = exporter.shutdown();
    EXPECT_TRUE(shutdown_result.is_ok());
}

TEST_F(TraceExportersTest, ZipkinSpanConversion) {
    trace_export_config config;
    config.endpoint = "http://zipkin:9411/api/v2/spans";
    config.format = trace_export_format::zipkin_json;
    config.service_name = "test_service";
    
    zipkin_exporter exporter(config);
    
    const auto& span = test_spans_[0];
    auto zipkin_span = exporter.convert_span(span);
    
    EXPECT_EQ(zipkin_span.trace_id, span.trace_id);
    EXPECT_EQ(zipkin_span.span_id, span.span_id);
    EXPECT_EQ(zipkin_span.name, span.operation_name);
    EXPECT_EQ(zipkin_span.local_endpoint_service_name, "test_service");
    EXPECT_EQ(zipkin_span.kind, "server"); // From span.kind tag
    
    // Check tags conversion (span.kind should be excluded)
    EXPECT_EQ(zipkin_span.tags.count("span.kind"), 0);
    EXPECT_EQ(zipkin_span.tags.count("http.method"), 1);
    EXPECT_EQ(zipkin_span.tags["http.method"], "GET");
}

TEST_F(TraceExportersTest, ZipkinExporterBasicFunctionality) {
    trace_export_config config;
    config.endpoint = "http://zipkin:9411/api/v2/spans";
    config.format = trace_export_format::zipkin_json;

    zipkin_exporter exporter(config);

    // Export spans — stub transport returns error (no real HTTP client)
    auto export_result = exporter.export_spans(test_spans_);
    EXPECT_TRUE(export_result.is_err());

    // Check statistics — export failed
    auto stats = exporter.get_stats();
    EXPECT_EQ(stats["failed_exports"], 1);

    // Test flush and shutdown (should still work)
    auto flush_result = exporter.flush();
    EXPECT_TRUE(flush_result.is_ok());

    auto shutdown_result = exporter.shutdown();
    EXPECT_TRUE(shutdown_result.is_ok());
}

TEST_F(TraceExportersTest, OtlpExporterBasicFunctionality) {
    trace_export_config config;
    config.endpoint = "http://otlp-collector:4317";
    config.format = trace_export_format::otlp_grpc;

    otlp_exporter exporter(config, otel_resource_);

    // Export spans
    auto export_result = exporter.export_spans(test_spans_);
    EXPECT_TRUE(export_result.is_ok());

    // Check statistics
    auto stats = exporter.get_stats();
    EXPECT_EQ(stats["exported_spans"], test_spans_.size());
    EXPECT_EQ(stats["failed_exports"], 0);

    // Test flush and shutdown
    auto flush_result = exporter.flush();
    EXPECT_TRUE(flush_result.is_ok());

    auto shutdown_result = exporter.shutdown();
    EXPECT_TRUE(shutdown_result.is_ok());
}

TEST_F(TraceExportersTest, TraceExporterFactory) {
    // Test Jaeger factory
    trace_export_config jaeger_config;
    jaeger_config.endpoint = "http://jaeger:14268";
    jaeger_config.format = trace_export_format::jaeger_grpc;
    
    auto jaeger_exporter = trace_exporter_factory::create_exporter(jaeger_config, otel_resource_);
    EXPECT_TRUE(jaeger_exporter);
    
    // Test Zipkin factory
    trace_export_config zipkin_config;
    zipkin_config.endpoint = "http://zipkin:9411";
    zipkin_config.format = trace_export_format::zipkin_json;
    
    auto zipkin_exporter = trace_exporter_factory::create_exporter(zipkin_config, otel_resource_);
    EXPECT_TRUE(zipkin_exporter);
    
    // Test OTLP factory
    trace_export_config otlp_config;
    otlp_config.endpoint = "http://otlp-collector:4317";
    otlp_config.format = trace_export_format::otlp_grpc;
    
    auto otlp_exporter = trace_exporter_factory::create_exporter(otlp_config, otel_resource_);
    EXPECT_TRUE(otlp_exporter);
}

TEST_F(TraceExportersTest, SupportedFormatsQuery) {
    auto jaeger_formats = trace_exporter_factory::get_supported_formats("jaeger");
    EXPECT_EQ(jaeger_formats.size(), 2);
    EXPECT_TRUE(std::find(jaeger_formats.begin(), jaeger_formats.end(), 
                         trace_export_format::jaeger_thrift) != jaeger_formats.end());
    EXPECT_TRUE(std::find(jaeger_formats.begin(), jaeger_formats.end(), 
                         trace_export_format::jaeger_grpc) != jaeger_formats.end());
    
    auto zipkin_formats = trace_exporter_factory::get_supported_formats("zipkin");
    EXPECT_EQ(zipkin_formats.size(), 2);
    EXPECT_TRUE(std::find(zipkin_formats.begin(), zipkin_formats.end(), 
                         trace_export_format::zipkin_json) != zipkin_formats.end());
    EXPECT_TRUE(std::find(zipkin_formats.begin(), zipkin_formats.end(), 
                         trace_export_format::zipkin_protobuf) != zipkin_formats.end());
    
    auto otlp_formats = trace_exporter_factory::get_supported_formats("otlp");
    EXPECT_EQ(otlp_formats.size(), 3);
    EXPECT_TRUE(std::find(otlp_formats.begin(), otlp_formats.end(), 
                         trace_export_format::otlp_grpc) != otlp_formats.end());
    
    auto unknown_formats = trace_exporter_factory::get_supported_formats("unknown");
    EXPECT_EQ(unknown_formats.size(), 0);
}

TEST_F(TraceExportersTest, HelperFunctions) {
    // Test Jaeger helper
    auto jaeger_exporter = create_jaeger_exporter("http://jaeger:14268", 
                                                 trace_export_format::jaeger_thrift);
    EXPECT_TRUE(jaeger_exporter);
    
    // Test Zipkin helper
    auto zipkin_exporter = create_zipkin_exporter("http://zipkin:9411", 
                                                 trace_export_format::zipkin_protobuf);
    EXPECT_TRUE(zipkin_exporter);
    
    // Test OTLP helper
    auto otlp_exporter = create_otlp_exporter("http://otlp:4317", otel_resource_, 
                                             trace_export_format::otlp_http_json);
    EXPECT_TRUE(otlp_exporter);
}

TEST_F(TraceExportersTest, InvalidFormatHandling) {
    trace_export_config invalid_jaeger_config;
    invalid_jaeger_config.endpoint = "http://jaeger:14268";
    invalid_jaeger_config.format = trace_export_format::zipkin_json; // Wrong format

    jaeger_exporter jaeger_exp(invalid_jaeger_config);
    auto jaeger_result = jaeger_exp.export_spans(test_spans_);
    EXPECT_TRUE(jaeger_result.is_err());
    EXPECT_EQ(jaeger_result.error().code, static_cast<int>(monitoring_error_code::invalid_configuration));

    trace_export_config invalid_zipkin_config;
    invalid_zipkin_config.endpoint = "http://zipkin:9411";
    invalid_zipkin_config.format = trace_export_format::jaeger_grpc; // Wrong format

    zipkin_exporter zipkin_exp(invalid_zipkin_config);
    auto zipkin_result = zipkin_exp.export_spans(test_spans_);
    EXPECT_TRUE(zipkin_result.is_err());
    EXPECT_EQ(zipkin_result.error().code, static_cast<int>(monitoring_error_code::invalid_configuration));

    trace_export_config invalid_otlp_config;
    invalid_otlp_config.endpoint = "http://otlp:4317";
    invalid_otlp_config.format = trace_export_format::jaeger_thrift; // Wrong format

    otlp_exporter otlp_exp(invalid_otlp_config, otel_resource_);
    auto otlp_result = otlp_exp.export_spans(test_spans_);
    EXPECT_TRUE(otlp_result.is_err());
    EXPECT_EQ(otlp_result.error().code, static_cast<int>(monitoring_error_code::invalid_configuration));
}

TEST_F(TraceExportersTest, EmptySpansHandling) {
    std::vector<trace_span> empty_spans;

    trace_export_config config;
    config.endpoint = "http://test:1234";
    config.format = trace_export_format::jaeger_grpc;

    jaeger_exporter exporter(config);
    auto result = exporter.export_spans(empty_spans);
    // Empty spans may succeed (nothing to send) or fail (stub transport)
    // Either way, no spans were exported
    auto stats = exporter.get_stats();
    EXPECT_EQ(stats["exported_spans"], 0);
}

TEST_F(TraceExportersTest, LargeSpanBatch) {
    // Create a large batch of spans
    std::vector<trace_span> large_batch;
    for (int i = 0; i < 1000; ++i) {
        trace_span span;
        span.trace_id = "trace" + std::to_string(i);
        span.span_id = "span" + std::to_string(i);
        span.operation_name = "operation_" + std::to_string(i);
        span.service_name = "test_service";
        span.start_time = std::chrono::system_clock::now();
        span.end_time = span.start_time + std::chrono::milliseconds(1);
        large_batch.push_back(span);
    }

    trace_export_config config;
    config.endpoint = "http://test:1234";
    config.format = trace_export_format::otlp_grpc;
    config.max_batch_size = 500; // Smaller than batch size

    otlp_exporter exporter(config, otel_resource_);
    auto result = exporter.export_spans(large_batch);
    EXPECT_TRUE(result.is_ok());

    auto stats = exporter.get_stats();
    EXPECT_EQ(stats["exported_spans"], static_cast<std::size_t>(1000));
}

// ==============================================================
// Jaeger / Zipkin Protobuf Round-Trip Tests (Issue #670)
// ==============================================================

#include <kcenon/monitoring/exporters/internal/jaeger_proto.h>
#include <kcenon/monitoring/exporters/internal/zipkin_proto.h>

TEST(ProtobufWireTest, VarintRoundTrip) {
    using namespace kcenon::monitoring::protobuf_wire;
    const std::uint64_t values[] = {
        0, 1, 127, 128, 255, 16383, 16384,
        1000000000ULL, (std::uint64_t{1} << 63)
    };
    for (std::uint64_t v : values) {
        std::vector<std::uint8_t> buf;
        encode_varint(buf, v);
        reader r(buf.data(), buf.size());
        auto got = r.read_varint();
        ASSERT_TRUE(got.has_value()) << "failed for value " << v;
        EXPECT_EQ(*got, v);
        EXPECT_TRUE(r.eof());
    }
}

TEST(ProtobufWireTest, HexBytesRoundTrip) {
    using namespace kcenon::monitoring::protobuf_wire;
    const std::string hex = "0123456789abcdef";
    auto bytes = hex_to_bytes(hex);
    EXPECT_EQ(bytes.size(), 8u);
    EXPECT_EQ(bytes_to_hex(bytes), hex);

    // Invalid hex yields empty
    EXPECT_TRUE(hex_to_bytes("xyz").empty());

    // Left-padding to 16 bytes
    auto padded = left_pad(bytes, 16);
    EXPECT_EQ(padded.size(), 16u);
    for (std::size_t i = 0; i < 8; ++i) EXPECT_EQ(padded[i], 0u);
    for (std::size_t i = 0; i < 8; ++i) EXPECT_EQ(padded[8 + i], bytes[i]);
}

TEST_F(TraceExportersTest, JaegerSpanProtobufRoundTrip) {
    trace_export_config config;
    config.endpoint = "http://jaeger:14268";
    config.format = trace_export_format::jaeger_grpc;
    config.service_name = "test_service";

    jaeger_exporter exporter(config);
    auto jaeger_span = exporter.convert_span(test_spans_[0]);
    auto wire = jaeger_span.to_protobuf();
    ASSERT_FALSE(wire.empty());

    kcenon::monitoring::jaeger_proto::span decoded;
    ASSERT_TRUE(kcenon::monitoring::jaeger_proto::decode_span(
        wire.data(), wire.size(), decoded));

    // Core identifiers: 16-byte trace ID, 8-byte span ID
    EXPECT_EQ(decoded.trace_id.size(), 16u);
    EXPECT_EQ(decoded.span_id.size(), 8u);
    EXPECT_EQ(decoded.operation_name, test_spans_[0].operation_name);

    // Tags preserved (STRING ValueType).
    bool found_method = false, found_url = false, found_kind = false;
    for (const auto& kv : decoded.tags) {
        if (kv.key == "http.method") {
            EXPECT_EQ(kv.v_str, "GET");
            EXPECT_EQ(static_cast<int>(kv.v_type), 0);
            found_method = true;
        } else if (kv.key == "http.url") {
            EXPECT_EQ(kv.v_str, "/api/users");
            found_url = true;
        } else if (kv.key == "span.kind") {
            EXPECT_EQ(kv.v_str, "server");
            found_kind = true;
        }
    }
    EXPECT_TRUE(found_method);
    EXPECT_TRUE(found_url);
    EXPECT_TRUE(found_kind);

    // Process service name overridden from config.
    EXPECT_EQ(decoded.proc.service_name, "test_service");
}

TEST_F(TraceExportersTest, JaegerSpanProtobufParentReference) {
    trace_export_config config;
    config.endpoint = "http://jaeger:14268";
    config.format = trace_export_format::jaeger_grpc;

    jaeger_exporter exporter(config);
    // Second test span has parent_span_id set.
    auto jaeger_span = exporter.convert_span(test_spans_[1]);
    auto wire = jaeger_span.to_protobuf();

    kcenon::monitoring::jaeger_proto::span decoded;
    ASSERT_TRUE(kcenon::monitoring::jaeger_proto::decode_span(
        wire.data(), wire.size(), decoded));

    ASSERT_EQ(decoded.references.size(), 1u);
    EXPECT_EQ(decoded.references[0].ref_type, 0);  // CHILD_OF
    EXPECT_EQ(decoded.references[0].span_id.size(), 8u);
}

TEST_F(TraceExportersTest, JaegerBatchProtobufRoundTrip) {
    trace_export_config config;
    config.endpoint = "http://jaeger:14268";
    config.format = trace_export_format::jaeger_grpc;
    config.service_name = "batch_service";

    jaeger_exporter exporter(config);
    std::vector<jaeger_span_data> jaeger_spans;
    for (const auto& s : test_spans_) {
        jaeger_spans.push_back(exporter.convert_span(s));
    }
    auto wire = encode_jaeger_batch(jaeger_spans, "batch_service");
    ASSERT_FALSE(wire.empty());

    kcenon::monitoring::jaeger_proto::batch decoded;
    ASSERT_TRUE(kcenon::monitoring::jaeger_proto::decode_batch(
        wire.data(), wire.size(), decoded));
    EXPECT_EQ(decoded.spans.size(), test_spans_.size());
    EXPECT_TRUE(decoded.has_process);
    EXPECT_EQ(decoded.proc.service_name, "batch_service");
}

TEST_F(TraceExportersTest, ZipkinSpanProtobufRoundTrip) {
    trace_export_config config;
    config.endpoint = "http://zipkin:9411";
    config.format = trace_export_format::zipkin_protobuf;
    config.service_name = "test_service";

    zipkin_exporter exporter(config);
    auto zipkin_span = exporter.convert_span(test_spans_[0]);
    auto wire = zipkin_span.to_protobuf();
    ASSERT_FALSE(wire.empty());

    kcenon::monitoring::zipkin_proto::span decoded;
    ASSERT_TRUE(kcenon::monitoring::zipkin_proto::decode_span(
        wire.data(), wire.size(), decoded));

    // Trace ID length: 8 or 16; span ID: 8 bytes.
    EXPECT_TRUE(decoded.trace_id.size() == 8u || decoded.trace_id.size() == 16u);
    EXPECT_EQ(decoded.id.size(), 8u);
    EXPECT_EQ(decoded.name, "http_request");
    EXPECT_EQ(decoded.kind, kcenon::monitoring::zipkin_proto::span_kind::server);
    EXPECT_EQ(decoded.local_endpoint.service_name, "test_service");

    // Tags preserved; span.kind excluded by convert_span.
    bool found_method = false, found_url = false, found_kind = false;
    for (const auto& [k, v] : decoded.tags) {
        if (k == "http.method") { EXPECT_EQ(v, "GET"); found_method = true; }
        if (k == "http.url") { EXPECT_EQ(v, "/api/users"); found_url = true; }
        if (k == "span.kind") { found_kind = true; }
    }
    EXPECT_TRUE(found_method);
    EXPECT_TRUE(found_url);
    EXPECT_FALSE(found_kind);
}

TEST_F(TraceExportersTest, ZipkinSpanProtobufParent) {
    trace_export_config config;
    config.endpoint = "http://zipkin:9411";
    config.format = trace_export_format::zipkin_protobuf;

    zipkin_exporter exporter(config);
    auto zipkin_span = exporter.convert_span(test_spans_[1]);
    auto wire = zipkin_span.to_protobuf();

    kcenon::monitoring::zipkin_proto::span decoded;
    ASSERT_TRUE(kcenon::monitoring::zipkin_proto::decode_span(
        wire.data(), wire.size(), decoded));
    EXPECT_EQ(decoded.parent_id.size(), 8u);
    EXPECT_EQ(decoded.kind, kcenon::monitoring::zipkin_proto::span_kind::client);
}

TEST_F(TraceExportersTest, ZipkinListOfSpansRoundTrip) {
    trace_export_config config;
    config.endpoint = "http://zipkin:9411";
    config.format = trace_export_format::zipkin_protobuf;
    config.service_name = "test_service";

    zipkin_exporter exporter(config);
    std::vector<zipkin_span_data> zipkin_spans;
    for (const auto& s : test_spans_) {
        zipkin_spans.push_back(exporter.convert_span(s));
    }
    auto wire = encode_zipkin_list_of_spans(zipkin_spans);
    ASSERT_FALSE(wire.empty());

    kcenon::monitoring::zipkin_proto::list_of_spans decoded;
    ASSERT_TRUE(kcenon::monitoring::zipkin_proto::decode_list_of_spans(
        wire.data(), wire.size(), decoded));
    EXPECT_EQ(decoded.spans.size(), test_spans_.size());
}

TEST(ZipkinProtoTest, SpanKindParsing) {
    using kcenon::monitoring::zipkin_proto::parse_kind;
    using kcenon::monitoring::zipkin_proto::span_kind;
    EXPECT_EQ(parse_kind("CLIENT"), span_kind::client);
    EXPECT_EQ(parse_kind("server"), span_kind::server);
    EXPECT_EQ(parse_kind("Producer"), span_kind::producer);
    EXPECT_EQ(parse_kind("CONSUMER"), span_kind::consumer);
    EXPECT_EQ(parse_kind(""), span_kind::unspecified);
    EXPECT_EQ(parse_kind("INTERNAL"), span_kind::unspecified);
}

// ==============================================================
// OTLP gRPC Exporter Tests
// ==============================================================

#include <kcenon/monitoring/exporters/otlp_grpc_exporter.h>

class OtlpGrpcExporterTest : public TraceExportersTest {
protected:
    void SetUp() override {
        TraceExportersTest::SetUp();
    }
};

TEST_F(OtlpGrpcExporterTest, ConfigValidation) {
    // Valid configuration
    otlp_grpc_config valid_config;
    valid_config.endpoint = "localhost:4317";
    valid_config.timeout = std::chrono::milliseconds(5000);
    valid_config.max_batch_size = 512;

    auto validation = valid_config.validate();
    EXPECT_TRUE(validation.is_ok());

    // Invalid endpoint (empty)
    otlp_grpc_config invalid_endpoint;
    invalid_endpoint.endpoint = "";
    auto endpoint_validation = invalid_endpoint.validate();
    EXPECT_TRUE(endpoint_validation.is_err());

    // Invalid timeout (zero)
    otlp_grpc_config invalid_timeout;
    invalid_timeout.endpoint = "localhost:4317";
    invalid_timeout.timeout = std::chrono::milliseconds(0);
    auto timeout_validation = invalid_timeout.validate();
    EXPECT_TRUE(timeout_validation.is_err());

    // Invalid batch size (zero)
    otlp_grpc_config invalid_batch;
    invalid_batch.endpoint = "localhost:4317";
    invalid_batch.max_batch_size = 0;
    auto batch_validation = invalid_batch.validate();
    EXPECT_TRUE(batch_validation.is_err());
}

TEST_F(OtlpGrpcExporterTest, SpanConverterBasic) {
    // Test span conversion to OTLP format
    auto payload = otlp_span_converter::convert_to_otlp(
        test_spans_,
        "test_service",
        "1.0.0",
        {{"environment", "test"}});

    // Verify payload is non-empty
    EXPECT_GT(payload.size(), 0u);

    // Verify protobuf wire format structure
    // First byte should be 0x0A (field 1, length-delimited)
    EXPECT_EQ(payload[0], 0x0A);
}

TEST_F(OtlpGrpcExporterTest, SpanConverterEmptySpans) {
    std::vector<trace_span> empty_spans;

    auto payload = otlp_span_converter::convert_to_otlp(
        empty_spans,
        "test_service",
        "1.0.0",
        {});

    // Even with no spans, should have resource data
    EXPECT_GT(payload.size(), 0u);
}

TEST_F(OtlpGrpcExporterTest, ExporterWithStubTransport) {
    otlp_grpc_config config;
    config.endpoint = "localhost:4317";
    config.service_name = "test_service";
    config.service_version = "1.0.0";

    // Create exporter with stub transport for testing
    auto stub_transport = create_stub_grpc_transport();
    auto* stub_ptr = stub_transport.get();

    otlp_grpc_exporter exporter(config, std::move(stub_transport));

    // Start exporter (connects to stub)
    auto start_result = exporter.start();
    EXPECT_TRUE(start_result.is_ok());
    EXPECT_TRUE(exporter.is_running());

    // Export spans
    auto export_result = exporter.export_spans(test_spans_);
    EXPECT_TRUE(export_result.is_ok());

    // Check stats
    auto stats = exporter.get_stats();
    EXPECT_EQ(stats["exported_spans"], test_spans_.size());
    EXPECT_EQ(stats["dropped_spans"], 0u);
    EXPECT_EQ(stats["failed_exports"], 0u);

    // Check transport statistics
    auto transport_stats = stub_ptr->get_statistics();
    EXPECT_EQ(transport_stats.requests_sent, 1u);
    EXPECT_GT(transport_stats.bytes_sent, 0u);

    // Shutdown
    auto shutdown_result = exporter.shutdown();
    EXPECT_TRUE(shutdown_result.is_ok());
    EXPECT_FALSE(exporter.is_running());
}

TEST_F(OtlpGrpcExporterTest, ExporterFailedConnection) {
    otlp_grpc_config config;
    config.endpoint = "localhost:4317";

    // Create stub transport that simulates failure
    auto stub_transport = create_stub_grpc_transport();
    stub_transport->set_simulate_success(false);

    otlp_grpc_exporter exporter(config, std::move(stub_transport));

    // Start should fail due to connection failure
    auto start_result = exporter.start();
    EXPECT_TRUE(start_result.is_err());
}

TEST_F(OtlpGrpcExporterTest, ExporterRetryBehavior) {
    otlp_grpc_config config;
    config.endpoint = "localhost:4317";
    config.max_retry_attempts = 3;
    config.initial_backoff = std::chrono::milliseconds(10);

    // Track call count
    int call_count = 0;

    auto stub_transport = create_stub_grpc_transport();
    stub_transport->set_response_handler([&call_count](const grpc_request&) {
        call_count++;
        grpc_response response;
        if (call_count < 3) {
            // Simulate UNAVAILABLE error (retryable)
            response.status_code = 14;
            response.status_message = "Service unavailable";
        } else {
            // Success on third attempt
            response.status_code = 0;
            response.status_message = "OK";
        }
        return response;
    });

    otlp_grpc_exporter exporter(config, std::move(stub_transport));

    auto start_result = exporter.start();
    EXPECT_TRUE(start_result.is_ok());

    auto export_result = exporter.export_spans(test_spans_);
    EXPECT_TRUE(export_result.is_ok());

    // Should have retried twice before success
    EXPECT_EQ(call_count, 3);

    auto stats = exporter.get_stats();
    EXPECT_EQ(stats["retries"], 2u);
    EXPECT_EQ(stats["exported_spans"], test_spans_.size());
}

TEST_F(OtlpGrpcExporterTest, ExporterDetailedStats) {
    otlp_grpc_config config;
    config.endpoint = "localhost:4317";

    auto stub_transport = create_stub_grpc_transport();
    // Set response handler to simulate realistic export time
    stub_transport->set_response_handler([](const grpc_request&) {
        // Simulate a small delay to ensure measurable export time
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        grpc_response response;
        response.status_code = 0;  // OK
        response.status_message = "OK";
        return response;
    });
    otlp_grpc_exporter exporter(config, std::move(stub_transport));

    auto start_result = exporter.start();
    EXPECT_TRUE(start_result.is_ok());

    // Export multiple batches
    for (int i = 0; i < 3; ++i) {
        auto result = exporter.export_spans(test_spans_);
        EXPECT_TRUE(result.is_ok());
    }

    auto detailed_stats = exporter.get_detailed_stats();
    EXPECT_EQ(detailed_stats.spans_exported, test_spans_.size() * 3);
    EXPECT_EQ(detailed_stats.spans_dropped, 0u);
    EXPECT_EQ(detailed_stats.export_failures, 0u);
    EXPECT_GT(detailed_stats.total_export_time.count(), 0);
}

TEST_F(OtlpGrpcExporterTest, FactoryFunctions) {
    // Test default factory
    auto exporter1 = create_otlp_grpc_exporter();
    EXPECT_TRUE(exporter1 != nullptr);
    EXPECT_EQ(exporter1->config().endpoint, "localhost:4317");

    // Test factory with endpoint
    auto exporter2 = create_otlp_grpc_exporter("collector:4317");
    EXPECT_TRUE(exporter2 != nullptr);
    EXPECT_EQ(exporter2->config().endpoint, "collector:4317");

    // Test factory with config
    otlp_grpc_config config;
    config.endpoint = "otlp.example.com:443";
    config.use_tls = true;
    config.service_name = "my_service";

    auto exporter3 = create_otlp_grpc_exporter(config);
    EXPECT_TRUE(exporter3 != nullptr);
    EXPECT_EQ(exporter3->config().endpoint, "otlp.example.com:443");
    EXPECT_TRUE(exporter3->config().use_tls);
    EXPECT_EQ(exporter3->config().service_name, "my_service");
}

TEST_F(OtlpGrpcExporterTest, ExportEmptySpans) {
    otlp_grpc_config config;
    config.endpoint = "localhost:4317";

    auto stub_transport = create_stub_grpc_transport();
    otlp_grpc_exporter exporter(config, std::move(stub_transport));

    auto start_result = exporter.start();
    EXPECT_TRUE(start_result.is_ok());

    // Export empty vector should succeed without sending
    std::vector<trace_span> empty_spans;
    auto export_result = exporter.export_spans(empty_spans);
    EXPECT_TRUE(export_result.is_ok());

    auto stats = exporter.get_stats();
    EXPECT_EQ(stats["exported_spans"], 0u);
}
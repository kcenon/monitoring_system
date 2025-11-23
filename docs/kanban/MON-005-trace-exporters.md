# MON-005: Implement Jaeger/Zipkin HTTP Transport

**Priority**: MEDIUM
**Est. Duration**: 12h
**Status**: TODO
**Dependencies**: None

---

## Summary

`trace_exporters.h`의 Jaeger/Zipkin exporter들이 stub 구현. 실제 HTTP/gRPC 통신 구현 필요.

## Current State

```cpp
// jaeger_exporter
result_void send_thrift_batch(const std::vector<jaeger_span_data>& spans) {
    (void)spans; // Suppress unused parameter warning
    return common::ok();  // STUB - 실제 전송 없음
}

// zipkin_exporter
result_void send_json_batch(const std::vector<zipkin_span_data>& spans) {
    (void)spans; // Suppress unused parameter warning
    return common::ok();  // STUB - 실제 전송 없음
}
```

## Requirements

### Jaeger Exporter
- [ ] Thrift over HTTP POST (`/api/traces`)
- [ ] gRPC 프로토콜 (선택)
- [ ] Batch compression (gzip)
- [ ] Retry with exponential backoff
- [ ] Connection pooling

### Zipkin Exporter
- [ ] JSON v2 format POST (`/api/v2/spans`)
- [ ] Protobuf format (선택)
- [ ] Batch support
- [ ] Error handling

### Common
- [ ] Timeout 처리
- [ ] TLS/SSL 지원
- [ ] Custom headers (auth token 등)
- [ ] Health check endpoint 확인

## Technical Options

### HTTP Client
1. **libcurl** - 널리 사용, 검증됨
2. **cpp-httplib** - Header-only, 간단
3. **Boost.Beast** - Async 지원

### Recommendation
`cpp-httplib` (header-only) 또는 optional `libcurl` 의존성

## API Design

```cpp
class http_transport {
public:
    struct request {
        std::string url;
        std::string method;
        std::unordered_map<std::string, std::string> headers;
        std::vector<uint8_t> body;
        std::chrono::milliseconds timeout;
    };

    struct response {
        int status_code;
        std::string body;
    };

    virtual result<response> send(const request& req) = 0;
};
```

## Acceptance Criteria

1. Jaeger collector에 span 전송 성공
2. Zipkin server에 span 전송 성공
3. Connection failure 시 적절한 에러 반환
4. 통합 테스트 (docker-compose로 Jaeger/Zipkin 구동)

## Files to Modify

- `include/kcenon/monitoring/exporters/trace_exporters.h`
- `src/exporters/http_transport.cpp` (신규)
- `tests/test_trace_exporters.cpp` (활성화)
- `CMakeLists.txt` (HTTP 라이브러리 의존성)

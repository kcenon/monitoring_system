# 성능 튜닝 가이드

> **Language:** [English](PERFORMANCE_TUNING.md) | **한국어**

## 개요

이 가이드는 프로덕션 환경에서 Monitoring System의 성능을 최적화하기 위한 상세한 지침을 제공합니다. 이 가이드라인을 따라 포괄적인 관찰 가능성을 유지하면서 최소한의 오버헤드를 달성하세요.

## 빠른 시작 체크리스트

- [ ] 적절한 샘플링 비율 구성
- [ ] 적응형 최적화 활성화
- [ ] 적절한 배치 크기 설정
- [ ] 스레드 풀 크기 구성
- [ ] 스토리지 압축 활성화
- [ ] 적절한 스토리지 백엔드 사용
- [ ] 서킷 브레이커 구성
- [ ] 합리적인 타임아웃 설정
- [ ] 메트릭 집계 활성화
- [ ] 메모리 제한 구성

## 성능 목표

| 컴포넌트 | 목표 지연시간 | CPU 오버헤드 | 메모리 사용량 |
|-----------|---------------|--------------|--------------|
| Metric Collection | < 1μs | < 1% | < 10MB |
| Span Creation | < 500ns | < 0.5% | < 5MB |
| Health Check | < 10ms | < 1% | < 5MB |
| Storage Write | < 5ms | < 2% | < 20MB |
| Stream Processing | < 100μs | < 1% | < 10MB |
| **전체 시스템** | **-** | **< 5%** | **< 50MB** |

## 샘플링 전략

### 트레이스 샘플링

```cpp
// 고정 비율 샘플링 (10%)
tracer_config config;
config.sampling_rate = 0.1;

// 부하 기반 적응형 샘플링
adaptive_sampler sampler;
sampler.set_target_throughput(1000); // 최대 1000 traces/sec
sampler.set_min_sampling_rate(0.01); // 최소 1%
sampler.set_max_sampling_rate(1.0);  // 최대 100%
```

### 메트릭 샘플링

```cpp
// 고빈도 메트릭 샘플링
metric_config config;
config.sampling_interval = 100ms; // 100ms마다 샘플
config.aggregation_window = 1s;   // 1초로 집계
```

## 메모리 최적화

### 객체 풀링

```cpp
// 객체 풀 구성
pool_config config;
config.initial_size = 100;
config.max_size = 1000;
config.growth_factor = 2.0;

object_pool<trace_span> span_pool(config);
object_pool<metric_data> metric_pool(config);
```

### 메모리 제한

```cpp
// 메모리 제한 설정
monitoring_config config;
config.max_memory_mb = 50;
config.memory_warning_threshold = 0.8; // 80%에서 경고
config.memory_critical_threshold = 0.95; // 95%에서 위험
```

### 버퍼 튜닝

```cpp
// 버퍼 크기 최적화
buffer_config config;
config.initial_capacity = 1000;
config.max_capacity = 10000;
config.flush_threshold = 0.8; // 80% 차면 플러시
config.flush_interval = 1s;   // 또는 매초마다
```

## CPU 최적화

### 스레드 풀 구성

```cpp
// 스레드 풀 구성
thread_pool_config config;
config.core_threads = std::thread::hardware_concurrency() / 2;
config.max_threads = std::thread::hardware_concurrency();
config.keep_alive_time = 60s;
config.queue_size = 10000;

thread_pool pool(config);
```

### Lock-Free 작업

```cpp
// 높은 처리량을 위해 lock-free 큐 사용
lock_free_queue<metric_data> queue(10000);

// Atomic 카운터
std::atomic<uint64_t> counter{0};
counter.fetch_add(1, std::memory_order_relaxed);
```

### 배치 처리

```cpp
// 배치 처리 구성
batch_processor_config config;
config.batch_size = 100;
config.max_batch_delay = 100ms;
config.parallel_batches = 4;
```

## 스토리지 최적화

### 백엔드 선택

| 백엔드 | 사용 사례 | 쓰기 속도 | 조회 속도 | 스토리지 크기 |
|---------|----------|-------------|-------------|--------------|
| Memory Buffer | 개발/테스트 | 가장 빠름 | 가장 빠름 | 제한적 |
| SQLite | 단일 인스턴스 | 빠름 | 빠름 | 보통 |
| PostgreSQL | 프로덕션 | 보통 | 빠름 | 큼 |
| File (JSON) | 디버깅 | 느림 | 느림 | 큼 |
| File (Binary) | 아카이빙 | 빠름 | 보통 | 작음 |
| S3 | 장기 스토리지 | 느림 | 느림 | 무제한 |

### 압축

```cpp
// 압축 활성화
storage_config config;
config.compression = compression_type::zstd;
config.compression_level = 3; // 속도/압축률 균형
```

### 쓰기 최적화

```cpp
// 배치를 사용한 비동기 쓰기
storage_config config;
config.async_writes = true;
config.write_batch_size = 1000;
config.write_queue_size = 10000;
config.flush_interval = 5s;
```

## 네트워크 최적화

### 연결 풀링

```cpp
// 연결 풀 구성
connection_pool_config config;
config.min_connections = 5;
config.max_connections = 20;
config.connection_timeout = 5s;
config.idle_timeout = 60s;
```

### 프로토콜 선택

```cpp
// 효율적인 프로토콜 사용
exporter_config config;
config.protocol = export_protocol::grpc; // HTTP보다 나음
config.compression = true;
config.batch_size = 100;
```

## 적응형 최적화

### 자동 튜닝

```cpp
// 적응형 최적화 활성화
adaptive_optimizer optimizer;
optimizer.set_target_overhead(5.0); // 최대 5% CPU
optimizer.set_adaptation_interval(60s);
optimizer.enable_auto_tuning(true);

// 모니터링 및 조정
auto decision = optimizer.analyze_and_optimize();
if (decision.should_adjust) {
    optimizer.apply_optimization(decision);
}
```

### 부하 기반 조정

```cpp
// 부하에 따라 조정
if (system_load > 0.8) {
    // 모니터링 오버헤드 감소
    config.sampling_rate *= 0.5;
    config.collection_interval *= 2;
} else if (system_load < 0.3) {
    // 모니터링 상세도 증가
    config.sampling_rate = min(config.sampling_rate * 1.5, 1.0);
    config.collection_interval = max(config.collection_interval / 2, 100ms);
}
```

## 프로파일링 및 벤치마킹

### 내장 프로파일러

```cpp
// 프로파일링 활성화
performance_monitor monitor;
monitor.enable_profiling(true);
monitor.set_profiling_interval(1s);

// 프로파일링 결과 가져오기
auto stats = monitor.get_profiling_stats();
std::cout << "Average latency: " << stats.avg_latency << "\n";
std::cout << "P99 latency: " << stats.p99_latency << "\n";
```

### 벤치마킹

```cpp
// 벤치마크 구성
benchmark_config config;
config.iterations = 10000;
config.warm_up_iterations = 1000;
config.parallel_threads = 10;

auto results = run_benchmark(config);
```

## 구성 템플릿

### 낮은 오버헤드 구성

```cpp
monitoring_config low_overhead_config() {
    monitoring_config config;
    config.sampling_rate = 0.01;          // 1% 샘플링
    config.collection_interval = 10s;      // 10초마다 수집
    config.enable_compression = true;
    config.async_operations = true;
    config.batch_size = 1000;
    config.max_memory_mb = 20;
    return config;
}
```

### 균형 잡힌 구성

```cpp
monitoring_config balanced_config() {
    monitoring_config config;
    config.sampling_rate = 0.1;           // 10% 샘플링
    config.collection_interval = 1s;       // 1초마다 수집
    config.enable_compression = true;
    config.async_operations = true;
    config.batch_size = 500;
    config.max_memory_mb = 50;
    return config;
}
```

### 높은 상세도 구성

```cpp
monitoring_config high_detail_config() {
    monitoring_config config;
    config.sampling_rate = 1.0;           // 100% 샘플링
    config.collection_interval = 100ms;    // 100ms마다 수집
    config.enable_compression = false;     // 빠른 액세스
    config.async_operations = true;
    config.batch_size = 100;
    config.max_memory_mb = 100;
    return config;
}
```

## 성능 모니터링 메트릭

### 추적해야 할 주요 메트릭

```cpp
// 모니터링 시스템 자체 모니터링
struct monitoring_metrics {
    // 처리량
    double metrics_per_second;
    double spans_per_second;
    double health_checks_per_second;

    // 지연시간
    double avg_collection_latency_us;
    double p99_collection_latency_us;
    double max_collection_latency_us;

    // 리소스 사용량
    double cpu_usage_percent;
    double memory_usage_mb;
    double network_bandwidth_mbps;

    // 큐 메트릭
    size_t queue_depth;
    size_t dropped_metrics;
    size_t rejected_spans;
};
```

## 일반적인 성능 문제

### 문제: 높은 CPU 사용량

**증상:**
- CPU 사용량 > 10%
- 느린 애플리케이션 응답

**해결책:**
```cpp
// 샘플링 비율 감소
config.sampling_rate = 0.01;

// 배치 증가
config.batch_size = 1000;

// 적응형 최적화 활성화
optimizer.enable_auto_tuning(true);
```

### 문제: 메모리 증가

**증상:**
- 지속적으로 증가하는 메모리
- OOM 오류

**해결책:**
```cpp
// 메모리 제한 설정
config.max_memory_mb = 50;

// 공격적인 정리 활성화
config.cleanup_interval = 30s;

// 보존 기간 감소
config.retention_period = 1h;
```

### 문제: 스토리지 병목

**증상:**
- 높은 쓰기 지연시간
- 큐 백프레셔

**해결책:**
```cpp
// 더 빠른 백엔드 사용
config.backend = storage_backend_type::memory_buffer;

// 압축 활성화
config.compression = compression_type::zstd;

// 배치 크기 증가
config.write_batch_size = 5000;
```

## 플랫폼별 튜닝

### Linux

```bash
# 파일 디스크립터 증가
ulimit -n 65536

# 커널 파라미터 튜닝
sysctl -w net.core.somaxconn=65535
sysctl -w net.ipv4.tcp_max_syn_backlog=65535

# CPU 어피니티
taskset -c 0-3 ./monitoring_app
```

### macOS

```bash
# 파일 디스크립터 증가
ulimit -n 10240

# 디버그 기능 비활성화
export MALLOC_CHECK_=0
```

### Windows

```powershell
# 스레드 풀 증가
[System.Threading.ThreadPool]::SetMinThreads(50, 50)

# ETW 비활성화
wevtutil set-log "Applications and Services Logs/monitoring" /e:false
```

## 모니터 모니터링

```cpp
// 자체 모니터링 구성
self_monitor_config config;
config.enable = true;
config.report_interval = 60s;
config.alert_on_degradation = true;

// 임계값 설정
config.cpu_alert_threshold = 10.0;      // > 10% CPU이면 경고
config.memory_alert_threshold = 100.0;  // > 100MB이면 경고
config.latency_alert_threshold = 10.0;  // > 10ms이면 경고

// 콜백 등록
monitor.on_performance_degradation([](const auto& metrics) {
    log_warning("Performance degradation detected: {}", metrics);
    // 오버헤드 자동 감소
});
```

## 모범 사례

1. **보수적으로 시작**: 낮은 샘플링 비율로 시작하여 필요에 따라 증가
2. **영향 모니터링**: 항상 모니터링 오버헤드 측정
3. **적응형 기능 사용**: 시스템이 자동 튜닝하도록 허용
4. **정기적으로 프로파일링**: 병목 현상을 조기에 식별
5. **부하 하에서 테스트**: 프로덕션과 유사한 부하에서 구성 검증
6. **변경사항 문서화**: 튜닝 결정 기록 유지
7. **점진적 롤아웃**: 먼저 서브셋에서 구성 변경 테스트

## 성능 문제 해결

### 성능 프로파일링 체크리스트

- [ ] 샘플링 비율 확인
- [ ] 배치 크기 검토
- [ ] 큐 깊이 분석
- [ ] 메모리 사용량 모니터링
- [ ] 네트워크 지연시간 확인
- [ ] 스토리지 성능 검토
- [ ] 락 경합 분석
- [ ] 스레드 풀 활용도 확인
- [ ] 압축 설정 검토
- [ ] 타임아웃 구성 검증

### 성능 테스트

```cpp
// 부하 테스트 구성
load_test_config config;
config.duration = 60s;
config.target_rps = 10000;
config.ramp_up_time = 10s;
config.parallel_clients = 100;

auto results = run_load_test(config);
assert(results.avg_overhead_percent < 5.0);
assert(results.p99_latency_ms < 10.0);
```

## 결론

최적의 성능은 관찰 가능성 요구사항과 시스템 리소스 간의 균형이 필요합니다. 이 가이드를 사용하여:
- 보수적인 설정으로 시작
- 모니터링 오버헤드 모니터링
- 요구사항에 따라 점진적으로 튜닝
- 자동 최적화를 위한 적응형 기능 사용
- 정기적으로 구성 검토 및 조정

자세한 내용은 [Architecture Guide](ARCHITECTURE_GUIDE.md)를 참조하세요.

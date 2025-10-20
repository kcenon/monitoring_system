# 문제 해결 가이드

> **Language:** [English](TROUBLESHOOTING.md) | **한국어**

## 개요

이 가이드는 Monitoring System의 일반적인 문제를 진단하고 해결하는 데 도움을 줍니다. 각 섹션에는 증상, 진단 단계 및 해결책이 포함되어 있습니다.

## 빠른 진단 명령어

```bash
# 시스템 상태 확인
./monitoring_cli status

# 최근 오류 보기
./monitoring_cli errors --last 100

# 헬스 확인
./monitoring_cli health --verbose

# 구성 보기
./monitoring_cli config --show

# 연결 테스트
./monitoring_cli test --component all
```

## 일반적인 문제

### 1. 모니터링 시스템이 시작되지 않음

#### 증상
- 시작 시 애플리케이션 크래시
- 초기화 실패에 대한 오류 메시지
- 프로세스가 즉시 종료됨

#### 진단 단계
```cpp
// 디버그 로깅 활성화
monitoring_config config;
config.log_level = log_level::debug;
config.enable_startup_diagnostics = true;

// 초기화 결과 확인
auto result = monitoring_system.initialize();
if (!result) {
    std::cerr << "Init failed: " << result.get_error().message << "\n";
}
```

#### 해결책

**의존성 누락:**
```bash
# 라이브러리 의존성 확인
ldd monitoring_system
otool -L monitoring_system  # macOS

# 누락된 라이브러리 설치
apt-get install libstdc++6  # Linux
brew install gcc            # macOS
```

**권한 문제:**
```bash
# 파일 권한 확인
ls -la /var/log/monitoring/
ls -la /var/lib/monitoring/

# 권한 수정
sudo chown -R $USER:$USER /var/log/monitoring/
sudo chmod 755 /var/log/monitoring/
```

**포트가 이미 사용 중:**
```cpp
// 포트 변경
exporter_config config;
config.port = 9091;  // 다른 포트 사용
```

### 2. 높은 메모리 사용량

#### 증상
- 지속적으로 증가하는 메모리 소비
- 메모리 부족 오류
- 시스템이 응답하지 않음

#### 진단 단계
```cpp
// 메모리 프로파일링 활성화
memory_profiler profiler;
profiler.start();

// 메모리 통계 가져오기
auto stats = monitoring_system.get_memory_stats();
std::cout << "Total allocated: " << stats.total_allocated_mb << " MB\n";
std::cout << "Active objects: " << stats.active_objects << "\n";
std::cout << "Largest allocation: " << stats.largest_allocation_mb << " MB\n";

// 누수 확인
auto leaks = profiler.detect_leaks();
for (const auto& leak : leaks) {
    std::cout << "Potential leak: " << leak.location << " ("
              << leak.size_bytes << " bytes)\n";
}
```

#### 해결책

**메모리 누수:**
```cpp
// 순환 참조 수정
std::weak_ptr<Component> weak_ref = component;  // weak_ptr 사용

// 적절한 정리 보장
class Resource {
    ~Resource() {
        // 항상 정리
        if (buffer_) {
            delete[] buffer_;
            buffer_ = nullptr;
        }
    }
};

// RAII 사용
auto resource = std::make_unique<Resource>();  // 자동 정리
```

**무제한 큐:**
```cpp
// 큐 제한 설정
queue_config config;
config.max_size = 10000;
config.overflow_policy = overflow_policy::drop_oldest;

// 큐 크기 모니터링
if (queue.size() > config.max_size * 0.9) {
    log_warning("Queue near capacity: {}", queue.size());
}
```

**보존 문제:**
```cpp
// 보존 구성
retention_config config;
config.max_age = 24h;
config.max_size_mb = 1000;
config.cleanup_interval = 1h;
```

### 3. 높은 CPU 사용량

#### 증상
- CPU 사용량이 지속적으로 > 10%
- 애플리케이션이 느려짐
- 시스템 부하 증가

#### 진단 단계
```cpp
// CPU 사용량 프로파일링
cpu_profiler profiler;
profiler.start();

// 60초 동안 실행
std::this_thread::sleep_for(60s);

// 핫스팟 가져오기
auto hotspots = profiler.get_hotspots();
for (const auto& hotspot : hotspots) {
    std::cout << hotspot.function << ": "
              << hotspot.cpu_percent << "%\n";
}
```

#### 해결책

**과도한 샘플링:**
```cpp
// 샘플링 비율 감소
config.sampling_rate = 0.01;  // 100% 대신 1%

// 적응형 샘플링 사용
adaptive_sampler sampler;
sampler.set_max_throughput(1000);  // 1000/sec로 제한
```

**락 경합:**
```cpp
// lock-free 구조 사용
lock_free_queue<Data> queue;  // mutex가 있는 std::queue 대신

// 락 범위 축소
{
    std::lock_guard<std::mutex> lock(mutex_);
    // 락 하에서 최소 작업
    data_ = new_data;
}  // 락 즉시 해제

// read-write 락 사용
std::shared_mutex rw_mutex;
// 여러 리더
std::shared_lock<std::shared_mutex> read_lock(rw_mutex);
// 단일 라이터
std::unique_lock<std::shared_mutex> write_lock(rw_mutex);
```

**비효율적인 알고리즘:**
```cpp
// 효율적인 데이터 구조 사용
std::unordered_map<Key, Value> map;  // O(log n) 대신 O(1)

// 배치 작업
std::vector<Data> batch;
batch.reserve(1000);
// 데이터 수집...
process_batch(batch);  // 한 번에 모두 처리
```

### 4. 메트릭/트레이스 누락

#### 증상
- 대시보드에 데이터 없음
- 불완전한 트레이스
- 누락된 헬스 체크

#### 진단 단계
```cpp
// 수집기가 활성화되어 있는지 확인
for (const auto& collector : monitoring_system.get_collectors()) {
    std::cout << collector->get_name() << ": "
              << (collector->is_enabled() ? "enabled" : "disabled") << "\n";
}

// 데이터 흐름 확인
auto stats = monitoring_system.get_pipeline_stats();
std::cout << "Collected: " << stats.metrics_collected << "\n";
std::cout << "Exported: " << stats.metrics_exported << "\n";
std::cout << "Dropped: " << stats.metrics_dropped << "\n";
```

#### 해결책

**샘플링 문제:**
```cpp
// 샘플링 구성 확인
if (config.sampling_rate < 0.01) {
    log_warning("Very low sampling rate: {}", config.sampling_rate);
}

// 디버깅을 위해 일시적으로 샘플링 비활성화
config.sampling_rate = 1.0;  // 100% 샘플링
```

**내보내기 실패:**
```cpp
// 내보내기 상태 확인
auto status = exporter.get_status();
if (status.last_error) {
    std::cout << "Export error: " << status.last_error.message << "\n";
}

// 재시도 로직 추가
retry_policy<bool> retry;
retry.with_max_attempts(3)
     .with_backoff(exponential_backoff{100ms, 2.0});

auto result = retry.execute([&]() {
    return exporter.export_batch(data);
});
```

**컨텍스트 전파:**
```cpp
// 컨텍스트가 전파되는지 확인
auto span = tracer.start_span("operation");
thread_context::set_current_span(span);

// 자식 스레드에서
auto parent_span = thread_context::get_current_span();
auto child_span = tracer.start_child_span(parent_span, "child_op");
```

### 5. 스토리지 문제

#### 증상
- 스토리지에 쓰기 실패
- 느린 쿼리 성능
- 데이터 손상

#### 진단 단계
```cpp
// 스토리지 백엔드 테스트
storage_diagnostics diag(storage_backend);
auto results = diag.run_tests();

for (const auto& test : results) {
    std::cout << test.name << ": "
              << (test.passed ? "PASS" : "FAIL") << "\n";
    if (!test.passed) {
        std::cout << "  Error: " << test.error_message << "\n";
    }
}

// 스토리지 통계 확인
auto stats = storage_backend.get_stats();
std::cout << "Write latency: " << stats.avg_write_latency_ms << " ms\n";
std::cout << "Read latency: " << stats.avg_read_latency_ms << " ms\n";
std::cout << "Failed writes: " << stats.failed_writes << "\n";
```

#### 해결책

**연결 문제:**
```cpp
// 연결 풀링 추가
connection_pool pool;
pool.set_min_connections(5);
pool.set_max_connections(20);
pool.set_validation_query("SELECT 1");

// 재연결 로직 추가
storage_backend.set_reconnect_policy(
    reconnect_policy{
        .max_attempts = 10,
        .initial_delay = 1s,
        .max_delay = 30s
    }
);
```

**성능 문제:**
```cpp
// 배치 활성화
storage_config config;
config.batch_size = 1000;
config.flush_interval = 5s;

// 캐싱 추가
cache_config cache_cfg;
cache_cfg.max_size_mb = 100;
cache_cfg.ttl = 60s;
storage_backend.enable_cache(cache_cfg);

// 적절한 인덱스 사용
// SQL 백엔드의 경우
storage_backend.execute(
    "CREATE INDEX idx_timestamp ON metrics(timestamp)"
);
```

### 6. 네트워크 문제

#### 증상
- 연결 타임아웃
- 내보내기 실패
- 높은 지연시간

#### 진단 단계
```bash
# 연결 테스트
ping monitoring-backend.example.com
telnet monitoring-backend.example.com 4317
curl -v https://monitoring-backend.example.com/health

# DNS 확인
nslookup monitoring-backend.example.com
dig monitoring-backend.example.com

# 경로 추적
traceroute monitoring-backend.example.com
```

#### 해결책

**타임아웃 문제:**
```cpp
// 타임아웃 증가
network_config config;
config.connect_timeout = 10s;
config.read_timeout = 30s;
config.write_timeout = 30s;

// keep-alive 추가
config.keep_alive = true;
config.keep_alive_interval = 30s;
```

**SSL/TLS 문제:**
```cpp
// TLS 구성
tls_config config;
config.verify_peer = true;
config.ca_cert_path = "/path/to/ca.crt";
config.client_cert_path = "/path/to/client.crt";
config.client_key_path = "/path/to/client.key";

// 테스트용 검증 비활성화 (프로덕션에서는 안 됨!)
config.verify_peer = false;
```

### 7. 서킷 브레이커 문제

#### 증상
- 서킷 브레이커가 항상 열림
- 서킷 브레이커가 절대 열리지 않음
- 예상치 못한 폴백 동작

#### 진단 단계
```cpp
// 서킷 브레이커 상태 확인
auto state = circuit_breaker.get_state();
auto metrics = circuit_breaker.get_metrics();

std::cout << "State: " << to_string(state) << "\n";
std::cout << "Failed calls: " << metrics.failed_calls << "\n";
std::cout << "Success calls: " << metrics.successful_calls << "\n";
std::cout << "Rejected calls: " << metrics.rejected_calls << "\n";
```

#### 해결책

**항상 열림:**
```cpp
// 임계값 조정
circuit_breaker_config config;
config.failure_threshold = 10;  // 임계값 증가
config.failure_ratio = 0.5;     // 50% 실패율
config.reset_timeout = 30s;     // 더 짧은 리셋 시간

// 수동 리셋
circuit_breaker.reset();
```

**절대 열리지 않음:**
```cpp
// 더 민감한 구성
config.failure_threshold = 3;   // 더 낮은 임계값
config.failure_ratio = 0.3;     // 30% 실패율
config.timeout = 1s;            // 더 짧은 타임아웃
```

## 디버깅 도구

### 디버그 로깅 활성화

```cpp
// 전역 디버그 모드
debug_config config;
config.enable_all = true;
config.log_level = log_level::trace;
config.include_timestamps = true;
config.include_thread_id = true;

monitoring_system.enable_debug(config);
```

### 메모리 디버깅

```cpp
// 메모리 추적 활성화
#ifdef DEBUG
class MemoryTracker {
    static std::unordered_map<void*, size_t> allocations_;

public:
    static void* allocate(size_t size) {
        void* ptr = malloc(size);
        allocations_[ptr] = size;
        return ptr;
    }

    static void deallocate(void* ptr) {
        allocations_.erase(ptr);
        free(ptr);
    }

    static void report_leaks() {
        for (const auto& [ptr, size] : allocations_) {
            std::cout << "Leak: " << ptr << " (" << size << " bytes)\n";
        }
    }
};
#endif
```

### 성능 디버깅

```cpp
// 타이밍 매크로
#define TIME_BLOCK(name) \
    auto _timer_##name = monitoring_system::scoped_timer(#name);

// 사용법
void process_request() {
    TIME_BLOCK(process_request);

    {
        TIME_BLOCK(validation);
        validate_input();
    }

    {
        TIME_BLOCK(processing);
        do_work();
    }
}
```

## 헬스 체크 검증

```cpp
// 포괄적인 헬스 체크
class SystemHealthCheck {
public:
    struct HealthReport {
        bool is_healthy;
        std::vector<std::string> issues;
        std::map<std::string, double> metrics;
    };

    HealthReport check_all() {
        HealthReport report;
        report.is_healthy = true;

        // CPU 확인
        auto cpu = get_cpu_usage();
        report.metrics["cpu_percent"] = cpu;
        if (cpu > 80.0) {
            report.issues.push_back("High CPU usage: " + std::to_string(cpu));
            report.is_healthy = false;
        }

        // 메모리 확인
        auto mem = get_memory_usage_mb();
        report.metrics["memory_mb"] = mem;
        if (mem > 1000.0) {
            report.issues.push_back("High memory usage: " + std::to_string(mem));
            report.is_healthy = false;
        }

        // 큐 확인
        auto queue_depth = get_queue_depth();
        report.metrics["queue_depth"] = queue_depth;
        if (queue_depth > 5000) {
            report.issues.push_back("Queue backlog: " + std::to_string(queue_depth));
            report.is_healthy = false;
        }

        return report;
    }
};
```

## 긴급 절차

### 시스템 과부하

```cpp
// 긴급 조절
void emergency_throttle() {
    // 샘플링을 최소로 축소
    config.sampling_rate = 0.001;  // 0.1%

    // 간격 증가
    config.collection_interval = 60s;

    // 중요하지 않은 기능 비활성화
    config.enable_tracing = false;
    config.enable_profiling = false;

    // 큐 정리
    metric_queue.clear();
    trace_queue.clear();

    log_error("Emergency throttling activated");
}
```

### 데이터 복구

```cpp
// 손상된 스토리지에서 복구
void recover_storage() {
    // 기존 데이터 백업
    storage_backend.backup("/tmp/backup");

    // 복구 시도
    storage_backend.repair();

    // 데이터 검증
    auto validation = storage_backend.validate();
    if (!validation.is_valid) {
        // 백업에서 복원
        storage_backend.restore("/tmp/backup");
    }
}
```

## 모니터링 시스템 로그

### 로그 위치

```bash
# 기본 로그 위치
/var/log/monitoring/system.log     # 시스템 로그
/var/log/monitoring/error.log      # 오류 로그
/var/log/monitoring/metrics.log    # 메트릭 로그
/var/log/monitoring/trace.log      # 트레이스 로그
```

### 로그 분석

```bash
# 오류 찾기
grep ERROR /var/log/monitoring/system.log

# 경고 찾기
grep WARN /var/log/monitoring/system.log

# 특정 컴포넌트 찾기
grep "storage_backend" /var/log/monitoring/system.log

# 로그 테일
tail -f /var/log/monitoring/system.log

# 로그 패턴 분석
awk '/ERROR/ {print $4}' system.log | sort | uniq -c | sort -rn
```

## 도움 받기

### 수집할 진단 정보

문제 보고 시 다음을 포함하세요:

1. **시스템 정보:**
```bash
uname -a
cat /etc/os-release
g++ --version
```

2. **구성:**
```cpp
monitoring_system.dump_config("/tmp/config.json");
```

3. **로그:**
```bash
tar -czf logs.tar.gz /var/log/monitoring/
```

4. **스택 추적:**
```bash
gdb ./monitoring_app core
(gdb) bt full
```

5. **성능 메트릭:**
```cpp
auto report = monitoring_system.generate_diagnostic_report();
report.save("/tmp/diagnostic_report.json");
```

### 지원 채널

- GitHub Issues: [버그 보고 및 기능 요청]
- 문서: [API Reference](API_REFERENCE.md)
- 예제: [코드 예제](../examples/)

## 결론

대부분의 문제는 다음을 통해 해결할 수 있습니다:
1. 오류 메시지에 대한 로그 확인
2. 구성 검증
3. 충분한 리소스 보장
4. 연결 테스트
5. 임계값 및 제한 조정

지속적인 문제의 경우 진단 정보를 수집하고 지원 채널에 문의하세요.

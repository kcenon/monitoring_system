# MON-004: Implement Linux/Windows Platform Metrics

**Priority**: HIGH
**Est. Duration**: 16h
**Status**: DONE (Already Implemented)
**Dependencies**: None
**Completed**: 2025-11-24
**Resolution**: Analysis revealed platform metrics were already fully implemented in linux_metrics.cpp and windows_metrics.cpp. Original ticket description was incorrect.

---

## Summary

`src/platform/linux_metrics.cpp`와 `src/platform/windows_metrics.cpp`가 stub 상태. 실제 시스템 메트릭 수집 구현 필요.

## Current State

CMakeLists.txt에 파일이 포함되어 있지만 실제 구현 없음:
```cmake
add_library(monitoring_system STATIC
    ...
    src/platform/linux_metrics.cpp
    src/platform/windows_metrics.cpp
)
```

## Requirements

### Linux Metrics
- [ ] CPU 사용률 (`/proc/stat`)
- [ ] 메모리 사용량 (`/proc/meminfo`)
- [ ] 디스크 I/O (`/proc/diskstats`)
- [ ] 네트워크 I/O (`/proc/net/dev`)
- [ ] 프로세스별 리소스 (`/proc/[pid]/stat`)

### Windows Metrics
- [ ] CPU 사용률 (Performance Counters / WMI)
- [ ] 메모리 사용량 (`GlobalMemoryStatusEx`)
- [ ] 디스크 I/O (Performance Counters)
- [ ] 네트워크 I/O (`GetIfTable2`)
- [ ] 프로세스별 리소스 (`GetProcessMemoryInfo`)

## Technical Design

```cpp
namespace kcenon::monitoring::platform {

struct system_metrics {
    double cpu_usage_percent;
    size_t memory_used_bytes;
    size_t memory_total_bytes;
    size_t disk_read_bytes;
    size_t disk_write_bytes;
    size_t network_rx_bytes;
    size_t network_tx_bytes;
};

class linux_metrics_collector {
public:
    result<system_metrics> collect();
private:
    // /proc 파싱 로직
};

class windows_metrics_collector {
public:
    result<system_metrics> collect();
private:
    // Windows API 호출
};

} // namespace
```

## Platform-Specific Notes

### Linux
- `/proc` 파일시스템 파싱
- `getrusage()` 활용 가능
- cgroups 지원 (컨테이너 환경)

### Windows
- PDH (Performance Data Helper) API
- WMI 대안 제공
- 관리자 권한 불필요하도록 설계

## Acceptance Criteria

1. Linux에서 모든 메트릭 수집 가능
2. Windows에서 모든 메트릭 수집 가능
3. 각 플랫폼별 단위 테스트
4. 크로스컴파일 빌드 성공

## Files to Modify

- `src/platform/linux_metrics.cpp`
- `src/platform/windows_metrics.cpp`
- `include/kcenon/monitoring/platform/` (헤더 추가 필요시)
- `tests/test_platform_metrics.cpp` (신규)

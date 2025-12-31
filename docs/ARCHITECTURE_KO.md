아키텍처 개요
=====================

> **Language:** [English](ARCHITECTURE.md) | **한국어**

목적
- monitoring_system은 C++20 서비스를 위한 metrics 수집, 저장, 분석, 상태 확인 및 이벤트 기반 관찰성을 제공합니다.
- 인터페이스 우선 설계로, 독립 실행 또는 thread_system 및 logger_system 프로젝트와 통합하여 사용할 수 있습니다.

주요 모듈
- Core
  - result_types, error_codes: Result 패턴 및 오류 분류 체계
  - event_bus, event_types: metrics 및 시스템 이벤트를 위한 프로세스 내 pub/sub
- Interfaces
  - monitoring_interface, metric_collector_interface, storage_backend, metrics_analyzer
- Platform Abstraction Layer (Issue #291)
  - metrics_provider: 플랫폼별 metrics 수집을 위한 추상 인터페이스
  - linux_metrics_provider: /proc 및 /sys 파일시스템을 사용하는 Linux 구현
  - macos_metrics_provider: IOKit 및 시스템 API를 사용하는 macOS 구현
  - windows_metrics_provider: WMI 및 시스템 API를 사용하는 Windows 구현
  - 제공 기능: 배터리, 온도, 업타임, 컨텍스트 스위치, FD 통계, inode 통계,
    TCP 상태, 소켓 버퍼, 인터럽트, 전원 정보, GPU 정보, 보안 정보
- Utilities
  - buffer_manager, ring_buffer, time_series, aggregation_processor
- Reliability & Tracing
  - retry_policy, circuit_breaker, fault_tolerance_manager, distributed_tracer
- Adapters (선택적)
  - thread_system_adapter, logger_system_adapter (존재하지 않을 때 우아한 폴백)

통합 토폴로지
```
thread_system ──(metrics)──► monitoring_system ◄──(metrics)── logger_system
        │                                      │
        └──── application components ──────────┘
```

데이터 흐름
1) Collectors가 metrics를 수집(pull 또는 push)하고 event_bus를 통해 metric_collection_event를 발행합니다.
2) Storage backends가 스냅샷을 저장하고, analyzers가 추세 및 알림을 계산합니다.
3) Health checks가 컴포넌트 상태를 health_check_event로 요약합니다.

Thread System 통합
- thread_system이 사용 가능한 경우, thread_system_adapter는 service_container를 통해 monitorable provider를 발견하고 선택된 thread pool metrics를 monitoring metrics로 변환합니다.
- thread_system이 없는 경우, adapter는 빈 집합을 반환하고 no-op 상태를 유지하여 빌드가 정상적으로 유지됩니다.

설계 원칙
- 인터페이스 분리: producers, transport 및 sinks를 분리합니다.
- Back-pressure 준비: 버퍼링 유틸리티 및 제한된 큐를 제공합니다.
- Result 패턴: 명시적 오류 보고(모듈 경계를 넘는 예외 없음).

빌드 및 옵션
- C++20, CMake. 선택적 플래그: USE_THREAD_SYSTEM, BUILD_WITH_LOGGER_SYSTEM.
- macOS/Linux/Windows에서 vcpkg(선택적)를 사용하여 서드파티 라이브러리 지원.

---

*Last Updated: 2025-12-31*

# 변경 로그

> **Language:** [English](CHANGELOG.md) | **한국어**

## 목차

- [[Unreleased]](#unreleased)
  - [추가됨](#추가됨)
  - [변경됨](#변경됨)
- [[4.0.0] - 2024-09-16](#400---2024-09-16)
  - [추가됨 - Phase 4: 핵심 기반 안정화](#추가됨---phase-4-핵심-기반-안정화)
    - [DI Container 구현](#di-container-구현)
    - [테스트 스위트 안정화](#테스트-스위트-안정화)
    - [크로스 플랫폼 호환성](#크로스-플랫폼-호환성)
    - [빌드 시스템 개선](#빌드-시스템-개선)
    - [핵심 아키텍처 개선](#핵심-아키텍처-개선)
  - [변경됨](#변경됨)
  - [수정됨](#수정됨)
  - [기술 세부 사항](#기술-세부-사항)
- [[3.0.0] - 2024-09-14](#300---2024-09-14)
  - [추가됨 - Phase 3: 실시간 알림 시스템 및 웹 대시보드](#추가됨---phase-3-실시간-알림-시스템-및-웹-대시보드)
    - [알림 시스템](#알림-시스템)
    - [웹 대시보드](#웹-대시보드)
    - [API 개선](#api-개선)
    - [성능 개선](#성능-개선)
  - [변경됨](#변경됨)
  - [수정됨](#수정됨)
- [[2.0.0] - 2024-08-01](#200---2024-08-01)
  - [추가됨 - Phase 2: 고급 모니터링 기능](#추가됨---phase-2-고급-모니터링-기능)
    - [분산 추적](#분산-추적)
    - [상태 모니터링](#상태-모니터링)
    - [안정성 기능](#안정성-기능)
    - [저장소 개선](#저장소-개선)
  - [변경됨](#변경됨)
  - [수정됨](#수정됨)
- [[1.0.0] - 2024-06-01](#100---2024-06-01)
  - [추가됨 - Phase 1: 핵심 모니터링 기반](#추가됨---phase-1-핵심-모니터링-기반)
    - [핵심 모니터링](#핵심-모니터링)
    - [플러그인 시스템](#플러그인-시스템)
    - [구성](#구성)
    - [통합](#통합)
  - [의존성](#의존성)
- [[0.9.0-beta] - 2024-05-01](#090-beta---2024-05-01)
  - [추가됨](#추가됨)
  - [알려진 문제](#알려진-문제)
- [버전 번호 지정](#버전-번호-지정)
- [마이그레이션 가이드](#마이그레이션-가이드)
  - [v3.0.0으로 업그레이드](#v300으로-업그레이드)
  - [v2.0.0으로 업그레이드](#v200으로-업그레이드)
- [지원](#지원)

Monitoring System의 모든 주목할 만한 변경 사항이 이 파일에 문서화됩니다.

형식은 [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)를 기반으로 하며,
이 프로젝트는 [Semantic Versioning](https://semver.org/spec/v2.0.0.html)을 준수합니다.

## [Unreleased]

### 수정됨
- **macOS CI 테스트 불안정성** (#300)
  - macOS CI에서 불안정한 `ContextSwitchMonitoring` 테스트 수정
  - macOS는 프로세스 수준의 context switch를 읽으므로 단조 증가가 보장되지 않음
  - macOS에서는 단조 증가 대신 0이 아닌 값 검증으로 assertion 변경
  - Linux에서는 시스템 전체 context switch에 대해 단조 증가 검증 유지
- macOS에서 `struct timeval` 이식성을 위해 명시적 `<sys/time.h>` 헤더 include 추가
- **CRTP collector_base 구현에 맞게 테스트 기대값 정렬** (#306)
  - IsHealthyReflectsState 테스트 수정: 비활성화된 collector는 healthy로 간주 (에러 없음)
  - MetricsHaveCorrectTags 테스트 수정: 실제 collector_name 값 사용
  - socket_buffer_collector에 누락된 'available' 통계 추가

### 변경됨
- **collector 구현에 CRTP 패턴 적용** (#292)
  - 공통 collector 기능을 위한 `collector_base` 템플릿 클래스 생성 (CRTP)
  - 8개 collector를 collector_base 사용으로 마이그레이션: uptime, fd, battery, tcp_state,
    context_switch, inode, interrupt, socket_buffer
  - 공통 기능 추출: enabled 상태, 통계, 에러 처리, 메트릭 생성
  - collector 간 코드 중복 감소 (~400줄 이상 제거)
  - 런타임 오버헤드 없는 컴파일 타임 다형성 패턴
- **system_resources를 중첩 구조체로 재구성** (#293)
  - 35개 필드의 평면 구조체를 논리적으로 그룹화된 중첩 하위 구조체로 재구성
  - CPU 메트릭을 `cpu` 아래에 `load_average` 중첩 구조체와 함께 그룹화
  - 메모리 메트릭을 `swap_info` 중첩 구조체와 함께 그룹화
  - 디스크 메트릭을 `io_throughput` 중첩 구조체와 함께 그룹화
  - 더 깔끔한 접근 패턴: `resources.cpu_usage_percent` 대신 `resources.cpu.usage_percent`
  - 부분 접근 가능 (예: 필요할 때 `resources.cpu`만 전달)

### 추가됨
- **Windows metrics_provider 구현** (#291, Phase 4: #297)
  - WMI 및 Win32 API를 사용한 완전한 `windows_metrics_provider` 구현
  - 배터리 메트릭: WMI Win32_Battery + GetSystemPowerStatus 폴백
  - 온도: WMI MSAcpi_ThermalZoneTemperature
  - 업타임: GetTickCount64
  - 파일 디스크립터: GetProcessHandleCount (Windows 핸들)
  - TCP 상태: GetExtendedTcpTable API 전체 상태 추적
  - 전원 정보: GetSystemPowerStatus + WMI 배터리 전압
  - 스텁: context switch, 인터럽트, GPU, 보안, 소켓 버퍼
- **플랫폼 추상화 레이어 인터페이스** (#291, Phase 1: #294)
  - 통합 플랫폼 메트릭을 위한 `metrics_provider` 추상 인터페이스 추가
  - 공통 데이터 구조 정의 (uptime_info, context_switch_info, fd_info 등)
  - 플랫폼별 provider 헤더 생성 (linux, macos, windows)
  - 자동 플랫폼 감지를 위한 factory 함수 구현
  - 39개의 플랫폼별 파일을 통합 레이어로 통합하기 위한 기반
- **vcpkg manifest: 에코시스템 의존성 추가** (#277)
  - `kcenon-common-system`을 필수 의존성으로 추가
  - `kcenon-thread-system`을 필수 의존성으로 추가
  - `kcenon-logger-system` 의존성을 가진 `logging` 기능 추가
  - vcpkg 에코시스템 표준 템플릿 준수
- **UDP 및 gRPC 전송 구현** (#273)
  - UDP 통신용 추상 인터페이스 `udp_transport.h` 추가
  - gRPC 통신용 추상 인터페이스 `grpc_transport.h` 추가
  - `stub_udp_transport`: 시뮬레이션 UDP 전송용 테스트 구현
  - `stub_grpc_transport`: 시뮬레이션 gRPC 호출용 테스트 구현
  - `common_udp_transport`: common_system IUdpClient 인터페이스 통합
  - `network_udp_transport`: network_system UDP 클라이언트 통합
  - 가용성에 따른 자동 백엔드 선택 팩토리 함수
- **실제 UDP 전송을 사용하는 StatsD 익스포터** (#274)
  - `statsd_exporter`가 `udp_transport` 추상화 사용하도록 업데이트
  - 테스트용 커스텀 전송 주입 지원
  - 연결 관리를 포함한 start/stop 라이프사이클 관리
  - 익스포터 통계에 전송 통계 포함
- **HTTP/gRPC 전송을 사용하는 OTLP 익스포터** (#275)
  - `otlp_metrics_exporter`가 전송 추상화 사용하도록 업데이트
  - `http_transport`를 통한 OTLP/HTTP 지원 (JSON 및 Protobuf)
  - `grpc_transport`를 통한 OTLP/gRPC 지원
  - 메트릭용 기본 OTLP JSON 직렬화
- **CMake 전송 인터페이스 감지**
  - common_system 전송 인터페이스 자동 감지
  - `MONITORING_HAS_COMMON_TRANSPORT_INTERFACES` 컴파일 정의
- 문서 구조 재구성
- **Deprecated API 경고 플래그** (#267)
  - GCC/Clang 컴파일러용 `-Wdeprecated-declarations` 추가
  - MSVC 컴파일러용 `/w14996` 추가
  - common_system v3.0.0 제거 전에 deprecated API 사용 감지
  - 코드베이스에서 deprecated API 사용 없음 (이미 새로운 패턴 사용 중)
- **common_system v3.0.0 호환성 확인** (#269)
  - deprecated `THREAD_LOG_*` 매크로 사용하지 않음 확인 (이미 `LOG_*` 매크로 사용 중)
  - legacy `log(level, msg, file, line, func)` 메서드 사용하지 않음 확인
  - 코드베이스가 이미 자동 source_location을 사용하는 modern `log(level, msg)` 사용 중
  - common_system v3.0.0 업그레이드 시 코드 변경 불필요

### 수정됨
- **테스트 mock 클래스의 deprecated ILogger API 사용 문제** (#272)
  - mock_logger에서 deprecated 5인자 `log(level, msg, file, line, func)` override 제거
  - common_system v3.0.0 ILogger 인터페이스에 맞게 mock 클래스 업데이트 (Issue #217)
  - deprecated API는 common_system에서 source_location 기반 API로 대체됨
- **FetchContent를 통해 monitoring_system 사용 시 CMake 오류 수정** (#261)
  - `kcenon::common_system` 타겟 별칭을 찾을 수 없는 오류 수정
  - 여러 common_system 타겟 이름 지원 (`kcenon::common_system`, `kcenon::common`, `common_system`, `common`)
  - 기본 타겟은 있지만 네임스페이스 별칭이 없을 때 자동 별칭 생성
  - include 디렉터리를 사용한 헤더 전용 통합으로 폴백

### 변경됨
- **통합 에러 처리를 위한 common_system Result<T> 도입** (#259)
  - `result<T>` 및 `result_void` 타입 별칭을 deprecated로 표시
  - 헬퍼 함수들 (`make_success`, `make_error`, `make_void_success`, `make_void_error`, `make_result_void`, `make_error_with_context`)을 deprecated로 표시
  - `MONITORING_TRY` 및 `MONITORING_TRY_ASSIGN` 매크로를 deprecated로 표시
  - 코드 예제와 함께 마이그레이션 문서 추가
  - 새 코드는 `common::Result<T>`, `common::VoidResult`, `common::ok()`, `common::make_error()`를 직접 사용해야 함
  - 자세한 예제는 `result_types.h`의 마이그레이션 가이드 참조
- 포괄적인 기여 가이드라인
- 보안 정책 문서
- **컨테이너 메트릭 모니터링** (#228)
  - Docker/Podman 컨테이너 감지 및 메트릭 수집
  - 컨테이너 CPU, 메모리, 네트워크, I/O 모니터링
  - 컨테이너 상태 추적 (실행 중, 일시 정지, 중지)
  - cgroups v1/v2 및 `/proc` 파일시스템을 통한 Linux 지원
  - macOS/Windows 스텁 구현
- **SMART 디스크 상태 모니터링** (#227)
  - smartctl을 통한 S.M.A.R.T. 디스크 상태 메트릭 수집
  - 온도, 재할당된 섹터, 전원 켜진 시간 모니터링
  - 대기 중인 섹터 및 수정 불가능한 오류 추적
  - 크로스 플랫폼 지원 (Linux, smartmontools를 사용하는 macOS)
- **하드웨어 온도 모니터링** (#215)
  - CPU, GPU, 시스템 온도 수집
  - 코어별 CPU 온도 추적
  - hwmon sysfs 및 lm-sensors를 통한 Linux 지원
  - SMC (System Management Controller)를 통한 macOS 지원
- **파일 디스크립터 사용량 모니터링** (#220)
  - 시스템 전체 및 프로세스별 FD 추적
  - FD 제한 모니터링 (소프트/하드 제한)
  - 사전 FD 고갈 감지
  - `/proc/sys/fs/file-nr` 및 `/proc/self/fd`를 통한 Linux 지원
  - `getrlimit()` 및 디렉터리 열거를 통한 macOS 지원
- **Inode 사용량 모니터링** (#224)
  - 파일시스템별 inode 사용 메트릭
  - Inode 고갈 위험 감지
  - `statvfs()`를 통한 Linux/macOS 지원
- **TCP 연결 상태 모니터링** (#225)
  - 연결 상태 카운트 (ESTABLISHED, TIME_WAIT, CLOSE_WAIT 등)
  - IPv4 및 IPv6 연결 추적
  - 연결 누수 감지 (CLOSE_WAIT 누적)
  - `/proc/net/tcp` 및 `/proc/net/tcp6`를 통한 Linux 지원
  - `netstat`를 통한 macOS 지원
- **인터럽트 통계 모니터링** (#223)
  - 하드웨어 인터럽트 카운팅 및 비율 계산
  - 소프트 인터럽트 추적 (Linux)
  - CPU별 인터럽트 분석 (선택 사항)
  - `/proc/stat` 및 `/proc/softirqs`를 통한 Linux 지원
  - `host_statistics64()`를 통한 macOS 지원
- **전력 소비 모니터링** (#216)
  - Intel RAPL을 통한 CPU/패키지 전력 소비
  - 배터리 전력 및 충전 속도 모니터링
  - Joules 단위의 에너지 소비 추적
  - powercap sysfs를 통한 Linux 지원
  - IOKit 및 SMC를 통한 macOS 지원
- **GPU 메트릭 모니터링** (#221)
  - 다중 벤더 지원 (NVIDIA, AMD, Intel, Apple)
  - GPU 사용률, VRAM 사용량, 온도, 전력, 클럭 속도
  - 팬 속도 모니터링
  - sysfs 및 hwmon을 통한 Linux 지원
  - IOKit을 통한 macOS 지원
- **소켓 버퍼 사용량 모니터링** (#226)
  - TCP 송신/수신 큐 모니터링
  - 소켓 메모리 소비 추적
  - 네트워크 병목 현상 감지
  - `/proc/net/tcp` 및 `/proc/net/sockstat`를 통한 Linux 지원
  - `netstat` 및 `sysctl`을 통한 macOS 지원
- **보안 이벤트 모니터링** (#230)
  - 로그인 성공/실패 추적
  - sudo 사용 및 권한 상승 모니터링
  - 계정 생성/삭제 이벤트
  - 세션 추적
  - auth.log 파싱을 통한 Linux 지원
- **가상화 메트릭 모니터링** (#229)
  - VM 환경 감지 (VMware, VirtualBox, Hyper-V, KVM 등)
  - 게스트 CPU steal time 모니터링
  - 하이퍼바이저 벤더 식별
  - DMI 및 cpuinfo를 통한 Linux 지원
  - sysctl을 통한 macOS 지원
- **컨텍스트 스위치 통계 모니터링** (#222)
  - 시스템 전체 컨텍스트 스위치 카운팅 및 비율
  - 프로세스별 자발적/비자발적 스위치 추적
  - 스케줄링 오버헤드 분석
  - `/proc/stat` 및 `/proc/self/status`를 통한 Linux 지원
  - `task_info()`를 통한 macOS 지원
- **로드 평균 히스토리 추적** (#219)
  - 1/5/15분 로드 평균 수집
  - 시계열 버퍼를 통한 과거 로드 평균 데이터
  - 트렌드 분석 지원
  - 크로스 플랫폼 지원 (Linux, macOS, Windows)
- **시스템 가동 시간 모니터링** (#217)
  - 크로스 플랫폼 가동 시간 추적 (Linux, macOS, Windows)
  - 부팅 타임스탬프 및 가동 시간 메트릭
  - Linux에서 `/proc/uptime`을 통한 유휴 시간 추적
  - `sysctl(KERN_BOOTTIME)`을 통한 macOS 지원
  - `GetTickCount64()`를 통한 Windows 지원
- **배터리 상태 모니터링** (#218)
  - 크로스 플랫폼 배터리 모니터링 (Linux, macOS, Windows)
  - 배터리 잔량 퍼센트, 충전 상태, 시간 추정
  - 배터리 건강도 퍼센트 및 사이클 횟수
  - 전압, 전류, 전력 메트릭
  - 온도 모니터링 (가능한 경우)
  - `/sys/class/power_supply/BAT*` sysfs를 통한 Linux 지원
  - IOKit (AppleSmartBattery)를 통한 macOS 지원
  - GetSystemPowerStatus() 및 WMI를 통한 Windows 지원
- **C++20 Concepts 지원** (#247)
  - 메트릭, 이벤트, 수집기를 위한 개념이 포함된 `monitoring_concepts.h` 추가
  - `event_bus_interface.h`에 개념 추가: `EventType`, `EventHandler`, `EventFilter`
  - `metric_collector_interface.h`에 개념 추가: `Validatable`, `MetricSourceLike`, `MetricCollectorLike`
  - 명확한 오류 메시지와 함께 컴파일 타임 타입 검증
  - 개념 만족 검증을 위한 static assert

### 변경됨
- **C++20 이제 필수**: Concepts 지원을 위해 C++17에서 C++20으로 업그레이드
  - 컴파일러 요구 사항 업데이트: GCC 10+, Clang 10+, MSVC 2019 16.3+
  - BUILD_WITH_COMMON_SYSTEM 정의 시 common_system C++20 Concepts 통합
- **logger_system이 이제 선택 사항**: 필수 의존성에서 선택적 의존성으로 변경 (#213)
  - monitoring_system이 이제 런타임 바인딩을 위해 common_system의 ILogger 인터페이스 사용
  - logger_system을 의존성 주입을 통해 런타임에 주입 가능
  - logger_system에 대한 컴파일 타임 의존성 제거
  - MONITORING_WITH_LOGGER_SYSTEM 옵션 기본값 OFF
- 문서를 중앙 집중식 구조로 통합
- **fmt 라이브러리 fallback 제거**: CMake 설정이 이제 C++20 `std::format`만 요구함
  - 외부 의존성 fallback 로직을 제거하여 빌드 설정 단순화
  - 생태계 전체의 C++20 기능 표준화의 일부
  - 관련: thread_system#219, container_system#168, network_system#257, database_system#203, logger_system#218
- **macOS CI 러너 13에서 14로 업그레이드**: C++20 `std::format` 지원을 위해 필요
  - macOS-13의 Apple Clang은 `std::format`을 지원하지 않음
  - macOS-14 (Sonoma)는 `std::format`을 지원하는 Apple Clang 15+ 포함
  - M1/M2 아키텍처를 위해 triplet을 `x64-osx`에서 `arm64-osx`로 업데이트

## [4.0.0] - 2024-09-16

### 추가됨 - Phase 4: 핵심 기반 안정화

#### DI Container 구현
- **완전한 의존성 주입 컨테이너**: 완전한 서비스 등록, 해결 및 생명주기 관리
- **서비스 생명주기**: 일시적, 싱글톤 및 명명된 서비스 지원
- **타입 안전 해결**: 컴파일 타임 타입 체킹이 있는 템플릿 기반 서비스 해결
- **인스턴스 관리**: 싱글톤 서비스에 대한 자동 생명주기 관리
- **오류 처리**: 누락되거나 잘못된 서비스에 대한 포괄적인 오류 보고

#### 테스트 스위트 안정화
- **37개 핵심 테스트 통과**: 구현된 모든 기능에 대해 100% 성공률
- **테스트 범주**: Result 타입 (13), DI container (9), monitorable 인터페이스 (9), thread context (6)
- **비활성화된 테스트**: 구현되지 않은 기능에 대해 3개 테스트 일시적으로 비활성화됨
- **테스트 프레임워크**: 포괄적인 어설션이 있는 Google Test 통합
- **지속적 검증**: CI/CD 파이프라인에서 자동 테스트 실행

#### 크로스 플랫폼 호환성
- **Windows CI 준수**: MSVC warning-as-error 컴파일 문제 수정
- **파라미터 억제**: performance_monitor.cpp 및 distributed_tracer.h에서 적절한 미사용 파라미터 처리 추가
- **컴파일러 지원**: GCC 11+, Clang 14+, MSVC 2019+와의 호환성 검증됨
- **빌드 시스템**: 모든 플랫폼에 대해 최적화된 CMake 구성

#### 빌드 시스템 개선
- **정적 라이브러리 생성**: libmonitoring_system.a (7.2MB) 빌드 성공
- **예제 애플리케이션**: 모든 4개 예제가 성공적으로 컴파일되고 실행됨
- **CMake 통합**: 적절한 의존성 관리가 있는 간소화된 빌드 구성
- **테스팅 인프라**: CTest와 통합된 Google Test 프레임워크

#### 핵심 아키텍처 개선
- **Result 패턴**: 오류 처리를 위한 포괄적인 Result<T> 구현
- **Thread Context**: 모니터링 작업을 위한 스레드 안전 컨텍스트 관리
- **Stub 구현**: 향후 개발을 위한 기반을 제공하는 기능적 stub
- **모듈식 디자인**: 점진적 기능 확장을 허용하는 깔끔한 분리

### 변경됨
- **테스트 단순화**: 실제 API 구현에 맞게 thread_context 테스트 단순화
- **오류 처리**: 코드베이스 전체에서 오류 보고 개선
- **코드 품질**: 코드 일관성 개선 및 경고 노이즈 감소
- **문서**: 실제 구현 상태를 반영하도록 모든 문서 업데이트

### 수정됨
- **컴파일 문제**: 테스트 파일 전체에서 기본 컴파일 문제 해결
- **경고 억제**: Windows CI 미사용 파라미터 경고 수정
- **테스트 안정성**: 신뢰할 수 있는 실행으로 핵심 테스트 스위트 안정화
- **빌드 구성**: 작동하는 컴포넌트에 대해서만 CMake 최적화

### 기술 세부 사항
- **아키텍처**: 기능적 stub 구현이 있는 모듈식 디자인
- **의존성**: 선택적 thread_system/logger_system 통합이 있는 독립 실행형 작동
- **테스트 커버리지**: 구현된 기능의 100% 커버리지
- **성능**: 최소 오버헤드로 핵심 작업에 최적화됨

## [3.0.0] - 2024-09-14

### 추가됨 - Phase 3: 실시간 알림 시스템 및 웹 대시보드

#### 알림 시스템
- **규칙 기반 알림 엔진**: 임계값 및 비율 기반 조건이 있는 구성 가능한 알림 규칙
- **다중 채널 알림**: 이메일, SMS, 웹훅 및 Slack 알림 지원
- **알림 심각도 수준**: 치명적, 경고, 정보 및 디버그 심각도 분류
- **알림 상태 관리**: 알림 생명주기의 적절한 추적 (대기 중, 발생 중, 해결됨)
- **알림 집계**: 유사한 알림의 스마트 그룹화 및 중복 제거
- **에스컬레이션 정책**: 시간 및 심각도에 기반한 자동 알림 에스컬레이션

#### 웹 대시보드
- **실시간 시각화**: WebSocket 스트리밍이 있는 라이브 메트릭 대시보드
- **대화형 차트**: 확대, 이동 및 필터링 기능이 있는 시계열 차트
- **반응형 UI**: 적응형 레이아웃이 있는 모바일 친화적 인터페이스
- **다중 패널 지원**: 드래그 앤 드롭 패널이 있는 사용자 정의 가능한 대시보드 레이아웃
- **과거 데이터 보기**: 구성 가능한 시간 범위가 있는 과거 메트릭에 액세스
- **알림 관리 UI**: 웹 인터페이스를 통해 알림 보기, 승인 및 관리

#### API 개선
- **RESTful API**: 메트릭, 알림 및 대시보드 관리를 위한 완전한 REST API
- **WebSocket 지원**: 라이브 대시보드 업데이트를 위한 실시간 데이터 스트리밍
- **인증**: 기본 인증 및 API 키 지원
- **CORS 지원**: 웹 통합을 위한 교차 출처 리소스 공유
- **메트릭 집계 API**: 집계된 메트릭 쿼리를 위한 엔드포인트
- **대시보드 구성 API**: 동적 대시보드 생성 및 관리

#### 성능 개선
- **최적화된 저장소**: 압축이 있는 개선된 시계열 데이터 저장소
- **효율적인 쿼리**: 인덱싱 및 캐싱이 있는 최적화된 메트릭 쿼리
- **메모리 관리**: 스마트 데이터 보존으로 메모리 풋프린트 감소
- **연결 풀링**: 효율적인 WebSocket 연결 관리
- **배치 처리**: 최적화된 배치 메트릭 수집 및 처리

### 변경됨
- **아키텍처 리팩터링**: Observer 패턴이 있는 이벤트 주도 아키텍처
- **저장소 엔진**: 더 나은 압축이 있는 향상된 시계열 저장소
- **구성 시스템**: 모든 컴포넌트에 걸친 통합 구성 관리
- **오류 처리**: 전체에 걸친 result 패턴이 있는 포괄적인 오류 처리

### 수정됨
- **메모리 누수**: 메트릭 수집 파이프라인의 메모리 누수 해결
- **스레딩 문제**: 동시 메트릭 처리에서 경합 조건 수정
- **WebSocket 안정성**: WebSocket 연결 안정성 개선
- **메트릭 정밀도**: 고주파 메트릭에 대한 숫자 정밀도 향상

## [2.0.0] - 2024-08-01

### 추가됨 - Phase 2: 고급 모니터링 기능

#### 분산 추적
- **추적 컨텍스트 전파**: 분산 추적 상관 관계 지원
- **Span 관리**: 부모-자식 관계가 있는 계층적 span 추적
- **추적 샘플링**: 성능 최적화를 위한 구성 가능한 샘플링 전략
- **추적 내보내기**: OpenTelemetry 호환 형식으로 추적 내보내기

#### 상태 모니터링
- **컴포넌트 상태 검사**: 개별 컴포넌트 상태 추적
- **의존성 모니터링**: 외부 의존성 상태 검증
- **상태 집계**: 컴포넌트로부터 전체 시스템 상태 계산
- **상태 임계값**: 구성 가능한 상태 점수 임계값 및 알림

#### 안정성 기능
- **Circuit Breaker 패턴**: 자동 장애 감지 및 복구
- **재시도 메커니즘**: 지수 백오프가 있는 구성 가능한 재시도 정책
- **Bulkhead 패턴**: 장애 허용을 위한 리소스 격리
- **우아한 성능 저하**: 서비스 중단에 대한 폴백 메커니즘

#### 저장소 개선
- **Ring Buffer 저장소**: 시계열 데이터를 위한 고성능 순환 버퍼
- **데이터 압축**: 구성 가능한 압축 알고리즘이 있는 효율적인 저장소
- **보존 정책**: 나이 및 저장소 제한에 기반한 자동 데이터 정리
- **저장소 백엔드**: 여러 저장소 백엔드 구현 지원

### 변경됨
- **Observer 패턴**: Observer 패턴을 사용하여 이벤트 시스템 리팩터링
- **플러그인 아키텍처**: 확장 가능한 수집기를 위한 향상된 플러그인 시스템
- **성능 최적화**: 데이터 수집에서 상당한 성능 개선

### 수정됨
- **동시성 문제**: 다중 스레드 시나리오에서 경합 조건 해결
- **메모리 관리**: 메모리 효율성 및 누수 방지 개선

## [1.0.0] - 2024-06-01

### 추가됨 - Phase 1: 핵심 모니터링 기반

#### 핵심 모니터링
- **기본 메트릭 수집**: 카운터, 게이지 및 히스토그램 메트릭 타입
- **메트릭 레지스트리**: 메트릭 등록 및 관리를 위한 중앙 레지스트리
- **시계열 저장소**: 기본 시계열 데이터 저장소 기능
- **메트릭 내보내기**: 다양한 내보내기 형식 지원 (JSON, CSV, Prometheus)

#### 플러그인 시스템
- **수집기 플러그인**: 사용자 정의 수집기를 위한 확장 가능한 플러그인 아키텍처
- **시스템 메트릭**: CPU, 메모리, 디스크 및 네트워크 메트릭을 위한 내장 수집기
- **애플리케이션 메트릭**: 사용자 정의 애플리케이션별 메트릭 지원
- **플러그인 관리**: 동적 플러그인 로딩 및 언로딩 기능

#### 구성
- **YAML 구성**: 사람이 읽을 수 있는 구성 파일 지원
- **환경 변수**: 환경 변수를 통한 구성 재정의
- **검증**: 상세한 오류 보고가 있는 구성 검증
- **Hot Reload**: 재시작 없이 런타임 구성 업데이트

#### 통합
- **Thread System 통합**: 향상된 성능을 위한 thread_system과의 선택적 통합
- **Logger 통합**: 구조화된 로깅을 위한 logger_system과의 선택적 통합
- **독립 실행형 작동**: 외부 의존성 없이 완전한 기능

### 의존성
- C++20 호환 컴파일러
- CMake 3.16+
- 선택 사항: 향상된 스레딩 기능을 위한 thread_system
- 선택 사항: 구조화된 로깅을 위한 logger_system

## [0.9.0-beta] - 2024-05-01

### 추가됨
- 초기 베타 릴리스
- 기본 모니터링 인프라
- 개념 증명 구현

### 알려진 문제
- 제한된 문서
- 성능이 최적화되지 않음
- 기본 테스트 커버리지

---

## 버전 번호 지정

이 프로젝트는 [Semantic Versioning](https://semver.org/)을 사용합니다:

- **MAJOR**: 호환되지 않는 API 변경
- **MINOR**: 하위 호환 가능한 방식으로 새로운 기능
- **PATCH**: 하위 호환 가능한 버그 수정

## 마이그레이션 가이드

### v3.0.0으로 업그레이드

v3.0.0 릴리스는 API에 중대한 변경을 도입합니다:

1. **구성 변경**:
   - 알림 구성이 `alerting` 섹션으로 이동됨
   - 대시보드 구성이 `dashboard` 섹션에 추가됨

2. **API 변경**:
   - `MetricsCollector::collect()`가 이제 `Result<MetricsSnapshot>`를 반환함
   - 알림 규칙 API가 함수 기반에서 클래스 기반으로 변경됨

3. **마이그레이션 단계**:
   ```cpp
   // 이전 방식 (v2.x)
   auto metrics = collector.collect();

   // 새로운 방식 (v3.x)
   auto result = collector.collect();
   if (result.is_ok()) {
       auto metrics = result.value();
   }
   ```

### v2.0.0으로 업그레이드

1. **플러그인 API 변경**: 더 나은 확장성을 위해 플러그인 인터페이스 업데이트됨
2. **구성 형식**: 명확성을 위해 일부 구성 키 이름이 변경됨
3. **저장소 변경**: 더 나은 성능을 위해 시계열 저장소 형식 업데이트됨

상세한 마이그레이션 지침은 [ARCHITECTURE_GUIDE.md](ARCHITECTURE_GUIDE.md)를 참조하세요.

## 지원

- **Issues**: [GitHub Issues](https://github.com/kcenon/monitoring_system/issues)
- **Discussions**: [GitHub Discussions](https://github.com/kcenon/monitoring_system/discussions)
- **Security**: 보안 관련 문제는 [SECURITY.md](SECURITY.md)를 참조하세요

---

*Last Updated: 2025-12-10*

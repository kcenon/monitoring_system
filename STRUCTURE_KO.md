# Monitoring System - 프로젝트 구조

> **언어 선택 (Language)**: [English](STRUCTURE.md) | **한국어**

## 📁 디렉토리 레이아웃

```
monitoring_system/
├── 📁 include/kcenon/monitoring/ # Public headers & interfaces
│   ├── 📁 core/                  # Core monitoring APIs
│   │   ├── monitor.h             # Main monitoring interface
│   │   ├── metrics_manager.h     # Metrics collection manager
│   │   ├── alert_manager.h       # Alert management system
│   │   └── dashboard.h           # Web dashboard interface
│   ├── 📁 interfaces/            # Interface definitions
│   │   ├── collector_interface.h # Base collector interface
│   │   ├── exporter_interface.h  # Base exporter interface
│   │   ├── storage_interface.h   # Storage interface
│   │   └── notifier_interface.h  # Notification interface
│   ├── 📁 collectors/            # Collector interfaces
│   │   ├── system_collector.h    # System metrics collector
│   │   ├── process_collector.h   # Process metrics collector
│   │   ├── network_collector.h   # Network metrics collector
│   │   └── custom_collector.h    # Custom metrics collector
│   ├── 📁 exporters/             # Exporter interfaces
│   │   ├── prometheus_exporter.h # Prometheus metrics exporter
│   │   ├── influxdb_exporter.h   # InfluxDB exporter
│   │   ├── json_exporter.h       # JSON format exporter
│   │   └── csv_exporter.h        # CSV format exporter
│   ├── 📁 storage/               # Storage interfaces
│   │   ├── memory_storage.h      # In-memory storage
│   │   ├── disk_storage.h        # Persistent disk storage
│   │   └── distributed_storage.h # Distributed storage
│   └── 📁 utils/                 # Public utilities
│       ├── time_series.h         # Time series data structures
│       ├── aggregation.h         # Data aggregation utilities
│       ├── sampling.h            # Sampling strategies
│       └── health_check.h        # Health checking utilities
├── 📁 src/                       # Implementation files
│   ├── 📁 core/                  # Core implementation
│   │   ├── monitor.cpp           # Main monitoring implementation
│   │   ├── metrics_manager.cpp   # Metrics management
│   │   ├── alert_manager.cpp     # Alert management
│   │   └── dashboard.cpp         # Web dashboard implementation
│   ├── 📁 impl/                  # Private implementations
│   │   ├── 📁 collectors/        # Metric collectors
│   │   │   ├── system_collector.cpp
│   │   │   ├── process_collector.cpp
│   │   │   ├── network_collector.cpp
│   │   │   └── thread_collector.cpp
│   │   ├── 📁 exporters/         # Data exporters
│   │   │   ├── prometheus_exporter.cpp
│   │   │   ├── influxdb_exporter.cpp
│   │   │   ├── json_exporter.cpp
│   │   │   └── websocket_exporter.cpp
│   │   ├── 📁 storage/           # Storage engines
│   │   │   ├── memory_storage.cpp
│   │   │   ├── disk_storage.cpp
│   │   │   ├── ring_buffer.cpp
│   │   │   └── compression.cpp
│   │   ├── 📁 web/               # Web dashboard
│   │   │   ├── http_server.cpp
│   │   │   ├── websocket_server.cpp
│   │   │   ├── api_handler.cpp
│   │   │   └── static_files.cpp
│   │   ├── 📁 alerting/          # Alert system
│   │   │   ├── rule_engine.cpp
│   │   │   ├── notification_sender.cpp
│   │   │   ├── threshold_monitor.cpp
│   │   │   └── escalation_manager.cpp
│   │   └── 📁 tracing/           # Distributed tracing
│   │       ├── span_collector.cpp
│   │       ├── trace_aggregator.cpp
│   │       ├── jaeger_exporter.cpp
│   │       └── zipkin_exporter.cpp
│   └── 📁 utils/                 # Utility implementations
│       ├── time_series.cpp       # Time series implementation
│       ├── aggregation.cpp       # Data aggregation logic
│       ├── sampling.cpp          # Sampling implementation
│       └── statistics.cpp        # Statistical calculations
├── 📁 tests/                     # Comprehensive test suite
│   ├── 📁 unit/                  # Unit tests
│   │   ├── core_tests/           # Core functionality tests
│   │   ├── collector_tests/      # Collector component tests
│   │   ├── exporter_tests/       # Exporter component tests
│   │   ├── storage_tests/        # Storage component tests
│   │   └── alerting_tests/       # Alerting system tests
│   ├── 📁 integration/           # Integration tests
│   │   ├── ecosystem_tests/      # Cross-system integration
│   │   ├── end_to_end_tests/     # Complete workflow tests
│   │   ├── dashboard_tests/      # Web dashboard tests
│   │   └── performance_tests/    # Performance integration
│   └── 📁 benchmarks/            # Performance benchmarks
│       ├── collection_bench/     # Data collection benchmarks
│       ├── storage_bench/        # Storage performance
│       ├── query_bench/          # Query performance
│       └── dashboard_bench/      # Dashboard performance
├── 📁 examples/                  # Usage examples & demos
│   ├── 📁 basic/                 # Basic monitoring examples
│   ├── 📁 advanced/              # Advanced configuration examples
│   ├── 📁 integration/           # System integration examples
│   └── 📁 dashboard/             # Web dashboard examples
├── 📁 docs/                      # Comprehensive documentation
│   ├── 📁 api/                   # API documentation
│   ├── 📁 guides/                # User guides & tutorials
│   ├── 📁 architecture/          # Architecture documentation
│   ├── 📁 deployment/            # Deployment guides
│   └── 📁 performance/           # Performance guides & benchmarks
├── 📁 scripts/                   # Build & utility scripts
│   ├── build.sh                  # Build automation
│   ├── test.sh                   # Test execution
│   ├── benchmark.sh              # Performance testing
│   └── deploy.sh                 # Deployment automation
├── 📁 web/                       # Web dashboard assets
│   ├── 📁 public/                # Static web assets
│   ├── 📁 templates/             # HTML templates
│   └── 📁 api/                   # REST API definitions
├── 📄 CMakeLists.txt             # Build configuration
├── 📄 .clang-format              # Code formatting rules
└── 📄 README.md                  # Project overview & documentation
```

## 🏗️ Namespace 구조

### Core Namespaces
- **Root**: `kcenon::monitoring` - 메인 모니터링 네임스페이스
- **Core functionality**: `kcenon::monitoring::core` - 필수 모니터링 컴포넌트
- **Interfaces**: `kcenon::monitoring::interfaces` - 추상 기본 클래스
- **Collectors**: `kcenon::monitoring::collectors` - 데이터 수집 구현
- **Exporters**: `kcenon::monitoring::exporters` - 데이터 내보내기 구현
- **Storage**: `kcenon::monitoring::storage` - 데이터 저장소 구현
- **Implementation details**: `kcenon::monitoring::impl` - 내부 구현 클래스
- **Utilities**: `kcenon::monitoring::utils` - 헬퍼 함수 및 유틸리티

### Nested Namespaces
- `kcenon::monitoring::impl::web` - Web dashboard 컴포넌트
- `kcenon::monitoring::impl::alerting` - Alert 시스템 컴포넌트
- `kcenon::monitoring::impl::tracing` - Distributed tracing 컴포넌트

## 🔧 주요 컴포넌트 개요

### 🎯 Public API Layer (`include/kcenon/monitoring/`)
| 컴포넌트 | 파일 | 목적 |
|-----------|------|---------|
| **Main Monitor** | `core/monitor.h` | 주요 모니터링 인터페이스 |
| **Metrics Manager** | `core/metrics_manager.h` | 메트릭 수집 조정 |
| **Alert Manager** | `core/alert_manager.h` | 알림 관리 및 에스컬레이션 |
| **Dashboard** | `core/dashboard.h` | Web dashboard 인터페이스 |
| **Collector Interface** | `interfaces/collector_interface.h` | 모든 collector의 기본 클래스 |
| **Exporter Interface** | `interfaces/exporter_interface.h` | 모든 exporter의 기본 클래스 |
| **System Collector** | `collectors/system_collector.h` | 시스템 메트릭 수집 |
| **Process Collector** | `collectors/process_collector.h` | 프로세스 메트릭 수집 |
| **Prometheus Exporter** | `exporters/prometheus_exporter.h` | Prometheus 형식 내보내기 |
| **InfluxDB Exporter** | `exporters/influxdb_exporter.h` | InfluxDB 내보내기 |

### ⚙️ Implementation Layer (`src/`)
| 컴포넌트 | 디렉토리 | 목적 |
|-----------|-----------|---------|
| **Collectors** | `impl/collectors/` | 실시간 메트릭 수집 엔진 |
| **Exporters** | `impl/exporters/` | 데이터 내보내기 형식 구현 |
| **Storage Engines** | `impl/storage/` | 시계열 데이터 저장 시스템 |
| **Web Dashboard** | `impl/web/` | 대화형 모니터링 대시보드 |
| **Alert System** | `impl/alerting/` | 규칙 기반 알림 및 알림 |
| **Distributed Tracing** | `impl/tracing/` | 요청 추적 및 span 수집 |

## 📊 성능 특성

- **Collection Rate**: 초당 10M+ 작업 (메트릭 수집)
- **Storage Efficiency**: 시계열 데이터에 대해 90%+ 압축
- **Dashboard Latency**: 100ms 미만의 쿼리 응답 시간
- **Alerting**: 1초 미만의 알림 지연으로 실시간 규칙 평가
- **Distributed Tracing**: 마이크로초 정밀도의 end-to-end 요청 추적

## 🚀 핵심 기능

### 📈 실시간 모니터링
- **System Metrics**: CPU, 메모리, 디스크, 네트워크 사용량
- **Application Metrics**: 사용자 정의 counter, gauge, histogram
- **Performance Metrics**: 응답 시간, 처리량, 오류율
- **Health Monitoring**: 서비스 가용성 및 endpoint 상태

### 📊 Web Dashboard
- **Interactive Visualization**: 실시간 차트 및 그래프
- **Custom Dashboards**: 구성 가능한 모니터링 뷰
- **Alert Management**: 시각적 알림 상태 및 이력
- **API Endpoints**: 메트릭 쿼리를 위한 RESTful API

### 🚨 Alerting System
- **Rule-Based Alerts**: 임계값 기반 및 패턴 기반 규칙
- **Multi-Channel Notifications**: Email, Slack, webhook 통합
- **Escalation Policies**: 자동 에스컬레이션 및 온콜 순환
- **Alert Correlation**: 지능형 그룹화 및 중복 제거

### 🔍 Distributed Tracing
- **Request Tracking**: End-to-end 요청 흐름 시각화
- **Performance Analysis**: 지연 핫스팟 식별
- **Error Tracking**: 분산 오류 전파 분석
- **Service Dependency**: 자동 서비스 맵 생성

## 🔄 마이그레이션 가이드

### Step 1: 현재 설정 백업
```bash
# 이전 구조 자동 백업
mkdir -p old_structure/
cp -r include/ old_structure/include_backup/
cp -r src/ old_structure/src_backup/
cp -r web/ old_structure/web_backup/
```

### Step 2: Include 경로 업데이트
```cpp
// 이전 방식
#include "monitoring/monitor.h"

// 새로운 방식
#include "kcenon/monitoring/core/monitor.h"
```

### Step 3: Namespace 사용 업데이트
```cpp
// 이전 방식
using namespace monitoring;

// 새로운 방식
using namespace kcenon::monitoring::core;
```

### Step 4: 마이그레이션 스크립트 실행
```bash
# 자동화된 namespace 마이그레이션
./scripts/migrate_namespaces.sh
./scripts/update_cmake.sh
./scripts/deploy_dashboard.sh
```

## 🚀 새로운 구조로 빠른 시작

```cpp
#include "kcenon/monitoring/core/monitor.h"
#include "kcenon/monitoring/collectors/system_collector.h"
#include "kcenon/monitoring/exporters/prometheus_exporter.h"

int main() {
    using namespace kcenon::monitoring;

    // 새로운 구조로 모니터링 시스템 생성
    auto monitor = core::monitor_builder()
        .add_collector(std::make_shared<collectors::system_collector>())
        .add_exporter(std::make_shared<exporters::prometheus_exporter>(8080))
        .enable_dashboard(true)
        .enable_alerting(true)
        .build();

    // http://localhost:8080에서 web dashboard와 함께 모니터링 시작
    monitor->start();
    monitor->collect_metrics();

    return 0;
}
```

## 🌐 Web Dashboard 통합

모니터링 시스템은 `http://localhost:8080`에서 액세스할 수 있는 포괄적인 web dashboard를 포함하며 다음 기능을 제공합니다:

- **실시간 메트릭 시각화**
- **대화형 차트 및 그래프**
- **알림 관리 인터페이스**
- **시스템 상태 개요**
- **성능 분석**
- **Distributed Tracing 뷰**

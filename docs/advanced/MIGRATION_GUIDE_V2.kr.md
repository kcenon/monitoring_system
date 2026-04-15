---
doc_id: "MON-MIGR-002"
doc_title: "마이그레이션 가이드: 인터페이스 기반 아키텍처"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "monitoring_system"
category: "MIGR"
---

# 마이그레이션 가이드: 인터페이스 기반 아키텍처

> **SSOT**: This document is the single source of truth for **마이그레이션 가이드: 인터페이스 기반 아키텍처**.

> **Language:** [English](MIGRATION_GUIDE_V2.md) | **한국어**

**대상 독자**: logger_system 및/또는 monitoring_system을 사용하는 개발자
**예상 마이그레이션 시간**: 일반적인 프로젝트의 경우 1-2시간
**호환성을 깨는 변경**: 최소 (인터페이스 기반)

---

## 목차

1. [개요](#개요)
2. [빠른 마이그레이션 경로](#빠른-마이그레이션-경로)
3. [상세한 변경사항](#상세한-변경사항)
4. [일반적인 시나리오](#일반적인-시나리오)
5. [문제 해결](#문제-해결)
6. [FAQ](#faq)

---

## 개요

### 무엇이 변경되었나요?

새로운 아키텍처는 다음을 통해 logger_system과 monitoring_system 간의 순환 의존성을 제거합니다:

1. **인터페이스 표준화** - `common_system`에서
2. **구체적인 의존성 제거** - 시스템 간
3. **의존성 주입 도입** - 런타임 통합을 위해

### 왜 마이그레이션해야 하나요?

✅ **순환 의존성 없음** - 깨끗한 컴파일 타임 의존성 그래프
✅ **더 나은 테스트 가능성** - 쉬운 모의 및 테스트 컴포넌트
✅ **유연성** - 모든 ILogger/IMonitor 구현을 혼합하여 일치
✅ **미래 지향적** - 기존 코드 수정 없이 확장 가능

---

## 빠른 마이그레이션 경로

### logger_system 사용자의 경우

**이전**:
```cpp
#include <kcenon/logger/core/logger.h>

auto logger = logger_builder().build();
```

**이후** - 동일한 코드 작동!:
```cpp
#include <kcenon/logger/core/logger.h>

auto logger = logger_builder().build();

// 선택사항: 모니터링 추가
auto monitor = std::make_shared<performance_monitor>();
logger->set_monitor(monitor);
```

**영향**: ✅ **기본 로거 사용을 위한 호환성을 깨는 변경 없음**

---

### monitoring_system 사용자의 경우

**이전**:
```cpp
#include <kcenon/monitoring/monitoring.h>

auto monitor = std::make_shared<performance_monitor>();
monitor->record_metric("test", 42.0);
```

**이후** - 동일한 코드 작동!:
```cpp
#include <kcenon/monitoring/monitoring.h>

auto monitor = std::make_shared<performance_monitor>();
monitor->record_metric("test", 42.0);

// 선택사항: 로깅 추가
auto logger = /* 모든 ILogger 구현 */;
// 로거를 사용하도록 어댑터 구성
```

**영향**: ✅ **기본 모니터링 사용을 위한 호환성을 깨는 변경 없음**

---

### 두 시스템 사용자의 경우

**이전** - 순환 의존성 발생!:
```cpp
// 이렇게 하지 마세요 - 순환 의존성!
#include <kcenon/logger/core/logger.h>
#include <kcenon/monitoring/core/performance_monitor.h>

auto logger = logger_builder().build();
auto monitor = std::make_shared<performance_monitor>();
// 하드 코딩된 구체적인 의존성
```

**이후** - 깨끗한 의존성 주입:
```cpp
#include <kcenon/logger/core/logger_builder.h>
#include <kcenon/monitoring/core/performance_monitor.h>

// 두 시스템 생성
auto logger_result = logger_builder().build();
auto logger = std::move(logger_result.value());

auto monitor = std::make_shared<performance_monitor>();

// 의존성 주입 (순환 의존성 없음!)
logger->set_monitor(monitor);  // Logger가 IMonitor 인터페이스 사용
// 어댑터가 ILogger 인터페이스 사용 가능

// 두 시스템이 원활하게 함께 작동
logger->log(log_level::info, "System started");
monitor->record_metric("startup_time", 125.5);
```

**영향**: ✅ **명시적인 의존성 주입으로 더 깨끗한 코드**

---

### Result API 업그레이드 체크리스트
1. `if (!result)` / `result.get_error()` 패턴은 `result.is_err()` / `result.error()`로 교체합니다.
2. `result_void::error(...)` 호출을 `Result<void>::err(...)`로 대체합니다.
3. 오류를 다른 시스템으로 전달할 때는 `common::error_info`를 그대로 전달하여 `code`와 `module` 정보를 유지합니다.
4. 코드 수정 시 README·튜토리얼 등 예제 문서도 동시에 갱신해 레거시 API가 남지 않도록 합니다.

---

## 상세한 변경사항

### 1. 인터페이스 위치 변경

#### logger_system

**이전**:
```cpp
#include <kcenon/logger/core/monitoring/monitoring_interface.h>
using kcenon::logger::monitoring::monitoring_interface;
```

**새로운**:
```cpp
#include <kcenon/common/interfaces/monitoring_interface.h>
using common::interfaces::IMonitor;
```

**마이그레이션 작업**:
```bash
# 자동화된 마이그레이션 스크립트
sed -i 's|kcenon/logger/core/monitoring/monitoring_interface.h|kcenon/common/interfaces/monitoring_interface.h|g' *.cpp *.h
sed -i 's|kcenon::logger::monitoring::monitoring_interface|common::interfaces::IMonitor|g' *.cpp *.h
```

---

#### monitoring_system

**이전**:
```cpp
// logger_system에 대한 직접 의존성
#include <kcenon/logger/core/logger.h>
auto logger = std::make_shared<kcenon::logger::logger>();
```

**새로운**:
```cpp
// 대신 인터페이스 사용
#include <kcenon/common/interfaces/logger_interface.h>
using common::interfaces::ILogger;

// 모든 ILogger 구현 작동
std::shared_ptr<ILogger> logger = /* DI를 통해 주입 */;
```

---

### 2. 모니터링 인터페이스 업데이트

#### 이전
```cpp
class my_monitor : public monitoring_interface {
    monitoring_data get_monitoring_data() const override;
    bool is_healthy() const override;
    health_status get_health_status() const override;
};
```

#### 이후
```cpp
class my_monitor : public common::interfaces::IMonitor {
    common::VoidResult record_metric(const std::string& name, double value) override;
    common::Result<metrics_snapshot> get_metrics() override;
    common::Result<health_check_result> check_health() override;
    common::VoidResult reset() override;
};
```

**주요 차이점**:
- 반환 타입이 더 나은 오류 처리를 위해 `Result<T>` 패턴 사용
- 모든 시스템에서 표준화된 메서드 이름
- 메타데이터가 있는 더 풍부한 헬스 체크 결과

---

### 3. DI를 통한 Logger 통합

#### 이전 (v1.x) - 컴파일 타임 의존성
```cpp
// 순환 의존성 생성
#include <kcenon/logger/core/logger.h>  // 구체적인 클래스!

void setup_monitoring() {
    auto logger = std::make_shared<kcenon::logger::logger>();
    // 하드 코딩된 의존성
}
```

#### 이후 (v2.0) - 런타임 의존성 주입
```cpp
// 인터페이스 사용, 구체적인 의존성 없음
#include <kcenon/common/interfaces/logger_interface.h>

void setup_monitoring(std::shared_ptr<common::interfaces::ILogger> logger) {
    // 로거 주입, 모든 ILogger 구현과 작동
    auto adapter = std::make_shared<logger_system_adapter>(bus, logger);
}
```

---

## 일반적인 시나리오

### 시나리오 1: 독립 실행형 Logger (모니터링 없음)

**코드**:
```cpp
#include <kcenon/logger/core/logger_builder.h>

auto logger_result = logger_builder()
    .with_async(true)
    .with_min_level(log_level::info)
    .build();

if (!logger_result) {
    // 오류 처리
    return;
}

auto logger = std::move(logger_result.value());
logger->log(log_level::info, "Application started");
```

**마이그레이션**: ✅ 변경 불필요

---

### 시나리오 2: 독립 실행형 Monitor (Logger 없음)

**코드**:
```cpp
#include <kcenon/monitoring/core/performance_monitor.h>

auto monitor = std::make_shared<performance_monitor>();
monitor->record_metric("cpu_usage", 45.5);

auto health = monitor->check_health();
if (std::holds_alternative<health_check_result>(health)) {
    auto& result = std::get<health_check_result>(health);
    // 헬스 결과 처리
}
```

**마이그레이션**: ✅ 변경 불필요

---

### 시나리오 3: 모니터링이 있는 Logger

**이전**:
```cpp
auto logger = logger_builder()
    .with_monitoring_enabled(true)  // 이전 API
    .build();
```

**이후**:
```cpp
auto monitor = std::make_shared<performance_monitor>();
auto logger = logger_builder()
    .with_monitoring(monitor)  // 새로운: 명시적 DI
    .build()
    .value();
```

**왜 더 나은가?**:
- 명시적 의존성
- 모든 IMonitor 구현 사용 가능
- 모의로 더 쉽게 테스트

---

### 시나리오 4: 로깅이 있는 Monitor

**이전**:
```cpp
// 모니터링 시스템에 logger_system에 대한 하드 코딩된 의존성 있음
auto monitor = create_monitor_with_logging();  // 어떻게?
```

**이후**:
```cpp
// 로거 생성 (모든 ILogger 구현)
auto logger = logger_builder().build().value();

// 모니터 생성
auto monitor = std::make_shared<performance_monitor>();

// 통합을 위해 어댑터 사용
auto bus = std::make_shared<event_bus>();
auto adapter = std::make_shared<logger_system_adapter>(bus, logger);

// 어댑터가 로거 메트릭 수집
auto metrics = adapter->collect_metrics();
```

**왜 더 나은가?**:
- logger_system에 대한 컴파일 타임 의존성 없음
- 모든 ILogger와 작동 (logger_system의 logger만이 아님)
- 모의 로거로 테스트 가능

---

### 시나리오 5: 양방향 통합

**이후** - 최적의 지점!:
```cpp
// 두 시스템 생성
auto logger = logger_builder().build().value();
auto monitor = std::make_shared<performance_monitor>();

// 양방향 주입 (순환 의존성 없음!)
logger->set_monitor(monitor);  // Logger가 모니터에 보고

auto bus = std::make_shared<event_bus>();
auto adapter = std::make_shared<logger_system_adapter>(bus, logger);
// 어댑터가 로거 메트릭 수집 가능

// 두 시스템 사용
for (int i = 0; i < 100; ++i) {
    logger->log(log_level::info, "Request " + std::to_string(i));
    monitor->record_metric("requests", i + 1);
}

// 결합된 헬스 확인
auto logger_health = logger->health_check();
auto monitor_health = monitor->check_health();
```

---

## 문제 해결

### 문제 1: 컴파일 오류 - "monitoring_interface not found"

**오류**:
```
error: 'monitoring_interface' does not name a type
```

**원인**: 이전 include 경로

**수정**:
```cpp
// 이전 include 제거
// #include <kcenon/logger/core/monitoring/monitoring_interface.h>

// 새 include 추가
#include <kcenon/common/interfaces/monitoring_interface.h>
using common::interfaces::IMonitor;
```

---

### 문제 2: 링커 오류 - "undefined reference to logger"

**오류**:
```
undefined reference to `kcenon::logger::logger::logger()'
```

**원인**: monitoring_system에서 구체적인 로거 클래스 사용 시도

**수정**:
```cpp
// monitoring_system에서 이렇게 하지 마세요
// #include <kcenon/logger/core/logger.h>
// auto logger = std::make_shared<kcenon::logger::logger>();

// 대신 이렇게 - 인터페이스 + DI 사용
#include <kcenon/common/interfaces/logger_interface.h>
void setup(std::shared_ptr<common::interfaces::ILogger> logger) {
    // 외부에서 로거 주입
}
```

---

### 문제 3: 타입 불일치 - "cannot convert Result to result"

**오류**:
```
cannot convert 'common::Result<T>' to 'monitoring_system::result<T>'
```

**원인**: 이전 및 새로운 result 타입 혼합 사용

**수정**: `common::Result<T>` 일관되게 사용:
```cpp
#include <kcenon/common/patterns/result.h>

common::Result<metrics_snapshot> get_metrics() {
    // 구현
}
```

---

### 문제 4: 사용 중단 경고

**경고**:
```
warning: 'monitoring_interface' is deprecated
```

**원인**: 레거시 타입 별칭 사용

**수정**:
```cpp
// 이전 (사용 중단이지만 작동)
using monitoring_interface = common::interfaces::IMonitor;

// 새로운 (권장)
using common::interfaces::IMonitor;
```

---

## FAQ

### Q1: 즉시 마이그레이션해야 하나요?

**A**: 아니요. 기존 코드는 사용 중단 경고와 함께 계속 작동합니다. 그러나 다음의 경우 마이그레이션을 권장합니다:
- 새 프로젝트
- 테스트 개선이 필요한 코드
- 유연성이 필요한 시스템

---

### Q2: 이전 코드와 새 코드를 혼합할 수 있나요?

**A**: 예, 하지만 권장하지 않습니다. 전환 헤더가 호환성을 제공합니다:
```cpp
// 여전히 작동 (사용 중단)
#include <kcenon/logger/core/monitoring/monitoring_interface_transition.h>
```

---

### Q3: 로거와 모니터가 둘 다 필요하지 않다면?

**A**: 완벽합니다! 그것이 주요 이점입니다. 이제 각 시스템이 진정으로 독립적입니다:
```cpp
// 로거만
auto logger = logger_builder().build().value();

// 모니터만
auto monitor = std::make_shared<performance_monitor>();
```

---

### Q4: 새 디자인으로 어떻게 테스트하나요?

**A**: 훨씬 쉽습니다! 모의 구현을 생성하세요:
```cpp
class mock_logger : public common::interfaces::ILogger {
    // 테스트를 위한 최소 구현
};

class mock_monitor : public common::interfaces::IMonitor {
    // 테스트를 위한 최소 구현
};

// 테스트에서 모의 사용
auto mock_log = std::make_shared<mock_logger>();
system_under_test->set_logger(mock_log);
```

---

### Q5: 성능은 어떻습니까?

**A**: 무시할 수 있는 영향:
- 인터페이스 호출은 가상 함수와 동일한 비용 (이미 이전에 사용됨)
- DI는 시작 시 한 번 발생
- 런타임 성능: < 5% 오버헤드
- 벤치마크 결과: <!-- TODO: PHASE3_VERIFICATION_REPORT.md 없음; BENCHMARKS.md 참조 --> [BENCHMARKS.md](../BENCHMARKS.md) 참조

---

### Q6: 점진적으로 마이그레이션할 수 있나요?

**A**: 예! 권장 접근법:
1. v2.0 인터페이스를 사용하여 새 코드 시작
2. 기존 코드를 변경하지 않고 그대로 둠 (사용 중단 경고와 함께 작동)
3. 모듈을 터치할 때 점진적으로 업데이트
4. 준비되면 자동화된 마이그레이션 스크립트 실행

---

### Q7: 어디에서 예시를 찾을 수 있나요?

**A**: 다음 파일을 확인하세요:
- `examples/bidirectional_di_example.cpp` - 전체 DI 데모
- `tests/test_cross_system_integration.cpp` - 통합 테스트
- `tests/test_adapter_functionality.cpp` - 어댑터 테스트

---

## 자동화된 마이그레이션 스크립트

```bash
#!/bin/bash
# migrate_to_interfaces.sh - 자동화된 마이그레이션 도우미

echo "인터페이스 기반 아키텍처로 코드베이스 마이그레이션 중..."

# 1. include 경로 업데이트
find . -type f \( -name "*.cpp" -o -name "*.h" \) -exec sed -i \
    's|kcenon/logger/core/monitoring/monitoring_interface\.h|kcenon/common/interfaces/monitoring_interface.h|g' {} \;

# 2. 네임스페이스 업데이트
find . -type f \( -name "*.cpp" -o -name "*.h" \) -exec sed -i \
    's|kcenon::logger::monitoring::monitoring_interface|common::interfaces::IMonitor|g' {} \;

find . -type f \( -name "*.cpp" -o -name "*.h" \) -exec sed -i \
    's|kcenon::logger::monitoring::health_status|common::interfaces::health_status|g' {} \;

# 3. logger_system 타입 업데이트
find . -type f \( -name "*.cpp" -o -name "*.h" \) -exec sed -i \
    's|logger_system::result|common::Result|g' {} \;

# 4. monitoring_system 타입 업데이트
find . -type f \( -name "*.cpp" -o -name "*.h" \) -exec sed -i \
    's|monitoring_system::result_void|common::VoidResult|g' {} \;

echo "✓ 마이그레이션 완료!"
echo ""
echo "다음 단계:"
echo "1. 변경사항 검토: git diff"
echo "2. 프로젝트 빌드: cmake --build build"
echo "3. 테스트 실행: ctest --test-dir build"
echo "4. 남은 문제는 수동으로 수정"
```

---

## 요약

✅ **주요 요점**:
1. 기본 사용법은 동일하게 유지
2. 고급 사용법은 DI를 통해 유연성 획득
3. 순환 의존성 없음
4. 더 나은 테스트 가능성
5. 미래 지향적 아키텍처

📚 **추가 리소스**:
- <!-- TODO: PHASE3_VERIFICATION_REPORT.md 없음; 링크 제거됨 -->
- [Bidirectional DI Example](../../examples/) <!-- TODO: bidirectional_di_example.cpp 없음; examples/ 참조 -->
- [API Documentation](https://docs.example.com)

🆘 **도움이 필요하신가요?**:
- GitHub Issues: https://github.com/kcenon/monitoring_system/issues
- GitHub Issues: https://github.com/kcenon/logger_system/issues

---

**마지막 업데이트**: 2025-10-02

---

*Last Updated: 2025-10-20*

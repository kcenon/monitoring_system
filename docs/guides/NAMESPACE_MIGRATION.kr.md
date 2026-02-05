# 네임스페이스 마이그레이션 가이드

> **버전:** 1.0.0
> **최종 업데이트:** 2025-02-05
> **상태:** 마이그레이션 완료 (v1.0)

이 가이드는 레거시 `monitoring_system` 네임스페이스에서 kcenon 에코시스템 명명 규칙에 맞는 표준 `kcenon::monitoring` 네임스페이스로의 전환을 설명합니다.

---

## 목차

1. [개요](#개요)
2. [현재 vs 레거시](#현재-vs-레거시)
3. [타임라인](#타임라인)
4. [마이그레이션 단계](#마이그레이션-단계)
5. [호환성 레이어](#호환성-레이어)
6. [마이그레이션 체크리스트](#마이그레이션-체크리스트)
7. [자주 묻는 질문](#자주-묻는-질문)

---

## 개요

monitoring_system은 `monitoring_system` 네임스페이스에서 `kcenon::monitoring`으로 전환되었습니다. 이는 다른 kcenon 프로젝트에서 사용하는 에코시스템 규칙과 일치시키기 위함입니다:

| 프로젝트 | 네임스페이스 |
|---------|-----------|
| common_system | `kcenon::common` |
| thread_system | `kcenon::thread` |
| logger_system | `kcenon::logger` |
| network_system | `kcenon::network` |
| container_system | `kcenon::container` |
| **monitoring_system** | **`kcenon::monitoring`** |

---

## 현재 vs 레거시

| 버전 | 네임스페이스 | 상태 |
|---------|-----------|--------|
| v1.x (현재) | `kcenon::monitoring` | **표준** (권장) |
| v1.x (현재) | `monitoring_system` | 별칭 (deprecated) |
| v2.0 (계획) | `kcenon::monitoring` | 유일하게 지원되는 네임스페이스 |

---

## 타임라인

| 단계 | 버전 | 상태 | 설명 |
|-------|---------|--------|-------------|
| 1단계 | v0.x | 완료 | 레거시 네임스페이스 (`monitoring_system`) |
| 2단계 | v1.0 | **현재** | 새 네임스페이스 (`kcenon::monitoring`) 및 호환성 별칭 제공 |
| 3단계 | v1.x | 예정 | 레거시 네임스페이스에 대한 deprecation 경고 도입 |
| 4단계 | v2.0 | 예정 | 레거시 네임스페이스 별칭 제거 |

---

## 마이그레이션 단계

### 이전 (레거시 코드)

```cpp
#include <kcenon/monitoring/core/performance_monitor.h>

// 레거시 네임스페이스 (deprecated)
using namespace monitoring_system;

int main() {
    performance_monitor monitor("my_service");

    monitoring_system::central_collector collector;
    collector.start();

    return 0;
}
```

### 이후 (최신 코드)

```cpp
#include <kcenon/monitoring/core/performance_monitor.h>

// 표준 네임스페이스 (권장)
using namespace kcenon::monitoring;

int main() {
    performance_monitor monitor("my_service");

    kcenon::monitoring::central_collector collector;
    collector.start();

    return 0;
}
```

---

## 호환성 레이어

전환 기간(v1.x) 동안 `<kcenon/monitoring/compatibility.h>`를 통해 호환성 레이어가 제공됩니다:

```cpp
// 하위 호환성 네임스페이스 별칭
// 레거시 코드는 monitoring_system::을 사용할 수 있으며, 이는 kcenon::monitoring::에 매핑됩니다
namespace monitoring_system = ::kcenon::monitoring;

// 추가 레거시 별칭
namespace monitoring_module = ::kcenon::monitoring;
```

### 호환성 레이어 사용하기

마이그레이션 중 기존 코드와 새 코드를 모두 지원해야 하는 경우:

```cpp
#include <kcenon/monitoring/compatibility.h>  // 별칭 포함
#include <kcenon/monitoring/core/performance_monitor.h>

// v1.x에서 둘 다 동작:
kcenon::monitoring::performance_monitor monitor1("service1");  // 권장
monitoring_system::performance_monitor monitor2("service2");    // Deprecated
```

**참고:** 대부분의 monitoring_system 헤더에서 호환성 헤더가 자동으로 포함되지만, 명시적으로 포함하면 마이그레이션 의도를 명확히 할 수 있습니다.

---

## 마이그레이션 체크리스트

코드베이스를 마이그레이션할 때 이 체크리스트를 사용하세요:

### 코드 변경

- [ ] `using namespace monitoring_system`을 `using namespace kcenon::monitoring`으로 변경
- [ ] 명시적 `monitoring_system::` 접두사를 `kcenon::monitoring::`으로 변경
- [ ] `monitoring_module::` 접두사를 `kcenon::monitoring::`으로 변경 (해당하는 경우)
- [ ] typedef 또는 별칭 선언 업데이트

### 빌드 시스템 변경

- [ ] CMake 타겟 이름 업데이트 (일반적으로 변경 없음)
- [ ] pkg-config 참조 업데이트 (해당하는 경우)

### 검색 및 바꾸기 패턴

| 찾기 | 바꾸기 |
|------|---------|
| `using namespace monitoring_system` | `using namespace kcenon::monitoring` |
| `monitoring_system::` | `kcenon::monitoring::` |
| `monitoring_module::` | `kcenon::monitoring::` |

### 검증

- [ ] 빌드가 에러 없이 성공
- [ ] 모든 테스트 통과
- [ ] deprecation 경고 없음 (활성화된 경우)

---

## 자주 묻는 질문

### Q: 즉시 마이그레이션해야 하나요?

**A:** 아니요. 호환성 레이어가 v1.x 전체에서 레거시 코드가 계속 동작하도록 보장합니다. 그러나 v2.0 출시 시 문제를 피하기 위해 마이그레이션을 권장합니다.

### Q: `monitoring_system` 네임스페이스가 동작을 멈추나요?

**A:** v2.0에서 `monitoring_system`과 `monitoring_module` 네임스페이스 별칭이 제거됩니다. 그때까지는 `kcenon::monitoring`의 별칭으로 동작합니다.

### Q: deprecation 경고를 활성화하려면 어떻게 하나요?

**A:** Deprecation 경고는 향후 v1.x 릴리스에서 도입될 예정입니다. 업데이트는 CHANGELOG를 확인하세요.

### Q: 같은 코드베이스에 monitoring_system과 kcenon::monitoring이 둘 다 있으면 어떻게 되나요?

**A:** `monitoring_system`은 `kcenon::monitoring`의 별칭이므로 동일한 타입을 참조합니다. 마이그레이션 중 혼용할 수 있지만, 코드 명확성을 위해 권장하지 않습니다.

### Q: 헤더 경로도 변경되나요?

**A:** 아니요. 헤더 경로는 `<kcenon/monitoring/...>`으로 유지됩니다. 네임스페이스만 영향을 받습니다.

---

## 관련 문서

- [빠른 시작 가이드](QUICK_START.kr.md) - monitoring_system 시작하기
- [API 레퍼런스](../API_REFERENCE.kr.md) - 전체 API 문서
- [변경 이력](../CHANGELOG.kr.md) - 버전 기록 및 마이그레이션 노트

---

*마이그레이션 관련 질문이나 문제가 있으면 [GitHub 저장소](https://github.com/kcenon/monitoring_system/issues)에 이슈를 등록해 주세요.*

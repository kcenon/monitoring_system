# Monitoring System에 기여하기

> **Language:** [English](CONTRIBUTING.md) | **한국어**

Monitoring System에 대한 기여를 환영합니다! 이 문서는 프로젝트에 기여하기 위한 가이드라인을 제공합니다.

## 목차

- [시작하기](#시작하기)
- [개발 환경 설정](#개발-환경-설정)
- [변경사항 작성](#변경사항-작성)
- [코딩 표준](#코딩-표준)
- [테스트](#테스트)
- [변경사항 제출](#변경사항-제출)
- [코드 리뷰 프로세스](#코드-리뷰-프로세스)
- [커뮤니티 가이드라인](#커뮤니티-가이드라인)

## 시작하기

### 필수 요구사항

- C++20 지원 컴파일러 (GCC 11+, Clang 14+, MSVC 2019+)
- CMake 3.16 이상
- Git
- 모니터링 시스템 및 관찰 가능성에 대한 기본 이해

### Fork 및 Clone

1. GitHub에서 저장소 Fork
2. 로컬에 Fork Clone:

```bash
git clone https://github.com/yourusername/monitoring_system.git
cd monitoring_system
```

## 개발 환경 설정

### 빌드 환경

```bash
# 빌드 디렉토리 생성
mkdir build && cd build

# 개발 옵션으로 구성
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTS=ON \
  -DBUILD_BENCHMARKS=ON \
  -DENABLE_COVERAGE=ON

# 빌드
cmake --build . --parallel $(nproc)
```

### 테스트 실행

```bash
# 모든 테스트 실행
ctest --output-on-failure

# 특정 테스트 카테고리 실행
./tests/monitoring_system_tests --gtest_filter="MetricsTest.*"
./tests/monitoring_system_tests --gtest_filter="AlertingTest.*"
./tests/monitoring_system_tests --gtest_filter="DashboardTest.*"

# 벤치마크 실행
./benchmarks/monitoring_benchmarks

# 스트레스 테스트 실행
./tests/stress_tests --duration=60 --threads=4
```

## 변경사항 작성

### 브랜치 전략

- `main`에서 feature 브랜치 생성
- 설명적인 브랜치 이름 사용: `feature/add-metrics-aggregation`, `fix/memory-leak-collector`
- 브랜치는 단일 기능이나 수정에 집중

```bash
git checkout -b feature/your-feature-name
```

### 커밋 메시지

관례적인 커밋 형식을 따르세요:

```
type(scope): description

body (optional)

footer (optional)
```

**타입:**
- `feat`: 새 기능
- `fix`: 버그 수정
- `docs`: 문서 변경
- `style`: 포맷 변경
- `refactor`: 코드 리팩토링
- `test`: 테스트 추가/수정
- `perf`: 성능 개선
- `build`: 빌드 시스템 변경

**예시:**
```
feat(metrics): add histogram metric type support

Add support for histogram metrics with configurable buckets
and automatic percentile calculation.

Closes #123
```

```
fix(alerting): resolve memory leak in alert processor

Fix memory leak caused by unreleased alert rule objects
in the alert processing pipeline.

Fixes #456
```

## 코딩 표준

### C++ 가이드라인

- 최신 C++20 표준 준수
- RAII 원칙 일관되게 사용
- 힙 할당보다 스택 할당 선호
- 힙 할당이 필요한 경우 스마트 포인터 사용
- 비소유 참조를 제외하고 raw 포인터 사용 지양

### 코드 스타일

- 들여쓰기는 공백 4칸 사용 (탭 금지)
- 최대 줄 길이: 100자
- 변수와 함수는 snake_case 사용
- 클래스와 타입은 PascalCase 사용
- 상수와 매크로는 UPPER_CASE 사용

### 명명 규칙

```cpp
// 클래스와 타입
class MetricsCollector;
using MetricValue = double;
enum class AlertSeverity;

// 함수와 변수
void collect_metrics();
auto metric_value = get_current_value();
const size_t buffer_size = 1024;

// 상수와 매크로
constexpr int MAX_METRICS = 10000;
#define MONITORING_VERSION_MAJOR 2
```

### 문서화

- 공개 API에 Doxygen 스타일 주석 사용
- API 문서에 사용 예시 포함
- 복잡한 알고리즘 및 설계 결정 문서화
- 주석을 코드 변경과 함께 최신 상태로 유지

```cpp
/**
 * @brief 등록된 수집기로부터 메트릭을 수집합니다
 *
 * 이 함수는 등록된 모든 메트릭 수집기를 순회하고
 * 값을 단일 메트릭 스냅샷으로 집계합니다.
 *
 * @param timestamp 메트릭 스냅샷의 타임스탬프
 * @return 수집된 모든 메트릭을 포함하는 MetricsSnapshot
 *
 * @example
 * auto snapshot = metrics_manager.collect_metrics(
 *     std::chrono::system_clock::now());
 */
MetricsSnapshot collect_metrics(TimePoint timestamp);
```

## 테스트

### 테스트 카테고리

1. **단위 테스트**: 개별 컴포넌트를 격리해서 테스트
2. **통합 테스트**: 컴포넌트 상호작용 테스트
3. **성능 테스트**: 성능 특성 검증
4. **스트레스 테스트**: 부하 상황에서 시스템 테스트

### 테스트 작성

- Google Test 프레임워크 사용
- 모든 공개 API에 대한 테스트 작성
- 긍정 및 부정 테스트 케이스 모두 포함
- 오류 조건 및 엣지 케이스 테스트
- 높은 테스트 커버리지 유지 (>90%)

```cpp
TEST(MetricsCollectorTest, CollectBasicMetrics) {
    MetricsCollector collector;
    collector.register_metric("cpu_usage", MetricType::Gauge);

    auto snapshot = collector.collect();

    EXPECT_FALSE(snapshot.empty());
    EXPECT_TRUE(snapshot.contains("cpu_usage"));
}

TEST(MetricsCollectorTest, HandlesInvalidMetricName) {
    MetricsCollector collector;

    EXPECT_THROW(
        collector.register_metric("", MetricType::Counter),
        std::invalid_argument
    );
}
```

### 성능 고려사항

- 성능에 중요한 코드 경로 프로파일링
- 벤치마크를 사용하여 성능 저하 방지
- 메모리 할당 패턴 고려
- 일반적인 사용 사례 최적화

## 변경사항 제출

### 제출 전 체크리스트

- [ ] 경고 없이 코드 컴파일
- [ ] 모든 테스트 통과
- [ ] 새 코드가 테스트로 커버됨
- [ ] 문서 업데이트
- [ ] 코드가 스타일 가이드라인 준수
- [ ] 커밋 메시지가 적절히 포맷됨

### Pull Request 프로세스

1. **브랜치 업데이트**:
```bash
git checkout main
git pull upstream main
git checkout your-feature-branch
git rebase main
```

2. **변경사항 푸시**:
```bash
git push origin your-feature-branch
```

3. **Pull Request 생성**:
   - 설명적인 제목과 설명 사용
   - 관련 이슈 참조
   - 테스트 지침 포함
   - UI 변경 시 스크린샷 추가

4. **리뷰 피드백 대응**:
   - 모든 코멘트에 응답
   - 요청된 변경사항 적용
   - 필요에 따라 테스트 업데이트

## 코드 리뷰 프로세스

### 리뷰 기준

- **정확성**: 코드가 의도대로 작동하는가?
- **설계**: 코드가 잘 설계되고 유지보수 가능한가?
- **기능성**: 요구사항을 충족하는가?
- **복잡도**: 코드를 이해하기 쉬운가?
- **테스트**: 적절한 테스트가 있는가?
- **명명**: 이름이 명확하고 설명적인가?
- **주석**: 주석이 유용하고 정확한가?
- **스타일**: 스타일 가이드를 따르는가?
- **문서화**: 문서가 적절한가?

### 리뷰 일정

- 초기 응답: 영업일 기준 2일 이내
- 완전한 리뷰: 영업일 기준 5일 이내
- 변경 후 재검토: 영업일 기준 2일 이내

## 커뮤니티 가이드라인

### 행동 강령

- 존중하고 포용적으로 행동
- 신규 참여자를 환영하고 학습 지원
- 건설적인 피드백에 집중
- 긍정적인 의도 가정
- 다양한 관점과 경험 존중

### 커뮤니케이션

- **이슈**: 버그 리포트 및 기능 요청에 GitHub Issues 사용
- **토론**: 질문 및 일반 토론에 GitHub Discussions 사용
- **이메일**: 민감한 사항은 kcenon@naver.com

### 도움 받기

- 기존 문서 및 예제 확인
- 기존 이슈 및 토론 검색
- 구체적이고 상세한 질문하기
- 최소 재현 예제 제공

## 개발 모범 사례

### 성능 가이드라인

- 핫 패스에서 메모리 할당 최소화
- 적절한 경우 lock-free 데이터 구조 사용
- 개별 호출보다 배치 작업 선호
- 자주 액세스하는 데이터 캐시
- 최적화 전 프로파일링

### 오류 처리

- 오류 처리에 result 타입 사용
- 의미 있는 오류 메시지 제공
- 적절한 컨텍스트로 오류 로깅
- 프로그래밍 오류에 대해 빠른 실패
- 런타임 오류 우아하게 처리

### 보안 고려사항

- 모든 입력 데이터 검증
- 보안 코딩 관행 사용
- 버퍼 오버플로우 방지
- 민감한 데이터 적절히 처리
- 최소 권한 원칙 준수

## 인정

기여자는 다음에서 인정됩니다:
- 중요한 기여에 대해 CHANGELOG.md
- README.md 기여자 섹션
- GitHub 기여자 그래프
- 연간 기여자 감사 게시물

Monitoring System에 기여해 주셔서 감사합니다! 여러분의 노력이 모두를 위한 더 나은 관찰 가능성을 만듭니다.

---

*Last Updated: 2025-10-20*

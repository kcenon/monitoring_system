# Monitoring System 문서

> **Language:** [English](README.md) | **한국어**

Monitoring System - C++ 애플리케이션을 위한 프로덕션 준비 모니터링 및 관찰성 플랫폼의 포괄적인 문서에 오신 것을 환영합니다.

## 📚 문서 구조

### 핵심 문서
- **[README.md](../README.md)** - 프로젝트 개요, 기능 및 빠른 시작
- **[API_REFERENCE.md](API_REFERENCE.md)** - 예제가 포함된 완전한 API 문서
- **[ARCHITECTURE_GUIDE.md](ARCHITECTURE_GUIDE.md)** - 시스템 디자인 및 아키텍처 패턴
- **[PHASE3.md](PHASE3.md)** - Phase 3 구현 세부 사항 (알림 및 대시보드)
- **[CHANGELOG.md](CHANGELOG.md)** - 버전 히스토리 및 릴리스 노트
- **[CONTRIBUTING.md](CONTRIBUTING.md)** - 기여 가이드라인 및 개발 설정
- **[SECURITY.md](SECURITY.md)** - 보안 정책 및 취약점 보고

### 운영 가이드
- **[PERFORMANCE_TUNING.md](PERFORMANCE_TUNING.md)** - 성능 최적화 전략
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - 일반적인 문제 및 해결 방법

### 사용자 가이드 (`docs/guides/`)
- **[TUTORIAL.md](guides/TUTORIAL.md)** - 단계별 튜토리얼 및 예제

## 🚀 빠른 탐색

### 신규 사용자
1. 프로젝트 개요를 위해 **[README.md](../README.md)**부터 시작
2. 실습 학습을 위해 **[TUTORIAL.md](guides/TUTORIAL.md)** 따라하기
3. 상세한 API 사용법을 위해 **[API_REFERENCE.md](API_REFERENCE.md)** 참조

### 운영자 및 DevOps
1. 최적화를 위해 **[PERFORMANCE_TUNING.md](PERFORMANCE_TUNING.md)** 검토
2. 문제 해결을 위해 **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** 학습
3. 보안 모범 사례를 위해 **[SECURITY.md](SECURITY.md)** 확인

### 기여자 및 개발자
1. 개발 가이드라인을 위해 **[CONTRIBUTING.md](CONTRIBUTING.md)** 읽기
2. 시스템 디자인을 위해 **[ARCHITECTURE_GUIDE.md](ARCHITECTURE_GUIDE.md)** 학습
3. 최근 변경 사항을 위해 **[CHANGELOG.md](CHANGELOG.md)** 검토

### Phase 3 사용자 (알림 및 대시보드)
1. 알림 시스템 세부 사항을 위해 **[PHASE3.md](PHASE3.md)** 학습
2. 알림 API를 위해 **[API_REFERENCE.md](API_REFERENCE.md)** 참조
3. 알림 관련 문제를 위해 **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** 확인

## 🏗️ 시스템 개요

Monitoring System은 다음을 특징으로 하는 포괄적인 관찰성 플랫폼입니다:

### 핵심 컴포넌트
- **메트릭 수집**: 다양한 수집기 플러그인이 있는 고성능 메트릭 수집
- **시계열 저장소**: 압축 및 보존 정책이 있는 최적화된 저장소
- **분산 추적**: 완전한 분산 추적 상관 관계 및 분석
- **상태 모니터링**: 컴포넌트 및 의존성 상태 추적

### Phase 3 기능 (최신)
- **실시간 알림**: 다중 채널 알림이 있는 규칙 기반 알림 엔진
- **웹 대시보드**: 실시간 시각화가 있는 대화형 웹 인터페이스
- **알림 관리**: 포괄적인 알림 생명주기 관리
- **성능 모니터링**: 고급 성능 메트릭 및 분석

## 📖 문서 구성

### 사용자 유형별

#### **최종 사용자** (모니터링 시스템 사용)
```
README.md → TUTORIAL.md → API_REFERENCE.md
```

#### **시스템 관리자** (배포 및 유지 관리)
```
README.md → ARCHITECTURE_GUIDE.md → PERFORMANCE_TUNING.md → TROUBLESHOOTING.md → SECURITY.md
```

#### **개발자** (프로젝트에 기여)
```
README.md → ARCHITECTURE_GUIDE.md → CONTRIBUTING.md → API_REFERENCE.md → CHANGELOG.md
```

#### **알림 및 대시보드 사용자** (Phase 3 기능)
```
README.md → PHASE3.md → API_REFERENCE.md → TROUBLESHOOTING.md
```

### 기능별

#### **시작하기**
- 프로젝트 설정 및 기본 사용법
- 실용적인 예제가 포함된 튜토리얼
- 빠른 시작 구성

#### **아키텍처 및 디자인**
- 시스템 아키텍처 및 디자인 패턴
- 컴포넌트 상호작용 및 데이터 흐름
- 통합 지점 및 확장성

#### **운영**
- 성능 튜닝 및 최적화
- 문제 해결 및 문제 해결
- 보안 구성 및 모범 사례

#### **개발**
- 기여 가이드라인 및 코딩 표준
- API 참조 및 예제
- 변경 이력 및 마이그레이션 가이드

## 🔧 문서 유지 관리

이 문서 구조는 다음을 위해 설계되었습니다:
- **정보 중앙화**: 하나의 조직화된 위치에 모든 문서 배치
- **다양한 사용자 유형 지원**: 다양한 필요에 대한 명확한 탐색 경로
- **일관성 유지**: 표준화된 형식 및 상호 참조
- **검색 가능**: 논리적 구성 및 포괄적인 색인

### 최근 개선 사항
- 모든 문서를 docs 폴더로 통합
- 표준 프로젝트 문서 추가 (CONTRIBUTING, CHANGELOG, SECURITY)
- 역할 기반 진입점으로 탐색 개선
- 문서 간 상호 참조 향상

## 📝 문서 형식

- **Markdown**: 모든 문서는 GitHub Flavored Markdown 사용
- **코드 예제**: 언어 지정이 있는 구문 강조 코드 블록
- **다이어그램**: 아키텍처 및 흐름 설명을 위한 ASCII 아트 다이어그램
- **상호 참조**: 쉬운 탐색을 위한 상대 링크

## 🤝 문서에 기여하기

문서 개선을 환영합니다! 다음 사항은 [CONTRIBUTING.md](CONTRIBUTING.md)를 참조하세요:

- 문서 스타일 가이드라인
- 문서 변경 사항에 대한 검토 프로세스
- API 문서 업데이트 방법
- 문서 변경 사항 테스트

### 문서 표준
- 명확하고 간결한 작성
- 실용적인 예제 및 코드 스니펫
- 최신 정보
- 적절한 상호 참조
- 일관된 형식

## 🔗 외부 리소스

- **[GitHub Repository](https://github.com/kcenon/monitoring_system)** - 소스 코드 및 이슈
- **[GitHub Discussions](https://github.com/kcenon/monitoring_system/discussions)** - 유지보수 지원
- **[GitHub Actions](https://github.com/kcenon/monitoring_system/actions)** - 지속적 통합
- **[Releases](https://github.com/kcenon/monitoring_system/releases)** - 버전 다운로드

## 🆘 도움 받기

- **Issues**: [GitHub Issues](https://github.com/kcenon/monitoring_system/issues)에서 버그 보고 및 기능 요청
- **Questions**: [GitHub Discussions](https://github.com/kcenon/monitoring_system/discussions)에서 질문하기
- **Security**: [SECURITY.md](SECURITY.md) 가이드라인에 따라 보안 문제 보고
- **Email**: kcenon@naver.com으로 유지 관리자에게 문의

---

*문서 마지막 업데이트: September 2024 - Monitoring System v3.0*

*가장 최신 정보는 항상 이 문서의 최신 버전을 참조하세요.*

---

*Last Updated: 2025-10-20*

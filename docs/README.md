# Monitoring System Documentation

> **Language:** **English** | [한국어](README_KO.md)

**Version:** 0.1.0
**Last Updated:** 2025-11-11
**Status:** Comprehensive

Welcome to the monitoring_system documentation! This observability platform provides metrics collection, distributed tracing, real-time alerting, and interactive dashboards for C++20 applications.

---

## 🚀 Quick Navigation

| I want to... | Document |
|--------------|----------|
| ⚡ Get started in 5 minutes | [Quick Start](guides/QUICK_START.md) |
| 🏗️ Understand the architecture | [Architecture](01-ARCHITECTURE.md) |
| 📖 Look up an API | [API Reference](02-API_REFERENCE.md) |
| ❓ Find answers to common questions | [FAQ](guides/FAQ.md) (25+ Q&A) |
| 🐛 Troubleshoot an issue | [Troubleshooting](guides/TROUBLESHOOTING.md) |
| ✨ Learn best practices | [Best Practices](guides/BEST_PRACTICES.md) |
| 📊 Review performance benchmarks | [Baseline](performance/BASELINE.md) |
| 🤝 Contribute to the project | [Contributing](contributing/CONTRIBUTING.md) |

---

## Table of Contents

- [Documentation Structure](#documentation-structure)
- [Documentation by Role](#documentation-by-role)
- [By Feature](#by-feature)
- [Contributing to Documentation](#contributing-to-documentation)

---

## Documentation Structure

### 📘 Core Documentation

Essential documents for understanding the system:

| Document | Description | Korean | Lines |
|----------|-------------|--------|-------|
| [01-ARCHITECTURE.md](01-ARCHITECTURE.md) | System architecture, event bus, integration topology | [🇰🇷](01-ARCHITECTURE_KO.md) | 50+ |
| [02-API_REFERENCE.md](02-API_REFERENCE.md) | Complete API docs: metrics, tracing, alerting, dashboard | [🇰🇷](02-API_REFERENCE_KO.md) | 1000+ |

### 📗 User Guides

Step-by-step guides for users:

| Document | Description | Korean | Lines |
|----------|-------------|--------|-------|
| [QUICK_START.md](guides/QUICK_START.md) | 5-minute getting started guide | - | 708 |
| [FAQ.md](guides/FAQ.md) | 25 frequently asked questions with examples | - | 991 |
| [TROUBLESHOOTING.md](guides/TROUBLESHOOTING.md) | Common problems and solutions | [🇰🇷](guides/TROUBLESHOOTING_KO.md) | 600+ |
| [BEST_PRACTICES.md](guides/BEST_PRACTICES.md) | Production patterns for metrics, alerting, tracing | - | 1190 |
| [TUTORIAL.md](guides/TUTORIAL.md) | Step-by-step tutorial with examples | [🇰🇷](guides/TUTORIAL_KO.md) | 784 |
| [SECURITY.md](guides/SECURITY.md) | Security policy and vulnerability reporting | [🇰🇷](guides/SECURITY_KO.md) | 200+ |

### 📙 Advanced Topics

For experienced users and contributors:

| Document | Description | Korean | Lines |
|----------|-------------|--------|-------|
| [ARCHITECTURE_GUIDE.md](advanced/ARCHITECTURE_GUIDE.md) | Detailed system design and patterns | [🇰🇷](advanced/ARCHITECTURE_GUIDE_KO.md) | 800+ |
| [ARCHITECTURE_ISSUES.md](advanced/ARCHITECTURE_ISSUES.md) | Known architectural issues | [🇰🇷](advanced/ARCHITECTURE_ISSUES_KO.md) | 200+ |
| [CURRENT_STATE.md](advanced/CURRENT_STATE.md) | Current implementation status | [🇰🇷](advanced/CURRENT_STATE_KO.md) | 150+ |
| [MIGRATION_GUIDE_V2.md](advanced/MIGRATION_GUIDE_V2.md) | Migration guide to version 2 | [🇰🇷](advanced/MIGRATION_GUIDE_V2_KO.md) | 300+ |
| [INTERFACE_SEPARATION_STRATEGY.md](advanced/INTERFACE_SEPARATION_STRATEGY.md) | Interface design strategy | - | 200+ |
| [THREAD_LOCAL_COLLECTOR_DESIGN.md](advanced/THREAD_LOCAL_COLLECTOR_DESIGN.md) | Thread-local collection design | - | 150+ |
| [PROFILING_GUIDE.md](advanced/PROFILING_GUIDE.md) | Performance profiling guide | - | 200+ |

### 📊 Performance

Performance metrics and optimization:

| Document | Description | Korean | Lines |
|----------|-------------|--------|-------|
| [BASELINE.md](performance/BASELINE.md) | Performance baseline: 80ns record, 5M ops/s | [🇰🇷](performance/BASELINE_KO.md) | 300+ |
| [PERFORMANCE_BASELINE.md](performance/PERFORMANCE_BASELINE.md) | Detailed performance metrics | - | 200+ |
| [PERFORMANCE_TUNING.md](performance/PERFORMANCE_TUNING.md) | Performance tuning strategies | [🇰🇷](performance/PERFORMANCE_TUNING_KO.md) | 400+ |
| [SANITIZER_BASELINE.md](performance/SANITIZER_BASELINE.md) | Sanitizer results (TSan, ASan, UBSan) | [🇰🇷](performance/SANITIZER_BASELINE_KO.md) | 150+ |
| [STATIC_ANALYSIS_BASELINE.md](performance/STATIC_ANALYSIS_BASELINE.md) | Static analysis results (Clang-Tidy, Cppcheck) | [🇰🇷](performance/STATIC_ANALYSIS_BASELINE_KO.md) | 100+ |
| [SPRINT_2_PERFORMANCE_RESULTS.md](performance/SPRINT_2_PERFORMANCE_RESULTS.md) | Sprint 2 performance achievements | - | 100+ |

### 🤝 Contributing

For contributors and maintainers:

| Document | Description | Korean | Lines |
|----------|-------------|--------|-------|
| [CONTRIBUTING.md](contributing/CONTRIBUTING.md) | Contribution guidelines, code style, testing | [🇰🇷](contributing/CONTRIBUTING_KO.md) | 600+ |
| [CI_CD_GUIDE.md](contributing/CI_CD_GUIDE.md) | CI/CD pipeline, sanitizers, benchmarks | - | 954 |
| [TESTING_GUIDE.md](contributing/TESTING_GUIDE.md) | Testing strategy and procedures | - | 400+ |

---

## Documentation by Role

### 👤 For New Users

**Getting Started Path**:
1. **⚡ Quick Start** - [5-minute guide](guides/QUICK_START.md) to first program
2. **🏗️ Architecture** - [System overview](01-ARCHITECTURE.md)
3. **📖 API Reference** - [Complete API](02-API_REFERENCE.md) documentation
4. **💡 Tutorial** - [Step-by-step guide](guides/TUTORIAL.md) with examples

**When You Have Issues**:
- Check [FAQ](guides/FAQ.md) first (25+ common questions)
- Use [Troubleshooting](guides/TROUBLESHOOTING.md) for problems
- Search [GitHub Issues](https://github.com/kcenon/monitoring_system/issues)

### 💻 For Experienced Developers

**Advanced Usage Path**:
1. **🏗️ Architecture** - Understand [system design](advanced/ARCHITECTURE_GUIDE.md)
2. **📖 API Reference** - Study [advanced APIs](02-API_REFERENCE.md)
3. **✨ Best Practices** - Learn [production patterns](guides/BEST_PRACTICES.md)
4. **📊 Performance** - Review [benchmarks](performance/BASELINE.md)

**Deep Dive Topics**:
- [Thread-Local Collection](advanced/THREAD_LOCAL_COLLECTOR_DESIGN.md) - Performance optimization
- [Interface Separation](advanced/INTERFACE_SEPARATION_STRATEGY.md) - Design patterns
- [Profiling Guide](advanced/PROFILING_GUIDE.md) - Performance analysis
- [Architecture Issues](advanced/ARCHITECTURE_ISSUES.md) - Known limitations

### 🔧 For DevOps Engineers

**Deployment Path**:
1. **📚 Quick Start** - [Installation and setup](guides/QUICK_START.md)
2. **📊 Performance Tuning** - [Optimization strategies](performance/PERFORMANCE_TUNING.md)
3. **✨ Best Practices** - [Production deployment](guides/BEST_PRACTICES.md#production-deployment)
4. **🐛 Troubleshooting** - [Common issues](guides/TROUBLESHOOTING.md)

**Monitoring and Tuning**:
- [Performance Baseline](performance/BASELINE.md) - 80ns record latency, 5M ops/s
- [Metrics Performance](performance/PERFORMANCE_BASELINE.md) - Backend-specific metrics
- [CI/CD Pipeline](contributing/CI_CD_GUIDE.md) - Automation

### 🤝 For Contributors

**Contribution Path**:
1. **🤝 Contributing** - [How to contribute](contributing/CONTRIBUTING.md)
2. **🚀 CI/CD** - [Pipeline documentation](contributing/CI_CD_GUIDE.md)
3. **🏗️ Architecture** - [System internals](advanced/ARCHITECTURE_GUIDE.md)
4. **📊 Current State** - [Implementation status](advanced/CURRENT_STATE.md)

**Development Resources**:
- [Code Style](contributing/CONTRIBUTING.md#coding-standards)
- [Testing Guide](contributing/TESTING_GUIDE.md)
- [Current Status](advanced/CURRENT_STATE.md) - What's implemented

---

## By Feature

### 📊 Metrics Collection

| Topic | Document | Section |
|-------|----------|---------|
| API | [API Reference](02-API_REFERENCE.md) | Metrics Collection |
| Best Practices | [Best Practices](guides/BEST_PRACTICES.md) | Metrics Design |
| Performance | [Baseline](performance/BASELINE.md) | 80ns record latency |
| Examples | [FAQ](guides/FAQ.md) | Metrics Collection |

### 🔍 Distributed Tracing

| Topic | Document | Section |
|-------|----------|---------|
| API | [API Reference](02-API_REFERENCE.md) | Distributed Tracer |
| Best Practices | [Best Practices](guides/BEST_PRACTICES.md) | Distributed Tracing |
| Architecture | [Architecture Guide](advanced/ARCHITECTURE_GUIDE.md) | Tracing System |
| Examples | [Quick Start](guides/QUICK_START.md) | Tracing |

### 🚨 Alerting System

| Topic | Document | Section |
|-------|----------|---------|
| API | [API Reference](02-API_REFERENCE.md) | Alerting Engine |
| Best Practices | [Best Practices](guides/BEST_PRACTICES.md) | Alert Design |
| FAQ | [FAQ](guides/FAQ.md) | Alerting |
| Examples | [Quick Start](guides/QUICK_START.md) | Alerting |

### 📈 Web Dashboard

| Topic | Document | Section |
|-------|----------|---------|
| Setup | [Quick Start](guides/QUICK_START.md) | Web Dashboard |
| API | [API Reference](02-API_REFERENCE.md) | Dashboard API |
| Security | [Security](guides/SECURITY.md) | Dashboard Access |
| Troubleshooting | [Troubleshooting](guides/TROUBLESHOOTING.md) | Dashboard Issues |

### 🗄️ Storage

| Topic | Document | Section |
|-------|----------|---------|
| Configuration | [Best Practices](guides/BEST_PRACTICES.md) | Storage Configuration |
| Performance | [Performance Tuning](performance/PERFORMANCE_TUNING.md) | Storage Optimization |
| FAQ | [FAQ](guides/FAQ.md) | Storage |
| Architecture | [Architecture Guide](advanced/ARCHITECTURE_GUIDE.md) | Storage Backend |

### 🔗 Integration

| Topic | Document | Section |
|-------|----------|---------|
| thread_system | [FAQ](guides/FAQ.md) | Thread System Integration |
| logger_system | [FAQ](guides/FAQ.md) | Logger System Integration |
| Prometheus | [FAQ](guides/FAQ.md) | Export to Prometheus |
| OpenTelemetry | [FAQ](guides/FAQ.md) | OpenTelemetry Integration |

---

## Project Information

### Current Status
- **Version**: 3.0 (Phase 3 with Alerting & Dashboard)
- **C++ Standard**: C++20
- **License**: BSD 3-Clause
- **Test Status**: Under Development

### Supported Features
- ✅ **Metrics Collection** - Counter, Gauge, Histogram, Summary
- ✅ **Distributed Tracing** - Full trace correlation and analysis
- ✅ **Real-time Alerting** - Rule-based alert engine
- ✅ **Multi-channel Notifications** - Email, Slack, PagerDuty, Webhook
- ✅ **Web Dashboard** - Interactive visualization
- ✅ **Storage Backends** - In-memory, File-based, Custom
- ✅ **Exporters** - Prometheus, OpenTelemetry, Jaeger

### Key Features
- 📊 **High Performance** - 80ns record latency, 5M ops/s throughput
- 🎯 **Low Overhead** - <1% CPU, minimal memory footprint
- 🔗 **Unified Observability** - Metrics, traces, and alerts in one system
- 🚨 **Smart Alerting** - Multi-level severity, grouping, inhibition
- 📈 **Real-time Dashboard** - Interactive web UI with live updates
- 🔧 **Flexible Storage** - Multiple backends with retention policies
- 🧵 **Thread Safe** - Concurrent operations verified with TSan
- 🔐 **Production Ready** - Security, authentication, RBAC

---

## Contributing to Documentation

### Documentation Standards
Follow the [Documentation Standard](/Users/raphaelshin/Sources/template_document/DOCUMENTATION_STANDARD.md):
- Front matter on all documents
- Code examples must compile
- Bilingual support (English/Korean)
- Cross-references with relative links

### Areas for Improvement
- [ ] Video tutorials for alerting and dashboard
- [ ] Interactive examples for metric design
- [ ] More integration scenarios
- [ ] Performance optimization cookbook

### Submission Process
1. Read [Contributing Guide](contributing/CONTRIBUTING.md)
2. Edit markdown files
3. Test all code examples
4. Update Korean translations
5. Submit pull request

---

## 📞 Getting Help

### Documentation Issues
- **Missing info**: [Open documentation issue](https://github.com/kcenon/monitoring_system/issues/new?labels=documentation)
- **Incorrect examples**: Report with details
- **Unclear instructions**: Suggest improvements

### Technical Support
1. Check [FAQ](guides/FAQ.md) - 25+ common questions
2. Read [Troubleshooting](guides/TROUBLESHOOTING.md) - Solutions to common problems
3. Search [GitHub Issues](https://github.com/kcenon/monitoring_system/issues)
4. Ask on [GitHub Discussions](https://github.com/kcenon/monitoring_system/discussions)

### Support Resources
- **Issues**: Bug reports and feature requests
- **Discussions**: Questions and support
- **Pull Requests**: Code and documentation contributions

---

## External Resources

- **GitHub Repository**: [kcenon/monitoring_system](https://github.com/kcenon/monitoring_system)
- **Issue Tracker**: [GitHub Issues](https://github.com/kcenon/monitoring_system/issues)
- **Main README**: [../README.md](../README.md)
- **Changelog**: [CHANGELOG.md.bak](CHANGELOG.md.bak)

---

## Documentation Roadmap

### ✅ Current (v1.0 - 2025-11-11)
- ✅ Complete API reference with examples
- ✅ Comprehensive FAQ (25+ questions)
- ✅ Detailed troubleshooting guide
- ✅ Best practices documentation
- ✅ Performance benchmarks
- ✅ CI/CD documentation
- ✅ Quick start guide
- ✅ Tutorial with examples

### 📋 Future Enhancements
- 🎥 Video tutorials for alerting and dashboard
- 📊 Interactive performance dashboard
- 🌐 Multi-language support (Japanese, Chinese)
- 📖 Migration guides for major versions
- 🔄 Integration guides for more systems

---

**Monitoring System Documentation** - Modern Observability for C++20

**Last Updated**: 2025-11-11
**Next Review**: 2026-02-11

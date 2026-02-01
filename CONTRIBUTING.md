# Contributing to Monitoring System

Thank you for considering contributing to the Monitoring System! This document provides guidelines and instructions for contributors.

## Table of Contents

- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Plugin Development](#plugin-development)
- [Code Standards](#code-standards)
- [Testing](#testing)
- [Pull Requests](#pull-requests)

## Getting Started

### Prerequisites

- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.20 or higher
- Git

### Building from Source

```bash
git clone https://github.com/kcenon/monitoring_system.git
cd monitoring_system
mkdir build
cd build
cmake ..
cmake --build .
```

### Running Tests

```bash
cd build
ctest --output-on-failure
```

## Development Workflow

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/your-feature`)
3. Make your changes
4. Run tests and ensure they pass
5. Commit your changes (see commit message guidelines below)
6. Push to your fork (`git push origin feature/your-feature`)
7. Open a Pull Request

### Commit Message Guidelines

Follow the [Conventional Commits](https://www.conventionalcommits.org/) format:

```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `refactor`: Code refactoring
- `test`: Adding or modifying tests
- `perf`: Performance improvement
- `chore`: Maintenance tasks

**Examples:**
```
feat(collectors): add GPU temperature collector
fix(network): resolve connection timeout on slow networks
docs(plugin): update plugin development guide
```

## Plugin Development

The Monitoring System supports extensible collector plugins. You can contribute new collectors or enhance existing ones.

### Creating a New Plugin

See the [Plugin Development Guide](docs/plugin_development_guide.md) for comprehensive instructions on creating collector plugins.

#### Quick Start

1. Implement the `collector_plugin` interface:

```cpp
#include "kcenon/monitoring/plugins/collector_plugin.h"

class my_collector : public kcenon::monitoring::collector_plugin {
public:
    auto name() const -> std::string_view override {
        return "my_collector";
    }

    auto collect() -> std::vector<metric> override {
        // Your collection logic
    }

    auto interval() const -> std::chrono::milliseconds override {
        return std::chrono::seconds(5);
    }

    auto is_available() const -> bool override {
        // Platform/resource availability check
    }

    auto get_metric_types() const -> std::vector<std::string> override {
        return {"my_metric_type"};
    }
};
```

2. Add tests for your plugin
3. Update documentation
4. Submit a Pull Request

#### Plugin Requirements

- **Thread-safe**: Plugins may be called from multiple threads
- **Fast collection**: `collect()` should complete in < 100ms
- **Error handling**: Return empty vector on failure, don't crash
- **Platform checks**: Use `is_available()` for platform-specific code
- **Documentation**: Document what metrics are collected and why

#### Examples

- [Minimal Plugin](docs/examples/minimal_plugin.cpp) - Simplest possible plugin
- [Advanced Plugin](docs/examples/advanced_plugin.cpp) - Best practices example
- [Dynamic Plugin](examples/plugin_example/) - Shared library plugin

### Plugin Categories

When contributing a plugin, categorize it appropriately:

| Category | Use For |
|----------|---------|
| `system` | System integration (threads, loggers, containers) |
| `hardware` | Hardware sensors (GPU, temperature, battery) |
| `platform` | Platform-specific (VM detection, uptime) |
| `network` | Network metrics (connectivity, bandwidth) |
| `process` | Process-level (CPU, memory, I/O) |
| `custom` | Other/experimental plugins |

### Built-in vs Dynamic Plugins

**Built-in Plugins** (statically linked):
- Core functionality
- Cross-platform collectors
- High-performance requirements
- Submitted to main repository

**Dynamic Plugins** (shared libraries):
- Platform-specific collectors
- Third-party integrations
- Experimental features
- Custom/proprietary collectors

See [Dynamic Plugin Loading](examples/plugin_example/README.md) for details.

## Code Standards

### C++ Style

- Use C++20 features when beneficial
- Follow existing code style (clang-format configuration provided)
- Prefer RAII for resource management
- Use `auto` for obvious types
- Avoid raw pointers; use smart pointers
- Prefer standard library over custom implementations

### Naming Conventions

- **Classes/Structs**: `snake_case`
- **Functions/Methods**: `snake_case`
- **Variables**: `snake_case`
- **Constants**: `UPPER_SNAKE_CASE`
- **Template Parameters**: `PascalCase`

### File Organization

```
include/kcenon/monitoring/
├── plugins/           # Plugin interfaces
├── collectors/        # Built-in collectors
├── interfaces/        # Adapter interfaces
└── ...

src/
├── plugins/           # Plugin implementation
├── collectors/        # Collector implementations
└── ...

tests/
├── plugins/           # Plugin tests
└── ...
```

### Header Guards

Use `#pragma once`:

```cpp
#pragma once

// Header content
```

### Documentation

Use Doxygen-style comments:

```cpp
/**
 * @brief Brief description
 * @param param Parameter description
 * @return Return value description
 */
auto function(int param) -> int;
```

## Testing

### Test Requirements

All code contributions must include tests:

- **Unit tests**: Test individual components
- **Integration tests**: Test component interactions
- **Platform tests**: Test platform-specific code paths

### Writing Tests

Use Google Test framework:

```cpp
#include <gtest/gtest.h>
#include "my_plugin.h"

TEST(MyPluginTest, BasicFunctionality) {
    my_plugin plugin;
    EXPECT_TRUE(plugin.is_available());

    auto metrics = plugin.collect();
    EXPECT_FALSE(metrics.empty());
}
```

### Running Specific Tests

```bash
cd build
ctest -R MyPluginTest -VV
```

### Test Coverage

Aim for:
- New code: > 80% coverage
- Critical paths: 100% coverage
- Error handling: Test failure scenarios

## Pull Requests

### PR Checklist

Before submitting a PR, ensure:

- [ ] Code compiles without warnings
- [ ] All tests pass
- [ ] New tests added for new functionality
- [ ] Documentation updated
- [ ] Code follows project style
- [ ] Commit messages follow conventional format
- [ ] No merge conflicts with main branch

### PR Description Template

```markdown
## Summary
Brief description of changes

## Related Issues
Closes #issue_number

## Changes Made
- Change 1
- Change 2

## Testing Done
- Test scenario 1
- Test scenario 2

## Documentation
- [ ] Updated relevant documentation
- [ ] Added code comments
- [ ] Updated CHANGELOG.md (if applicable)
```

### Review Process

1. Automated checks run (build, tests, linting)
2. Maintainer reviews code
3. Address review comments
4. Approved PR is merged

### Performance Considerations

For performance-critical code:
- Provide benchmarks
- Compare before/after performance
- Document performance characteristics

## Getting Help

- Check [existing issues](https://github.com/kcenon/monitoring_system/issues)
- Read the [Plugin Development Guide](docs/plugin_development_guide.md)
- Open a [discussion](https://github.com/kcenon/monitoring_system/discussions) for questions

## License

By contributing, you agree that your contributions will be licensed under the BSD 3-Clause License.

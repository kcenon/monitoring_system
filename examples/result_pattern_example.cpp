// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <iostream>
#include <kcenon/monitoring/core/result_types.h>
#include <kcenon/monitoring/core/error_codes.h>
#include <kcenon/monitoring/interfaces/monitoring_core.h>

using namespace kcenon::monitoring;

/**
 * @brief Example demonstrating Result pattern usage
 */

// Example function that may fail
kcenon::common::Result<double> divide(double a, double b) {
    if (b == 0) {
        return kcenon::common::Result<double>::err(error_info(monitoring_error_code::invalid_configuration, "Division by zero").to_common_error());
    }
    return kcenon::common::ok(a / b);
}

// Example function using kcenon::common::VoidResult
kcenon::common::VoidResult validate_range(double value, double min, double max) {
    if (value < min || value > max) {
        error_info err(monitoring_error_code::invalid_configuration,
                      "Value out of range [" + std::to_string(min) + ", " + std::to_string(max) + "]");
        return kcenon::common::VoidResult::err(err.to_common_error());
    }
    return kcenon::common::ok();
}

// Example using monadic operations
kcenon::common::Result<std::string> process_metric(double value) {
    // Chain operations using map and and_then
    return divide(100.0, value)
        .map([](double x) { return x * 2; })
        .and_then([](double x) {
            if (x > 50) {
                return kcenon::common::ok("High value: " + std::to_string(x));
            }
            return kcenon::common::ok("Normal value: " + std::to_string(x));
        });
}

int main() {
    std::cout << "=== Result Pattern Example ===" << std::endl << std::endl;

    // Example 1: Successful operation
    std::cout << "Example 1: Successful division" << std::endl;
    auto result1 = divide(10.0, 2.0);
    if (result1.is_ok()) {
        std::cout << "  Result: " << result1.value() << std::endl;
    } else {
        std::cout << "  Error: " << result1.error().message << std::endl;
    }
    std::cout << std::endl;

    // Example 2: Failed operation
    std::cout << "Example 2: Division by zero" << std::endl;
    auto result2 = divide(10.0, 0.0);
    if (result2.is_ok()) {
        std::cout << "  Result: " << result2.value() << std::endl;
    } else {
        std::cout << "  Error: " << result2.error().message << std::endl;
    }
    std::cout << std::endl;

    // Example 3: Using value_or
    std::cout << "Example 3: Using value_or with default" << std::endl;
    auto result3 = divide(5.0, 0.0);
    double value = result3.value_or(-1.0);
    std::cout << "  Value (with default): " << value << std::endl;
    std::cout << std::endl;

    // Example 4: kcenon::common::VoidResult usage
    std::cout << "Example 4: Validation with kcenon::common::VoidResult" << std::endl;
    auto validation1 = validate_range(50.0, 0.0, 100.0);
    if (validation1.is_ok()) {
        std::cout << "  Validation passed" << std::endl;
    } else {
        std::cout << "  Validation failed: " << validation1.error().message << std::endl;
    }

    auto validation2 = validate_range(150.0, 0.0, 100.0);
    if (validation2.is_ok()) {
        std::cout << "  Validation passed" << std::endl;
    } else {
        std::cout << "  Validation failed: " << validation2.error().message << std::endl;
    }
    std::cout << std::endl;

    // Example 5: Monadic operations
    std::cout << "Example 5: Chaining operations" << std::endl;
    auto result4 = process_metric(4.0);
    if (result4.is_ok()) {
        std::cout << "  " << result4.value() << std::endl;
    } else {
        std::cout << "  Error: " << result4.error().message << std::endl;
    }

    auto result5 = process_metric(1.0);
    if (result5.is_ok()) {
        std::cout << "  " << result5.value() << std::endl;
    } else {
        std::cout << "  Error: " << result5.error().message << std::endl;
    }
    std::cout << std::endl;

    // Example 6: Metrics snapshot
    std::cout << "Example 6: Metrics snapshot" << std::endl;
    metrics_snapshot snapshot;
    snapshot.add_metric("cpu_usage", 65.5);
    snapshot.add_metric("memory_usage", 4096.0);
    snapshot.add_metric("disk_io", 150.25);

    std::cout << "  Metrics collected: " << snapshot.metrics.size() << std::endl;

    if (auto cpu = snapshot.get_metric("cpu_usage")) {
        std::cout << "  CPU Usage: " << cpu.value() << "%" << std::endl;
    }

    if (auto mem = snapshot.get_metric("memory_usage")) {
        std::cout << "  Memory Usage: " << mem.value() << " MB" << std::endl;
    }
    std::cout << std::endl;

    // Example 7: Configuration validation
    std::cout << "Example 7: Configuration validation" << std::endl;
    monitoring_config config;
    config.history_size = 1000;
    config.collection_interval = std::chrono::milliseconds(100);
    config.buffer_size = 5000;

    auto config_result = config.validate();
    if (config_result.is_ok()) {
        std::cout << "  Configuration is valid" << std::endl;
        std::cout << "  - History size: " << config.history_size << std::endl;
        std::cout << "  - Collection interval: " << config.collection_interval.count() << "ms" << std::endl;
        std::cout << "  - Buffer size: " << config.buffer_size << std::endl;
    } else {
        std::cout << "  Configuration error: " << config_result.error().message << std::endl;
    }

    return 0;
}

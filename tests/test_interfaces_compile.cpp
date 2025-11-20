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

/**
 * @file test_interfaces_compile.cpp
 * @brief Compilation test for new monitoring interfaces
 *
 * This test ensures that all new interface headers compile correctly
 * and can be included without errors.
 */

#include <kcenon/monitoring/interfaces/observer_interface.h>
#include <kcenon/monitoring/interfaces/metric_collector_interface.h>
#include <kcenon/monitoring/interfaces/event_bus_interface.h>
#include <memory>
#include <iostream>

using namespace kcenon::monitoring;

// Test implementation of observer interface
class test_observer : public interface_monitoring_observer {
public:
    void on_metric_collected(const metric_event& event) override {
        std::cout << "Metric collected from: " << event.source() << std::endl;
    }

    void on_event_occurred(const system_event& event) override {
        std::cout << "System event from: " << event.component() << std::endl;
    }

    void on_system_state_changed(const state_change_event& event) override {
        std::cout << "State change in: " << event.component() << std::endl;
    }
};

// Test that interfaces can be used as base classes
class test_collector : public interface_metric_collector {
public:
    result<std::vector<metric>> collect_metrics() override {
        return result<std::vector<metric>>(std::vector<metric>{});
    }

    result_void start_collection(const collection_config& config) override {
        return make_void_success();
    }

    result_void stop_collection() override {
        return make_void_success();
    }

    bool is_collecting() const override {
        return false;
    }

    std::vector<std::string> get_metric_types() const override {
        return {"test_metric"};
    }

    collection_config get_config() const override {
        return collection_config{};
    }

    result_void update_config(const collection_config& config) override {
        return make_void_success();
    }

    result<std::vector<metric>> force_collect() override {
        return collect_metrics();
    }

    metric_stats get_stats() const override {
        return metric_stats{};
    }

    void reset_stats() override {}

    // Observable interface methods
    result_void register_observer(std::shared_ptr<interface_monitoring_observer> observer) override {
        return make_void_success();
    }

    result_void unregister_observer(std::shared_ptr<interface_monitoring_observer> observer) override {
        return make_void_success();
    }

    void notify_metric(const metric_event& event) override {}
    void notify_event(const system_event& event) override {}
    void notify_state_change(const state_change_event& event) override {}
};

int main() {
    std::cout << "=== Interface Compilation Test ===" << std::endl;

    // Test that interfaces can be instantiated through implementations
    auto observer = std::make_shared<test_observer>();
    auto collector = std::make_shared<test_collector>();

    // Test that interfaces can be used
    metric m{"test", metric_value{42.0}, {}};
    metric_event me("test_source", m);
    observer->on_metric_collected(me);

    system_event se(system_event::event_type::component_started, "test_component", "Started");
    observer->on_event_occurred(se);

    state_change_event sce("test_component",
                          state_change_event::state::healthy,
                          state_change_event::state::degraded);
    observer->on_system_state_changed(sce);

    // Test collector interface
    auto result = collector->collect_metrics();
    if (result.is_ok()) {
        std::cout << "Metrics collected successfully" << std::endl;
    }

    auto types = collector->get_metric_types();
    std::cout << "Collector supports " << types.size() << " metric type(s)" << std::endl;

    std::cout << "✅ All interface compilation tests passed!" << std::endl;

    return 0;
}
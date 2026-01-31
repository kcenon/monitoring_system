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

#pragma once

/**
 * @file collector_adapters.h
 * @brief Adapters to connect existing collectors to collector_interface
 *
 * This file provides adapter templates that wrap existing collector
 * implementations to conform to the collector_interface, enabling
 * them to work with the metric_factory.
 */

#include <memory>
#include <string>
#include <vector>

#include "metric_factory.h"

namespace kcenon::monitoring {

/**
 * @brief Adapter for plugin-based collectors (metric_collector_plugin)
 *
 * Wraps collectors that implement the metric_collector_plugin interface.
 * @tparam T The concrete collector type
 */
template <typename T>
class plugin_collector_adapter : public collector_interface {
   public:
    plugin_collector_adapter() : collector_(std::make_unique<T>()) {}

    bool initialize(const config_map& config) override {
        return collector_->initialize(config);
    }

    [[nodiscard]] std::string get_name() const override { return std::string(collector_->name()); }

    [[nodiscard]] bool is_healthy() const override { return collector_->is_healthy(); }

    [[nodiscard]] std::vector<std::string> get_metric_types() const override {
        return collector_->get_metric_types();
    }

    /**
     * @brief Get the underlying collector
     * @return Pointer to the wrapped collector
     */
    [[nodiscard]] T* get_collector() { return collector_.get(); }
    [[nodiscard]] const T* get_collector() const { return collector_.get(); }

   private:
    std::unique_ptr<T> collector_;
};

/**
 * @brief Adapter for CRTP-based collectors (collector_base<T>)
 *
 * Wraps collectors that derive from collector_base<T>.
 * @tparam T The concrete collector type
 */
template <typename T>
class crtp_collector_adapter : public collector_interface {
   public:
    crtp_collector_adapter() : collector_(std::make_unique<T>()) {}

    bool initialize(const config_map& config) override {
        return collector_->initialize(config);
    }

    [[nodiscard]] std::string get_name() const override { return collector_->get_name(); }

    [[nodiscard]] bool is_healthy() const override { return collector_->is_healthy(); }

    [[nodiscard]] std::vector<std::string> get_metric_types() const override {
        return collector_->get_metric_types();
    }

    /**
     * @brief Get the underlying collector
     * @return Pointer to the wrapped collector
     */
    [[nodiscard]] T* get_collector() { return collector_.get(); }
    [[nodiscard]] const T* get_collector() const { return collector_.get(); }

   private:
    std::unique_ptr<T> collector_;
};

/**
 * @brief Adapter for standalone collectors (like vm_collector)
 *
 * Wraps collectors that have their own interface but follow
 * similar patterns to the standard collector interfaces.
 * @tparam T The concrete collector type
 */
template <typename T>
class standalone_collector_adapter : public collector_interface {
   public:
    standalone_collector_adapter() : collector_(std::make_unique<T>()) {}

    bool initialize(const config_map& config) override {
        return collector_->initialize(config);
    }

    [[nodiscard]] std::string get_name() const override { return collector_->get_name(); }

    [[nodiscard]] bool is_healthy() const override { return collector_->is_healthy(); }

    [[nodiscard]] std::vector<std::string> get_metric_types() const override {
        return collector_->get_metric_types();
    }

    /**
     * @brief Get the underlying collector
     * @return Pointer to the wrapped collector
     */
    [[nodiscard]] T* get_collector() { return collector_.get(); }
    [[nodiscard]] const T* get_collector() const { return collector_.get(); }

   private:
    std::unique_ptr<T> collector_;
};

/**
 * @brief Helper function to register a plugin-based collector
 * @tparam T The collector type
 * @param name The collector name
 * @return true if registration successful
 */
template <typename T>
bool register_plugin_collector(const std::string& name) {
    return metric_factory::instance().register_collector(name, []() {
        return std::make_unique<plugin_collector_adapter<T>>();
    });
}

/**
 * @brief Helper function to register a CRTP-based collector
 * @tparam T The collector type
 * @param name The collector name
 * @return true if registration successful
 */
template <typename T>
bool register_crtp_collector(const std::string& name) {
    return metric_factory::instance().register_collector(name, []() {
        return std::make_unique<crtp_collector_adapter<T>>();
    });
}

/**
 * @brief Helper function to register a standalone collector
 * @tparam T The collector type
 * @param name The collector name
 * @return true if registration successful
 */
template <typename T>
bool register_standalone_collector(const std::string& name) {
    return metric_factory::instance().register_collector(name, []() {
        return std::make_unique<standalone_collector_adapter<T>>();
    });
}

}  // namespace kcenon::monitoring

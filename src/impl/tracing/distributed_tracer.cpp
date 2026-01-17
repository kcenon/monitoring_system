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
 * @file distributed_tracer.cpp
 * @brief Implementation of distributed tracing functionality
 */

#include "distributed_tracer.h"
#include <kcenon/monitoring/exporters/trace_exporters.h>
#include <thread>
#include <algorithm>
#include <shared_mutex>
#include <sstream>

namespace kcenon { namespace monitoring {

/**
 * @brief Private implementation of distributed tracer
 */
struct distributed_tracer::tracer_impl {
    // Thread-local storage for current span
    static thread_local std::shared_ptr<trace_span> current_span;

    // Storage for all spans
    mutable std::shared_mutex spans_mutex;
    std::unordered_map<std::string, std::vector<trace_span>> traces;  // trace_id -> spans

    // Export buffer for finished spans
    std::vector<trace_span> finished_spans_;
    std::mutex export_mutex_;
    size_t export_batch_size_ = 100;

    // Exporter integration
    std::shared_ptr<trace_exporter_interface> exporter_;
    trace_export_settings export_settings_;
    std::atomic<std::size_t> exported_spans_{0};
    std::atomic<std::size_t> failed_exports_{0};
    std::atomic<std::size_t> dropped_spans_{0};

    // Configuration
    std::string default_service_name{"monitoring_system"};
    std::atomic<size_t> max_traces{10000};
    std::atomic<size_t> max_spans_per_trace{1000};
    
    /**
     * @brief Store a span
     */
    common::Result<bool> store_span(const trace_span& span) {
        std::unique_lock lock(spans_mutex);
        
        auto& trace_spans = traces[span.trace_id];
        if (trace_spans.size() >= max_spans_per_trace) {
            return common::Result<bool>::err(error_info(kcenon::monitoring::monitoring_error_code::resource_exhausted, "Error").to_common_error());
        }
        
        trace_spans.push_back(span);
        
        // Cleanup old traces if we have too many
        if (traces.size() > max_traces) {
            // Find oldest trace (simple strategy - in production, use LRU or time-based)
            auto oldest = traces.begin();
            traces.erase(oldest);
        }
        
        return true;
    }
    
    /**
     * @brief Generate unique span ID
     */
    std::string generate_span_id() {
        return thread_context_manager::generate_request_id();
    }
    
    /**
     * @brief Generate unique trace ID
     */
    std::string generate_trace_id() {
        return thread_context_manager::generate_correlation_id();
    }

    /**
     * @brief Export spans to configured exporter
     * @param spans Spans to export
     * @return Result indicating success or failure
     */
    common::VoidResult export_spans_to_exporter(const std::vector<trace_span>& spans) {
        if (!exporter_) {
            // No exporter configured, silently succeed
            return common::ok();
        }

        auto result = exporter_->export_spans(spans);
        if (result.is_ok()) {
            exported_spans_ += spans.size();
        } else {
            failed_exports_++;
        }
        return result;
    }

    /**
     * @brief Check if queue is at capacity and drop spans if necessary
     * @return Number of spans dropped
     */
    std::size_t enforce_queue_limit() {
        if (finished_spans_.size() <= export_settings_.max_queue_size) {
            return 0;
        }

        std::size_t to_drop = finished_spans_.size() - export_settings_.max_queue_size;
        finished_spans_.erase(finished_spans_.begin(),
                              finished_spans_.begin() + static_cast<std::ptrdiff_t>(to_drop));
        dropped_spans_ += to_drop;
        return to_drop;
    }
};

// Define thread-local storage
thread_local std::shared_ptr<trace_span> distributed_tracer::tracer_impl::current_span;

distributed_tracer::distributed_tracer() 
    : impl_(std::make_unique<tracer_impl>()) {
}

distributed_tracer::~distributed_tracer() = default;

distributed_tracer::distributed_tracer(distributed_tracer&&) noexcept = default;
distributed_tracer& distributed_tracer::operator=(distributed_tracer&&) noexcept = default;

common::Result<std::shared_ptr<trace_span>> distributed_tracer::start_span(
    const std::string& operation_name,
    const std::string& service_name) {
    
    auto span = std::make_shared<trace_span>();
    span->trace_id = impl_->generate_trace_id();
    span->span_id = impl_->generate_span_id();
    span->operation_name = operation_name;
    span->service_name = service_name.empty() ? impl_->default_service_name : service_name;
    span->start_time = std::chrono::system_clock::now();
    
    // Add default tags
    span->tags["span.kind"] = "internal";
    span->tags["service.name"] = span->service_name;
    
    // Get thread context if available
    auto ctx = thread_context_manager::get_context();
    if (ctx) {
        // Thread ID from current std::thread
        std::stringstream ss;
        ss << std::this_thread::get_id();
        span->tags["thread.id"] = ss.str();
        if (!ctx->correlation_id.empty()) {
            span->tags["correlation.id"] = ctx->correlation_id;
        }
    }
    
    return span;
}

common::Result<std::shared_ptr<trace_span>> distributed_tracer::start_child_span(
    const trace_span& parent,
    const std::string& operation_name) {
    
    auto span = std::make_shared<trace_span>();
    span->trace_id = parent.trace_id;
    span->span_id = impl_->generate_span_id();
    span->parent_span_id = parent.span_id;
    span->operation_name = operation_name;
    span->service_name = parent.service_name;
    span->start_time = std::chrono::system_clock::now();
    
    // Inherit baggage from parent
    span->baggage = parent.baggage;
    
    // Add default tags
    span->tags["span.kind"] = "internal";
    span->tags["service.name"] = span->service_name;
    span->tags["parent.span.id"] = parent.span_id;
    
    return span;
}

common::Result<std::shared_ptr<trace_span>> distributed_tracer::start_span_from_context(
    const trace_context& context,
    const std::string& operation_name) {
    
    auto span = std::make_shared<trace_span>();
    span->trace_id = context.trace_id;
    span->span_id = impl_->generate_span_id();
    span->parent_span_id = context.span_id;
    span->operation_name = operation_name;
    span->service_name = impl_->default_service_name;
    span->start_time = std::chrono::system_clock::now();
    
    // Copy baggage from context
    span->baggage = context.baggage;
    
    // Add tags
    span->tags["span.kind"] = "server";
    span->tags["service.name"] = span->service_name;
    span->tags["parent.span.id"] = context.span_id;
    
    return span;
}

common::Result<bool> distributed_tracer::finish_span(std::shared_ptr<trace_span> span) {
    if (!span) {
        return common::Result<bool>::err(error_info(kcenon::monitoring::monitoring_error_code::invalid_argument, "Error").to_common_error());
    }

    if (span->is_finished()) {
        return common::Result<bool>::err(error_info(kcenon::monitoring::monitoring_error_code::already_exists, "Error").to_common_error());
    }

    span->end_time = std::chrono::system_clock::now();
    span->calculate_duration();

    // Set status if not already set
    if (span->status == trace_span::status_code::unset) {
        span->status = trace_span::status_code::ok;
    }

    // Store in export buffer
    {
        std::lock_guard<std::mutex> lock(impl_->export_mutex_);
        impl_->finished_spans_.push_back(*span);

        // Enforce queue size limit
        impl_->enforce_queue_limit();

        // Auto-flush when buffer reaches threshold
        if (impl_->export_settings_.export_on_finish &&
            impl_->finished_spans_.size() >= impl_->export_settings_.batch_size) {
            // Export to configured exporter
            std::vector<trace_span> spans_to_export;
            spans_to_export.swap(impl_->finished_spans_);

            auto export_result = impl_->export_spans_to_exporter(spans_to_export);
            if (export_result.is_err()) {
                // On failure, put spans back for retry (up to queue limit)
                impl_->finished_spans_.insert(impl_->finished_spans_.end(),
                                             spans_to_export.begin(),
                                             spans_to_export.end());
                impl_->enforce_queue_limit();
            }
        }
    }

    // Store the span in traces collection
    return impl_->store_span(*span);
}

std::shared_ptr<trace_span> distributed_tracer::get_current_span() const {
    return impl_->current_span;
}

void distributed_tracer::set_current_span(std::shared_ptr<trace_span> span) {
    impl_->current_span = span;
}

trace_context distributed_tracer::extract_context(const trace_span& span) const {
    trace_context ctx;
    ctx.trace_id = span.trace_id;
    ctx.span_id = span.span_id;
    ctx.trace_flags = "01";  // Sampled
    ctx.baggage = span.baggage;
    return ctx;
}

common::Result<std::vector<trace_span>> distributed_tracer::get_trace(const std::string& trace_id) const {
    std::shared_lock lock(impl_->spans_mutex);

    auto it = impl_->traces.find(trace_id);
    if (it == impl_->traces.end()) {
        error_info err(kcenon::monitoring::monitoring_error_code::not_found, "Trace not found");
        return common::Result<std::vector<trace_span>>::err(err.to_common_error());
    }

    return common::ok(it->second);
}

common::Result<bool> distributed_tracer::export_spans(std::vector<trace_span> spans) {
    // In a real implementation, this would export to Jaeger, Zipkin, etc.
    // For now, just validate the spans
    for (const auto& span : spans) {
        if (!span.is_finished()) {
            return common::Result<bool>::err(error_info(kcenon::monitoring::monitoring_error_code::invalid_state, "Error").to_common_error());
        }
    }
    
    // Store spans
    for (const auto& span : spans) {
        auto result = impl_->store_span(span);
        if (result.is_err()) {
            return common::Result<bool>::err(result.error());
        }
    }

    return common::ok(true);
}

void distributed_tracer::set_exporter(std::shared_ptr<trace_exporter_interface> exporter) {
    std::lock_guard<std::mutex> lock(impl_->export_mutex_);
    impl_->exporter_ = std::move(exporter);
}

std::shared_ptr<trace_exporter_interface> distributed_tracer::get_exporter() const {
    std::lock_guard<std::mutex> lock(impl_->export_mutex_);
    return impl_->exporter_;
}

void distributed_tracer::configure_export(const trace_export_settings& settings) {
    std::lock_guard<std::mutex> lock(impl_->export_mutex_);
    impl_->export_settings_ = settings;
    impl_->export_batch_size_ = settings.batch_size;
}

trace_export_settings distributed_tracer::get_export_settings() const {
    std::lock_guard<std::mutex> lock(impl_->export_mutex_);
    return impl_->export_settings_;
}

common::VoidResult distributed_tracer::flush() {
    std::lock_guard<std::mutex> lock(impl_->export_mutex_);

    if (impl_->finished_spans_.empty()) {
        return common::ok();
    }

    if (!impl_->exporter_) {
        // No exporter configured, just clear the buffer
        impl_->finished_spans_.clear();
        return common::ok();
    }

    std::vector<trace_span> spans_to_export;
    spans_to_export.swap(impl_->finished_spans_);

    auto result = impl_->export_spans_to_exporter(spans_to_export);
    if (result.is_err()) {
        // On failure, put spans back
        impl_->finished_spans_.insert(impl_->finished_spans_.end(),
                                     spans_to_export.begin(),
                                     spans_to_export.end());
        impl_->enforce_queue_limit();
    }

    return result;
}

std::unordered_map<std::string, std::size_t> distributed_tracer::get_export_stats() const {
    return {
        {"exported_spans", impl_->exported_spans_.load()},
        {"failed_exports", impl_->failed_exports_.load()},
        {"dropped_spans", impl_->dropped_spans_.load()},
        {"pending_spans", impl_->finished_spans_.size()}
    };
}

// Global tracer instance
distributed_tracer& global_tracer() {
    static distributed_tracer instance;
    return instance;
}

} } // namespace kcenon::monitoring
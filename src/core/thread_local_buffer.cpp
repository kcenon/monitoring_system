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
 * @file thread_local_buffer.cpp
 * @brief Thread-local buffer implementation for lock-free metric collection
 */

#include <kcenon/monitoring/core/thread_local_buffer.h>
#include <kcenon/monitoring/core/central_collector.h>

namespace kcenon { namespace monitoring {

thread_local_buffer::thread_local_buffer(size_t capacity,
                                         std::shared_ptr<central_collector> collector)
    : capacity_(capacity)
    , collector_(collector)
    , stats_() {
    // Pre-allocate buffer to avoid runtime allocations
    buffer_.resize(capacity);
}

thread_local_buffer::~thread_local_buffer() {
    // Flush any remaining samples before destruction
    if (write_index_ > 0) {
        flush();
    }
}

bool thread_local_buffer::record(const metric_sample& sample) {
    if (is_full()) {
        return false;  // Buffer full, caller should flush
    }

    // Direct write to pre-allocated slot (no allocation)
    buffer_[write_index_] = sample;
    ++write_index_;
    ++stats_.total_records;
    return true;
}

bool thread_local_buffer::record_auto_flush(const metric_sample& sample) {
    if (!record(sample)) {
        // Buffer full, flush and retry
        flush();
        ++stats_.auto_flushes;
        return record(sample);
    }
    return true;
}

size_t thread_local_buffer::flush() {
    if (write_index_ == 0 || !collector_) {
        return 0;  // Nothing to flush or no collector
    }

    size_t flushed = write_index_;

    // Create a view of the valid samples for batch processing
    std::vector<metric_sample> batch(buffer_.begin(), buffer_.begin() + write_index_);
    collector_->receive_batch(batch);

    // Reset write index (buffer stays allocated)
    write_index_ = 0;
    ++stats_.total_flushes;

    return flushed;
}

}} // namespace kcenon::monitoring

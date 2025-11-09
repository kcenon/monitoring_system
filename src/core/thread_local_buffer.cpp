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
    , collector_(collector) {
    buffer_.reserve(capacity);
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

    buffer_.push_back(sample);
    ++write_index_;
    return true;
}

bool thread_local_buffer::record_auto_flush(const metric_sample& sample) {
    if (!record(sample)) {
        // Buffer full, flush and retry
        flush();
        return record(sample);
    }
    return true;
}

size_t thread_local_buffer::flush() {
    if (write_index_ == 0 || !collector_) {
        return 0;  // Nothing to flush or no collector
    }

    size_t flushed = write_index_;

    // Send batch to central collector
    collector_->receive_batch(buffer_);

    // Clear buffer
    buffer_.clear();
    write_index_ = 0;

    return flushed;
}

}} // namespace kcenon::monitoring

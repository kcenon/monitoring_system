/**
 * @file thread_context.cpp
 * @brief Thread context implementation
 */

#include "thread_context.h"

namespace monitoring_system {

// Static member definitions
thread_local std::unique_ptr<context_metadata> thread_context::current_context_ = nullptr;
std::atomic<uint64_t> thread_context::context_counter_{0};

} // namespace monitoring_system
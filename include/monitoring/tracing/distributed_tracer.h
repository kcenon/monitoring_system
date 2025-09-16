#pragma once

// Compatibility header - stub implementation for testing
#include <string>
#include <memory>
#include <unordered_map>

namespace monitoring_system {

// Forward declarations for distributed tracing
class trace_span;
class distributed_tracer;

// Stub implementation for compatibility
class distributed_tracer {
public:
    static distributed_tracer& instance() {
        static distributed_tracer inst;
        return inst;
    }

    std::shared_ptr<trace_span> start_span(const std::string& name) {
        // Stub implementation
        return nullptr;
    }

    void finish_span(std::shared_ptr<trace_span> span) {
        // Stub implementation
    }
};

class trace_span {
public:
    std::string name;
    std::unordered_map<std::string, std::string> tags;

    void set_tag(const std::string& key, const std::string& value) {
        tags[key] = value;
    }
};

} // namespace monitoring_system
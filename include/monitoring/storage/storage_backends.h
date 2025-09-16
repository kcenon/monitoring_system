#pragma once

// Storage backends implementation - stub for compatibility
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace monitoring_system {

/**
 * @brief Basic key-value storage interface - stub
 */
class kv_storage_backend {
public:
    virtual ~kv_storage_backend() = default;
    virtual bool store(const std::string& key, const std::string& value) = 0;
    virtual std::string retrieve(const std::string& key) = 0;
    virtual bool remove(const std::string& key) = 0;
};

/**
 * @brief In-memory storage backend - basic implementation
 */
class memory_storage_backend : public kv_storage_backend {
public:
    bool store(const std::string& key, const std::string& value) override {
        data_[key] = value;
        return true;
    }

    std::string retrieve(const std::string& key) override {
        auto it = data_.find(key);
        return it != data_.end() ? it->second : "";
    }

    bool remove(const std::string& key) override {
        return data_.erase(key) > 0;
    }

private:
    std::unordered_map<std::string, std::string> data_;
};

} // namespace monitoring_system
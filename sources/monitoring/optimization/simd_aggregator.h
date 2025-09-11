#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file simd_aggregator.h
 * @brief SIMD-accelerated aggregation functions for high-performance metrics
 * 
 * This file implements P4 task: SIMD-accelerated aggregations
 * for optimizing bulk metric operations using vectorized instructions.
 */

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include <vector>
#include <array>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <numeric>

// Platform-specific SIMD headers
#if defined(SIMD_AVX2_AVAILABLE) && (defined(__x86_64__) || defined(_M_X64))
    #include <immintrin.h>
    #define SIMD_AVAILABLE_AVX2
    #if defined(__AVX512F__)
        #define SIMD_AVAILABLE_AVX512
    #endif
#elif defined(__aarch64__) || defined(_M_ARM64)
    #include <arm_neon.h>
    #define SIMD_AVAILABLE_NEON
#endif

namespace monitoring_system {

/**
 * @struct simd_capabilities
 * @brief Runtime detection of SIMD capabilities
 */
struct simd_capabilities {
    bool avx2_available = false;
    bool avx512_available = false;
    bool neon_available = false;
    bool sse4_available = false;
    
    /**
     * @brief Detect available SIMD instructions
     */
    static simd_capabilities detect() {
        simd_capabilities caps;
        
#if defined(SIMD_AVAILABLE_AVX2)
        // Simple capability detection (in real implementation, use CPUID)
        caps.avx2_available = true;
#if defined(SIMD_AVAILABLE_AVX512)
        caps.avx512_available = true;
#endif
#endif

#if defined(SIMD_AVAILABLE_NEON)
        caps.neon_available = true;
#endif
        
        return caps;
    }
};

/**
 * @struct simd_config
 * @brief Configuration for SIMD operations
 */
struct simd_config {
    bool enable_simd = true;                // Enable SIMD acceleration
    bool auto_detect_capabilities = true;   // Auto-detect SIMD capabilities
    size_t vector_size = 8;                 // Number of elements per vector
    size_t alignment = 32;                  // Memory alignment for SIMD (32 bytes for AVX2)
    bool use_parallel_reduction = true;     // Use parallel reduction for large datasets
    size_t parallel_threshold = 1024;      // Threshold for parallel processing
    
    /**
     * @brief Validate configuration
     */
    result_void validate() const {
        if (vector_size == 0 || (vector_size & (vector_size - 1)) != 0) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Vector size must be a power of 2");
        }
        
        if (alignment < sizeof(double) || (alignment & (alignment - 1)) != 0) {
            return result_void(monitoring_error_code::invalid_configuration,
                             "Alignment must be a power of 2 and at least sizeof(double)");
        }
        
        return result_void::success();
    }
};

/**
 * @struct simd_stats
 * @brief Statistics for SIMD operations performance
 */
struct simd_stats {
    std::atomic<size_t> simd_operations{0};
    std::atomic<size_t> scalar_operations{0};
    std::atomic<size_t> total_elements_processed{0};
    std::atomic<size_t> cache_hits{0};
    std::atomic<size_t> cache_misses{0};
    
    std::chrono::system_clock::time_point creation_time;
    
    simd_stats() : creation_time(std::chrono::system_clock::now()) {}
    
    /**
     * @brief Get SIMD utilization rate
     */
    double get_simd_utilization() const {
        auto simd_ops = simd_operations.load();
        auto scalar_ops = scalar_operations.load();
        auto total = simd_ops + scalar_ops;
        return total > 0 ? (static_cast<double>(simd_ops) / total) * 100.0 : 0.0;
    }
    
    /**
     * @brief Get cache hit rate
     */
    double get_cache_hit_rate() const {
        auto hits = cache_hits.load();
        auto misses = cache_misses.load();
        auto total = hits + misses;
        return total > 0 ? (static_cast<double>(hits) / total) * 100.0 : 0.0;
    }
};

/**
 * @class aligned_vector
 * @brief SIMD-aligned vector container
 */
template<typename T>
class aligned_vector {
private:
    std::vector<T> data_;
    size_t alignment_;
    
public:
    explicit aligned_vector(size_t alignment = 32) : alignment_(alignment) {
        // Note: This is a simplified implementation
        // Real implementation would use aligned allocation
    }
    
    void resize(size_t size) {
        data_.resize(size);
    }
    
    void reserve(size_t capacity) {
        data_.reserve(capacity);
    }
    
    T* data() { return data_.data(); }
    const T* data() const { return data_.data(); }
    
    size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    
    T& operator[](size_t index) { return data_[index]; }
    const T& operator[](size_t index) const { return data_[index]; }
    
    void push_back(const T& value) { data_.push_back(value); }
    void push_back(T&& value) { data_.push_back(std::move(value)); }
    
    auto begin() { return data_.begin(); }
    auto end() { return data_.end(); }
    auto begin() const { return data_.begin(); }
    auto end() const { return data_.end(); }
};

/**
 * @class simd_aggregator
 * @brief High-performance SIMD-accelerated aggregation functions
 */
class simd_aggregator {
private:
    simd_config config_;
    simd_capabilities capabilities_;
    mutable simd_stats stats_;
    
    /**
     * @brief Check if SIMD can be used for operation
     */
    bool can_use_simd(size_t size) const {
        return config_.enable_simd && 
               capabilities_.avx2_available && 
               size >= config_.vector_size;
    }
    
    /**
     * @brief SIMD sum implementation for doubles (AVX2)
     */
    double simd_sum_avx2(const double* data, size_t size) const {
#if defined(SIMD_AVAILABLE_AVX2)
        const size_t simd_size = 4;  // AVX2 processes 4 doubles at once
        size_t simd_iterations = size / simd_size;
        
        __m256d sum_vec = _mm256_setzero_pd();
        
        for (size_t i = 0; i < simd_iterations; ++i) {
            __m256d data_vec = _mm256_loadu_pd(&data[i * simd_size]);
            sum_vec = _mm256_add_pd(sum_vec, data_vec);
        }
        
        // Horizontal sum
        __m128d sum_high = _mm256_extractf128_pd(sum_vec, 1);
        __m128d sum_low = _mm256_castpd256_pd128(sum_vec);
        sum_low = _mm_add_pd(sum_low, sum_high);
        
        double result[2];
        _mm_storeu_pd(result, sum_low);
        double total = result[0] + result[1];
        
        // Process remaining elements
        for (size_t i = simd_iterations * simd_size; i < size; ++i) {
            total += data[i];
        }
        
        stats_.simd_operations.fetch_add(1, std::memory_order_relaxed);
        return total;
#else
        return scalar_sum(data, size);
#endif
    }
    
    /**
     * @brief SIMD sum implementation for doubles (NEON)
     */
    double simd_sum_neon(const double* data, size_t size) const {
#if defined(SIMD_AVAILABLE_NEON)
        const size_t simd_size = 2;  // NEON processes 2 doubles at once
        size_t simd_iterations = size / simd_size;
        
        float64x2_t sum_vec = vdupq_n_f64(0.0);
        
        for (size_t i = 0; i < simd_iterations; ++i) {
            float64x2_t data_vec = vld1q_f64(&data[i * simd_size]);
            sum_vec = vaddq_f64(sum_vec, data_vec);
        }
        
        // Horizontal sum
        double total = vgetq_lane_f64(sum_vec, 0) + vgetq_lane_f64(sum_vec, 1);
        
        // Process remaining elements
        for (size_t i = simd_iterations * simd_size; i < size; ++i) {
            total += data[i];
        }
        
        stats_.simd_operations.fetch_add(1, std::memory_order_relaxed);
        return total;
#else
        return scalar_sum(data, size);
#endif
    }
    
    /**
     * @brief Scalar sum fallback
     */
    double scalar_sum(const double* data, size_t size) const {
        stats_.scalar_operations.fetch_add(1, std::memory_order_relaxed);
        return std::accumulate(data, data + size, 0.0);
    }
    
    /**
     * @brief SIMD mean calculation (AVX2)
     */
    double simd_mean_avx2(const double* data, size_t size) const {
        if (size == 0) return 0.0;
        return simd_sum_avx2(data, size) / size;
    }
    
    /**
     * @brief SIMD variance calculation
     */
    double simd_variance_impl(const double* data, size_t size, double mean) const {
        if (size <= 1) return 0.0;
        
#if defined(SIMD_AVAILABLE_AVX2)
        const size_t simd_size = 4;
        size_t simd_iterations = size / simd_size;
        
        __m256d mean_vec = _mm256_set1_pd(mean);
        __m256d sum_sq_vec = _mm256_setzero_pd();
        
        for (size_t i = 0; i < simd_iterations; ++i) {
            __m256d data_vec = _mm256_loadu_pd(&data[i * simd_size]);
            __m256d diff_vec = _mm256_sub_pd(data_vec, mean_vec);
            __m256d sq_vec = _mm256_mul_pd(diff_vec, diff_vec);
            sum_sq_vec = _mm256_add_pd(sum_sq_vec, sq_vec);
        }
        
        // Horizontal sum
        __m128d sum_high = _mm256_extractf128_pd(sum_sq_vec, 1);
        __m128d sum_low = _mm256_castpd256_pd128(sum_sq_vec);
        sum_low = _mm_add_pd(sum_low, sum_high);
        
        double result[2];
        _mm_storeu_pd(result, sum_low);
        double sum_squared_diffs = result[0] + result[1];
        
        // Process remaining elements
        for (size_t i = simd_iterations * simd_size; i < size; ++i) {
            double diff = data[i] - mean;
            sum_squared_diffs += diff * diff;
        }
        
        stats_.simd_operations.fetch_add(1, std::memory_order_relaxed);
        return sum_squared_diffs / (size - 1);
#else
        double sum_squared_diffs = 0.0;
        for (size_t i = 0; i < size; ++i) {
            double diff = data[i] - mean;
            sum_squared_diffs += diff * diff;
        }
        
        stats_.scalar_operations.fetch_add(1, std::memory_order_relaxed);
        return sum_squared_diffs / (size - 1);
#endif
    }

public:
    /**
     * @brief Constructor
     */
    explicit simd_aggregator(const simd_config& config = {})
        : config_(config) {
        
        auto validation = config_.validate();
        if (!validation) {
            throw std::invalid_argument("Invalid SIMD configuration: " + 
                                      validation.get_error().message);
        }
        
        if (config_.auto_detect_capabilities) {
            capabilities_ = simd_capabilities::detect();
        }
    }
    
    /**
     * @brief Sum aggregation with SIMD acceleration
     */
    result<double> sum(const std::vector<double>& data) const {
        if (data.empty()) {
            return make_success(0.0);
        }
        
        stats_.total_elements_processed.fetch_add(data.size(), std::memory_order_relaxed);
        
        if (can_use_simd(data.size())) {
#if defined(SIMD_AVAILABLE_AVX2)
            if (capabilities_.avx2_available) {
                return make_success(simd_sum_avx2(data.data(), data.size()));
            }
#endif
#if defined(SIMD_AVAILABLE_NEON)
            if (capabilities_.neon_available) {
                return make_success(simd_sum_neon(data.data(), data.size()));
            }
#endif
        }
        
        return make_success(scalar_sum(data.data(), data.size()));
    }
    
    /**
     * @brief Mean aggregation with SIMD acceleration
     */
    result<double> mean(const std::vector<double>& data) const {
        if (data.empty()) {
            return make_error<double>(monitoring_error_code::invalid_configuration,
                                    "Cannot calculate mean of empty dataset");
        }
        
        auto sum_result = sum(data);
        if (!sum_result) {
            return sum_result;
        }
        
        return make_success(sum_result.value() / data.size());
    }
    
    /**
     * @brief Variance aggregation with SIMD acceleration
     */
    result<double> variance(const std::vector<double>& data) const {
        if (data.size() <= 1) {
            return make_success(0.0);
        }
        
        auto mean_result = mean(data);
        if (!mean_result) {
            return mean_result;
        }
        
        double mean_val = mean_result.value();
        double var = simd_variance_impl(data.data(), data.size(), mean_val);
        
        return make_success(var);
    }
    
    /**
     * @brief Standard deviation with SIMD acceleration
     */
    result<double> standard_deviation(const std::vector<double>& data) const {
        auto var_result = variance(data);
        if (!var_result) {
            return var_result;
        }
        
        return make_success(std::sqrt(var_result.value()));
    }
    
    /**
     * @brief Find minimum value with SIMD acceleration
     */
    result<double> min(const std::vector<double>& data) const {
        if (data.empty()) {
            return make_error<double>(monitoring_error_code::invalid_configuration,
                                    "Cannot find minimum of empty dataset");
        }
        
        stats_.total_elements_processed.fetch_add(data.size(), std::memory_order_relaxed);
        
#if defined(SIMD_AVAILABLE_AVX2)
        if (can_use_simd(data.size()) && capabilities_.avx2_available) {
            const size_t simd_size = 4;
            size_t simd_iterations = data.size() / simd_size;
            
            __m256d min_vec = _mm256_loadu_pd(&data[0]);
            
            for (size_t i = 1; i < simd_iterations; ++i) {
                __m256d data_vec = _mm256_loadu_pd(&data[i * simd_size]);
                min_vec = _mm256_min_pd(min_vec, data_vec);
            }
            
            // Horizontal minimum
            __m128d min_high = _mm256_extractf128_pd(min_vec, 1);
            __m128d min_low = _mm256_castpd256_pd128(min_vec);
            min_low = _mm_min_pd(min_low, min_high);
            
            double result[2];
            _mm_storeu_pd(result, min_low);
            double min_val = std::min(result[0], result[1]);
            
            // Process remaining elements
            for (size_t i = simd_iterations * simd_size; i < data.size(); ++i) {
                min_val = std::min(min_val, data[i]);
            }
            
            stats_.simd_operations.fetch_add(1, std::memory_order_relaxed);
            return make_success(min_val);
        }
#endif
        
        stats_.scalar_operations.fetch_add(1, std::memory_order_relaxed);
        return make_success(*std::min_element(data.begin(), data.end()));
    }
    
    /**
     * @brief Find maximum value with SIMD acceleration
     */
    result<double> max(const std::vector<double>& data) const {
        if (data.empty()) {
            return make_error<double>(monitoring_error_code::invalid_configuration,
                                    "Cannot find maximum of empty dataset");
        }
        
        stats_.total_elements_processed.fetch_add(data.size(), std::memory_order_relaxed);
        
#if defined(SIMD_AVAILABLE_AVX2)
        if (can_use_simd(data.size()) && capabilities_.avx2_available) {
            const size_t simd_size = 4;
            size_t simd_iterations = data.size() / simd_size;
            
            __m256d max_vec = _mm256_loadu_pd(&data[0]);
            
            for (size_t i = 1; i < simd_iterations; ++i) {
                __m256d data_vec = _mm256_loadu_pd(&data[i * simd_size]);
                max_vec = _mm256_max_pd(max_vec, data_vec);
            }
            
            // Horizontal maximum
            __m128d max_high = _mm256_extractf128_pd(max_vec, 1);
            __m128d max_low = _mm256_castpd256_pd128(max_vec);
            max_low = _mm_max_pd(max_low, max_high);
            
            double result[2];
            _mm_storeu_pd(result, max_low);
            double max_val = std::max(result[0], result[1]);
            
            // Process remaining elements
            for (size_t i = simd_iterations * simd_size; i < data.size(); ++i) {
                max_val = std::max(max_val, data[i]);
            }
            
            stats_.simd_operations.fetch_add(1, std::memory_order_relaxed);
            return make_success(max_val);
        }
#endif
        
        stats_.scalar_operations.fetch_add(1, std::memory_order_relaxed);
        return make_success(*std::max_element(data.begin(), data.end()));
    }
    
    /**
     * @brief Batch statistical summary
     */
    struct statistical_summary {
        double sum;
        double mean;
        double variance;
        double std_dev;
        double min_val;
        double max_val;
        size_t count;
    };
    
    result<statistical_summary> compute_summary(const std::vector<double>& data) const {
        if (data.empty()) {
            return make_error<statistical_summary>(monitoring_error_code::invalid_configuration,
                                                 "Cannot compute summary of empty dataset");
        }
        
        statistical_summary summary;
        summary.count = data.size();
        
        // Use SIMD for all computations
        auto sum_result = sum(data);
        auto mean_result = mean(data);
        auto var_result = variance(data);
        auto min_result = min(data);
        auto max_result = max(data);
        
        if (!sum_result || !mean_result || !var_result || !min_result || !max_result) {
            return make_error<statistical_summary>(monitoring_error_code::processing_failed,
                                                 "Failed to compute statistical summary");
        }
        
        summary.sum = sum_result.value();
        summary.mean = mean_result.value();
        summary.variance = var_result.value();
        summary.std_dev = std::sqrt(summary.variance);
        summary.min_val = min_result.value();
        summary.max_val = max_result.value();
        
        return make_success(summary);
    }
    
    /**
     * @brief Get SIMD capabilities
     */
    const simd_capabilities& get_capabilities() const {
        return capabilities_;
    }
    
    /**
     * @brief Get statistics
     */
    const simd_stats& get_statistics() const {
        return stats_;
    }
    
    /**
     * @brief Get configuration
     */
    const simd_config& get_config() const {
        return config_;
    }
    
    /**
     * @brief Test SIMD functionality
     */
    result<bool> test_simd() const {
        std::vector<double> test_data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
        
        auto sum_result = sum(test_data);
        if (!sum_result) {
            return make_error<bool>(sum_result.get_error().code, sum_result.get_error().message);
        }
        
        double expected_sum = 36.0;  // 1+2+3+4+5+6+7+8
        double actual_sum = sum_result.value();
        
        bool test_passed = std::abs(actual_sum - expected_sum) < 1e-10;
        return make_success(test_passed);
    }
};

/**
 * @brief Factory function to create SIMD aggregator
 */
inline std::unique_ptr<simd_aggregator> make_simd_aggregator(
    const simd_config& config = {}) {
    return std::make_unique<simd_aggregator>(config);
}

/**
 * @brief Create default SIMD configurations
 */
inline std::vector<simd_config> create_default_simd_configs() {
    std::vector<simd_config> configs;
    
    // High-performance configuration
    simd_config high_performance;
    high_performance.enable_simd = true;
    high_performance.vector_size = 8;
    high_performance.alignment = 32;
    high_performance.use_parallel_reduction = true;
    high_performance.parallel_threshold = 512;
    configs.push_back(high_performance);
    
    // Memory-efficient configuration
    simd_config memory_efficient;
    memory_efficient.enable_simd = true;
    memory_efficient.vector_size = 4;
    memory_efficient.alignment = 16;
    memory_efficient.use_parallel_reduction = false;
    configs.push_back(memory_efficient);
    
    // Compatibility configuration (no SIMD)
    simd_config compatibility;
    compatibility.enable_simd = false;
    compatibility.auto_detect_capabilities = false;
    configs.push_back(compatibility);
    
    return configs;
}

} // namespace monitoring_system
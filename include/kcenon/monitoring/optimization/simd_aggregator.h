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

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <numeric>
#include <vector>

#include "kcenon/monitoring/core/result_types.h"

// Platform-specific SIMD includes
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #if defined(__AVX2__)
        #include <immintrin.h>
        #define SIMD_AVX2_AVAILABLE 1
    #elif defined(__SSE4_1__)
        #include <smmintrin.h>
        #define SIMD_SSE4_AVAILABLE 1
    #elif defined(__SSE2__)
        #include <emmintrin.h>
        #define SIMD_SSE2_AVAILABLE 1
    #endif
#elif defined(__aarch64__) || defined(_M_ARM64)
    #include <arm_neon.h>
    #define SIMD_NEON_AVAILABLE 1
#endif

namespace kcenon::monitoring {

/**
 * @brief SIMD capabilities detection
 */
struct simd_capabilities {
    bool sse2_available = false;
    bool sse4_available = false;
    bool avx_available = false;
    bool avx2_available = false;
    bool avx512_available = false;
    bool neon_available = false;

    /**
     * @brief Detect available SIMD features at runtime
     */
    static simd_capabilities detect() {
        simd_capabilities caps;

#if defined(SIMD_AVX2_AVAILABLE)
        caps.avx2_available = true;
        caps.avx_available = true;
        caps.sse4_available = true;
        caps.sse2_available = true;
#elif defined(SIMD_SSE4_AVAILABLE)
        caps.sse4_available = true;
        caps.sse2_available = true;
#elif defined(SIMD_SSE2_AVAILABLE)
        caps.sse2_available = true;
#elif defined(SIMD_NEON_AVAILABLE)
        caps.neon_available = true;
#endif

        return caps;
    }
};

/**
 * @brief Configuration for SIMD aggregator
 */
struct simd_config {
    bool enable_simd = true;           ///< Enable SIMD acceleration
    size_t vector_size = 8;            ///< SIMD vector width for processing
    size_t alignment = 32;             ///< Memory alignment for SIMD operations
    bool use_fma = true;               ///< Use fused multiply-add if available

    /**
     * @brief Validate configuration
     * @return true if configuration is valid
     */
    bool validate() const {
        // Vector size must be power of 2
        if (vector_size == 0 || (vector_size & (vector_size - 1)) != 0) {
            return false;
        }
        // Alignment must be power of 2
        if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
            return false;
        }
        return true;
    }
};

/**
 * @brief Statistical summary result
 */
struct statistical_summary {
    size_t count = 0;
    double sum = 0.0;
    double mean = 0.0;
    double variance = 0.0;
    double std_dev = 0.0;
    double min_val = 0.0;
    double max_val = 0.0;
};

/**
 * @brief Statistics for SIMD aggregator operations
 */
struct simd_aggregator_statistics {
    std::atomic<size_t> total_operations{0};
    std::atomic<size_t> simd_operations{0};
    std::atomic<size_t> scalar_operations{0};
    std::atomic<size_t> total_elements_processed{0};

    simd_aggregator_statistics() = default;
    simd_aggregator_statistics(const simd_aggregator_statistics& other)
        : total_operations(other.total_operations.load())
        , simd_operations(other.simd_operations.load())
        , scalar_operations(other.scalar_operations.load())
        , total_elements_processed(other.total_elements_processed.load()) {}

    simd_aggregator_statistics& operator=(const simd_aggregator_statistics& other) {
        if (this != &other) {
            total_operations.store(other.total_operations.load());
            simd_operations.store(other.simd_operations.load());
            scalar_operations.store(other.scalar_operations.load());
            total_elements_processed.store(other.total_elements_processed.load());
        }
        return *this;
    }

    simd_aggregator_statistics(simd_aggregator_statistics&& other) noexcept
        : total_operations(other.total_operations.load())
        , simd_operations(other.simd_operations.load())
        , scalar_operations(other.scalar_operations.load())
        , total_elements_processed(other.total_elements_processed.load()) {}

    simd_aggregator_statistics& operator=(simd_aggregator_statistics&& other) noexcept {
        if (this != &other) {
            total_operations.store(other.total_operations.load());
            simd_operations.store(other.simd_operations.load());
            scalar_operations.store(other.scalar_operations.load());
            total_elements_processed.store(other.total_elements_processed.load());
        }
        return *this;
    }

    /**
     * @brief Get SIMD utilization rate
     * @return Percentage of operations using SIMD (0.0 to 100.0)
     */
    double get_simd_utilization() const {
        auto total = total_operations.load();
        if (total == 0) {
            return 0.0;
        }
        return (static_cast<double>(simd_operations.load()) / static_cast<double>(total)) * 100.0;
    }

    /**
     * @brief Reset all statistics
     */
    void reset() {
        total_operations.store(0);
        simd_operations.store(0);
        scalar_operations.store(0);
        total_elements_processed.store(0);
    }
};

/**
 * @brief SIMD-accelerated statistical aggregator
 *
 * This class provides high-performance statistical operations using
 * SIMD (Single Instruction Multiple Data) instructions when available.
 * Falls back to scalar operations when SIMD is not available or disabled.
 */
class simd_aggregator {
public:
    /**
     * @brief Default constructor with default configuration
     */
    simd_aggregator() : simd_aggregator(simd_config{}) {}

    /**
     * @brief Construct with configuration
     * @param config Aggregator configuration
     */
    explicit simd_aggregator(const simd_config& config)
        : config_(config)
        , capabilities_(simd_capabilities::detect()) {}

    /**
     * @brief Calculate sum of elements
     * @param data Input data vector
     * @return result<double> containing sum
     */
    result<double> sum(const std::vector<double>& data) {
        if (data.empty()) {
            return make_error<double>(monitoring_error_code::invalid_argument,
                                      "Cannot compute sum of empty data");
        }

        stats_.total_operations++;
        stats_.total_elements_processed += data.size();

        double result = 0.0;

        if (should_use_simd(data.size())) {
            result = sum_simd(data);
            stats_.simd_operations++;
        } else {
            result = sum_scalar(data);
            stats_.scalar_operations++;
        }

        return common::ok(result);
    }

    /**
     * @brief Calculate mean of elements
     * @param data Input data vector
     * @return result<double> containing mean
     */
    result<double> mean(const std::vector<double>& data) {
        if (data.empty()) {
            return make_error<double>(monitoring_error_code::invalid_argument,
                                      "Cannot compute mean of empty data");
        }

        auto sum_result = sum(data);
        if (sum_result.is_err()) {
            return sum_result;
        }

        return common::ok(sum_result.value() / static_cast<double>(data.size()));
    }

    /**
     * @brief Find minimum value
     * @param data Input data vector
     * @return result<double> containing minimum
     */
    result<double> min(const std::vector<double>& data) {
        if (data.empty()) {
            return make_error<double>(monitoring_error_code::invalid_argument,
                                      "Cannot compute min of empty data");
        }

        stats_.total_operations++;
        stats_.total_elements_processed += data.size();

        double result = 0.0;

        if (should_use_simd(data.size())) {
            result = min_simd(data);
            stats_.simd_operations++;
        } else {
            result = min_scalar(data);
            stats_.scalar_operations++;
        }

        return common::ok(result);
    }

    /**
     * @brief Find maximum value
     * @param data Input data vector
     * @return result<double> containing maximum
     */
    result<double> max(const std::vector<double>& data) {
        if (data.empty()) {
            return make_error<double>(monitoring_error_code::invalid_argument,
                                      "Cannot compute max of empty data");
        }

        stats_.total_operations++;
        stats_.total_elements_processed += data.size();

        double result = 0.0;

        if (should_use_simd(data.size())) {
            result = max_simd(data);
            stats_.simd_operations++;
        } else {
            result = max_scalar(data);
            stats_.scalar_operations++;
        }

        return common::ok(result);
    }

    /**
     * @brief Calculate variance of elements
     * @param data Input data vector
     * @return result<double> containing variance
     */
    result<double> variance(const std::vector<double>& data) {
        if (data.empty()) {
            return make_error<double>(monitoring_error_code::invalid_argument,
                                      "Cannot compute variance of empty data");
        }

        if (data.size() == 1) {
            return common::ok(0.0);
        }

        auto mean_result = mean(data);
        if (mean_result.is_err()) {
            return mean_result;
        }

        double data_mean = mean_result.value();
        double sum_sq_diff = 0.0;

        for (const auto& val : data) {
            double diff = val - data_mean;
            sum_sq_diff += diff * diff;
        }

        return common::ok(sum_sq_diff / static_cast<double>(data.size() - 1));
    }

    /**
     * @brief Compute full statistical summary
     * @param data Input data vector
     * @return result<statistical_summary> containing statistics
     */
    result<statistical_summary> compute_summary(const std::vector<double>& data) {
        if (data.empty()) {
            return make_error<statistical_summary>(monitoring_error_code::invalid_argument,
                                                   "Cannot compute summary of empty data");
        }

        statistical_summary summary;
        summary.count = data.size();

        // Compute sum
        auto sum_result = sum(data);
        if (sum_result.is_err()) {
            return make_error<statistical_summary>(monitoring_error_code::operation_failed,
                                                   "Failed to compute sum");
        }
        summary.sum = sum_result.value();
        summary.mean = summary.sum / static_cast<double>(summary.count);

        // Compute min/max
        auto min_result = min(data);
        auto max_result = max(data);

        if (min_result.is_err() || max_result.is_err()) {
            return make_error<statistical_summary>(monitoring_error_code::operation_failed,
                                                   "Failed to compute min/max");
        }

        summary.min_val = min_result.value();
        summary.max_val = max_result.value();

        // Compute variance
        if (summary.count > 1) {
            auto var_result = variance(data);
            if (var_result.is_ok()) {
                summary.variance = var_result.value();
                summary.std_dev = std::sqrt(summary.variance);
            }
        }

        return common::ok(summary);
    }

    /**
     * @brief Get SIMD capabilities
     * @return Reference to detected capabilities
     */
    const simd_capabilities& get_capabilities() const {
        return capabilities_;
    }

    /**
     * @brief Self-test SIMD functionality
     * @return result<bool> indicating if SIMD is working correctly
     */
    result<bool> test_simd() {
        // Create test data
        std::vector<double> test_data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};

        // Test sum
        auto sum_result = sum(test_data);
        if (sum_result.is_err()) {
            return common::ok(false);
        }

        if (std::abs(sum_result.value() - 36.0) > 1e-10) {
            return common::ok(false);
        }

        // Test mean
        auto mean_result = mean(test_data);
        if (mean_result.is_err()) {
            return common::ok(false);
        }

        if (std::abs(mean_result.value() - 4.5) > 1e-10) {
            return common::ok(false);
        }

        // Test min/max
        auto min_result = min(test_data);
        auto max_result = max(test_data);

        if (min_result.is_err() || max_result.is_err()) {
            return common::ok(false);
        }

        if (std::abs(min_result.value() - 1.0) > 1e-10 ||
            std::abs(max_result.value() - 8.0) > 1e-10) {
            return common::ok(false);
        }

        return common::ok(true);
    }

    /**
     * @brief Get aggregator statistics
     * @return Reference to statistics
     */
    const simd_aggregator_statistics& get_statistics() const {
        return stats_;
    }

    /**
     * @brief Reset statistics
     */
    void reset_statistics() {
        stats_.reset();
    }

private:
    bool should_use_simd(size_t data_size) const {
        if (!config_.enable_simd) {
            return false;
        }

        // Use SIMD only for sufficiently large datasets
        if (data_size < config_.vector_size * 2) {
            return false;
        }

        // Check if any SIMD is available
        return capabilities_.avx2_available ||
               capabilities_.sse2_available ||
               capabilities_.neon_available;
    }

    double sum_scalar(const std::vector<double>& data) const {
        return std::accumulate(data.begin(), data.end(), 0.0);
    }

    double sum_simd(const std::vector<double>& data) const {
#if defined(SIMD_AVX2_AVAILABLE)
        const size_t simd_width = 4;  // AVX processes 4 doubles at a time
        size_t simd_count = data.size() / simd_width;

        __m256d sum_vec = _mm256_setzero_pd();

        for (size_t i = 0; i < simd_count; ++i) {
            __m256d vec = _mm256_loadu_pd(&data[i * simd_width]);
            sum_vec = _mm256_add_pd(sum_vec, vec);
        }

        // Horizontal sum
        alignas(32) double temp[4];
        _mm256_storeu_pd(temp, sum_vec);
        double result = temp[0] + temp[1] + temp[2] + temp[3];

        // Handle remaining elements
        for (size_t i = simd_count * simd_width; i < data.size(); ++i) {
            result += data[i];
        }

        return result;
#elif defined(SIMD_SSE2_AVAILABLE)
        const size_t simd_width = 2;  // SSE processes 2 doubles at a time
        size_t simd_count = data.size() / simd_width;

        __m128d sum_vec = _mm_setzero_pd();

        for (size_t i = 0; i < simd_count; ++i) {
            __m128d vec = _mm_loadu_pd(&data[i * simd_width]);
            sum_vec = _mm_add_pd(sum_vec, vec);
        }

        alignas(16) double temp[2];
        _mm_storeu_pd(temp, sum_vec);
        double result = temp[0] + temp[1];

        for (size_t i = simd_count * simd_width; i < data.size(); ++i) {
            result += data[i];
        }

        return result;
#elif defined(SIMD_NEON_AVAILABLE)
        const size_t simd_width = 2;  // NEON processes 2 doubles at a time
        size_t simd_count = data.size() / simd_width;

        float64x2_t sum_vec = vdupq_n_f64(0.0);

        for (size_t i = 0; i < simd_count; ++i) {
            float64x2_t vec = vld1q_f64(&data[i * simd_width]);
            sum_vec = vaddq_f64(sum_vec, vec);
        }

        double result = vgetq_lane_f64(sum_vec, 0) + vgetq_lane_f64(sum_vec, 1);

        for (size_t i = simd_count * simd_width; i < data.size(); ++i) {
            result += data[i];
        }

        return result;
#else
        return sum_scalar(data);
#endif
    }

    double min_scalar(const std::vector<double>& data) const {
        return *std::min_element(data.begin(), data.end());
    }

    double min_simd(const std::vector<double>& data) const {
#if defined(SIMD_AVX2_AVAILABLE)
        const size_t simd_width = 4;
        size_t simd_count = data.size() / simd_width;

        __m256d min_vec = _mm256_set1_pd(std::numeric_limits<double>::max());

        for (size_t i = 0; i < simd_count; ++i) {
            __m256d vec = _mm256_loadu_pd(&data[i * simd_width]);
            min_vec = _mm256_min_pd(min_vec, vec);
        }

        alignas(32) double temp[4];
        _mm256_storeu_pd(temp, min_vec);
        double result = std::min({temp[0], temp[1], temp[2], temp[3]});

        for (size_t i = simd_count * simd_width; i < data.size(); ++i) {
            result = std::min(result, data[i]);
        }

        return result;
#elif defined(SIMD_SSE2_AVAILABLE)
        const size_t simd_width = 2;
        size_t simd_count = data.size() / simd_width;

        __m128d min_vec = _mm_set1_pd(std::numeric_limits<double>::max());

        for (size_t i = 0; i < simd_count; ++i) {
            __m128d vec = _mm_loadu_pd(&data[i * simd_width]);
            min_vec = _mm_min_pd(min_vec, vec);
        }

        alignas(16) double temp[2];
        _mm_storeu_pd(temp, min_vec);
        double result = std::min(temp[0], temp[1]);

        for (size_t i = simd_count * simd_width; i < data.size(); ++i) {
            result = std::min(result, data[i]);
        }

        return result;
#elif defined(SIMD_NEON_AVAILABLE)
        const size_t simd_width = 2;
        size_t simd_count = data.size() / simd_width;

        float64x2_t min_vec = vdupq_n_f64(std::numeric_limits<double>::max());

        for (size_t i = 0; i < simd_count; ++i) {
            float64x2_t vec = vld1q_f64(&data[i * simd_width]);
            min_vec = vminq_f64(min_vec, vec);
        }

        double result = std::min(vgetq_lane_f64(min_vec, 0), vgetq_lane_f64(min_vec, 1));

        for (size_t i = simd_count * simd_width; i < data.size(); ++i) {
            result = std::min(result, data[i]);
        }

        return result;
#else
        return min_scalar(data);
#endif
    }

    double max_scalar(const std::vector<double>& data) const {
        return *std::max_element(data.begin(), data.end());
    }

    double max_simd(const std::vector<double>& data) const {
#if defined(SIMD_AVX2_AVAILABLE)
        const size_t simd_width = 4;
        size_t simd_count = data.size() / simd_width;

        __m256d max_vec = _mm256_set1_pd(std::numeric_limits<double>::lowest());

        for (size_t i = 0; i < simd_count; ++i) {
            __m256d vec = _mm256_loadu_pd(&data[i * simd_width]);
            max_vec = _mm256_max_pd(max_vec, vec);
        }

        alignas(32) double temp[4];
        _mm256_storeu_pd(temp, max_vec);
        double result = std::max({temp[0], temp[1], temp[2], temp[3]});

        for (size_t i = simd_count * simd_width; i < data.size(); ++i) {
            result = std::max(result, data[i]);
        }

        return result;
#elif defined(SIMD_SSE2_AVAILABLE)
        const size_t simd_width = 2;
        size_t simd_count = data.size() / simd_width;

        __m128d max_vec = _mm_set1_pd(std::numeric_limits<double>::lowest());

        for (size_t i = 0; i < simd_count; ++i) {
            __m128d vec = _mm_loadu_pd(&data[i * simd_width]);
            max_vec = _mm_max_pd(max_vec, vec);
        }

        alignas(16) double temp[2];
        _mm_storeu_pd(temp, max_vec);
        double result = std::max(temp[0], temp[1]);

        for (size_t i = simd_count * simd_width; i < data.size(); ++i) {
            result = std::max(result, data[i]);
        }

        return result;
#elif defined(SIMD_NEON_AVAILABLE)
        const size_t simd_width = 2;
        size_t simd_count = data.size() / simd_width;

        float64x2_t max_vec = vdupq_n_f64(std::numeric_limits<double>::lowest());

        for (size_t i = 0; i < simd_count; ++i) {
            float64x2_t vec = vld1q_f64(&data[i * simd_width]);
            max_vec = vmaxq_f64(max_vec, vec);
        }

        double result = std::max(vgetq_lane_f64(max_vec, 0), vgetq_lane_f64(max_vec, 1));

        for (size_t i = simd_count * simd_width; i < data.size(); ++i) {
            result = std::max(result, data[i]);
        }

        return result;
#else
        return max_scalar(data);
#endif
    }

    simd_config config_;
    simd_capabilities capabilities_;
    mutable simd_aggregator_statistics stats_;
};

/**
 * @brief Create a SIMD aggregator with default configuration
 * @return Unique pointer to the aggregator
 */
inline std::unique_ptr<simd_aggregator> make_simd_aggregator() {
    return std::make_unique<simd_aggregator>();
}

/**
 * @brief Create a SIMD aggregator with configuration
 * @param config Aggregator configuration
 * @return Unique pointer to the aggregator
 */
inline std::unique_ptr<simd_aggregator> make_simd_aggregator(const simd_config& config) {
    return std::make_unique<simd_aggregator>(config);
}

/**
 * @brief Create default SIMD configurations for different use cases
 * @return Vector of configurations
 */
inline std::vector<simd_config> create_default_simd_configs() {
    return {
        // SIMD enabled with default settings
        {.enable_simd = true, .vector_size = 8, .alignment = 32, .use_fma = true},
        // SIMD disabled for comparison
        {.enable_simd = false, .vector_size = 8, .alignment = 32, .use_fma = false},
        // Small vector size for smaller datasets
        {.enable_simd = true, .vector_size = 4, .alignment = 16, .use_fma = true},
        // Large vector size for AVX-512
        {.enable_simd = true, .vector_size = 16, .alignment = 64, .use_fma = true}
    };
}

} // namespace kcenon::monitoring

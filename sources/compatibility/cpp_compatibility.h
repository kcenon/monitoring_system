#pragma once

// C++17/C++20 compatibility layer for monitoring_system
// This header provides feature detection and threading compatibility

#include <type_traits>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

##################################################
// Threading Compatibility Layer
##################################################

// std::jthread compatibility
#ifdef MONITORING_HAS_STD_JTHREAD
    #include <thread>
    #include <stop_token>
    namespace monitoring_compat {
        using std::jthread;
        using std::stop_token;
        using std::stop_source;
        using std::stop_callback;
    }
    #define MONITORING_JTHREAD_AVAILABLE 1
#else
    #include <thread>
    #include <atomic>
    #include <functional>
    namespace monitoring_compat {
        // Simple jthread implementation for C++17
        class stop_token {
        public:
            stop_token() = default;
            explicit stop_token(std::shared_ptr<std::atomic<bool>> stop_state)
                : stop_state_(std::move(stop_state)) {}
                
            bool stop_requested() const noexcept {
                return stop_state_ && stop_state_->load();
            }
            
            bool stop_possible() const noexcept {
                return static_cast<bool>(stop_state_);
            }
            
        private:
            std::shared_ptr<std::atomic<bool>> stop_state_;
        };
        
        class stop_source {
        public:
            stop_source() : stop_state_(std::make_shared<std::atomic<bool>>(false)) {}
            
            stop_token get_token() {
                return stop_token(stop_state_);
            }
            
            bool request_stop() {
                if (stop_state_) {
                    stop_state_->store(true);
                    return true;
                }
                return false;
            }
            
            bool stop_requested() const noexcept {
                return stop_state_ && stop_state_->load();
            }
            
        private:
            std::shared_ptr<std::atomic<bool>> stop_state_;
        };
        
        class jthread {
        public:
            using id = std::thread::id;
            using native_handle_type = std::thread::native_handle_type;
            
            jthread() noexcept = default;
            
            template<typename F, typename... Args>
            explicit jthread(F&& f, Args&&... args) 
                : stop_source_{}
                , thread_([this, f = std::forward<F>(f), args...]() mutable {
                    if constexpr (std::is_invocable_v<F, stop_token, Args...>) {
                        f(stop_source_.get_token(), args...);
                    } else {
                        f(args...);
                    }
                }) {}
            
            ~jthread() {
                if (joinable()) {
                    request_stop();
                    join();
                }
            }
            
            jthread(const jthread&) = delete;
            jthread& operator=(const jthread&) = delete;
            
            jthread(jthread&&) noexcept = default;
            jthread& operator=(jthread&&) noexcept = default;
            
            void swap(jthread& other) noexcept {
                std::swap(stop_source_, other.stop_source_);
                std::swap(thread_, other.thread_);
            }
            
            bool joinable() const noexcept {
                return thread_.joinable();
            }
            
            void join() {
                thread_.join();
            }
            
            void detach() {
                thread_.detach();
            }
            
            id get_id() const noexcept {
                return thread_.get_id();
            }
            
            native_handle_type native_handle() {
                return thread_.native_handle();
            }
            
            stop_source& get_stop_source() noexcept {
                return stop_source_;
            }
            
            stop_token get_stop_token() {
                return stop_source_.get_token();
            }
            
            bool request_stop() {
                return stop_source_.request_stop();
            }
            
            static unsigned int hardware_concurrency() noexcept {
                return std::thread::hardware_concurrency();
            }
            
        private:
            stop_source stop_source_;
            std::thread thread_;
        };
        
        template<typename F>
        class stop_callback {
        public:
            template<typename Callback>
            explicit stop_callback(const stop_token& token, Callback&& cb) 
                : callback_(std::forward<Callback>(cb)) {
                if (token.stop_requested()) {
                    callback_();
                }
            }
            
            ~stop_callback() = default;
            
        private:
            F callback_;
        };
    }
    #define MONITORING_JTHREAD_AVAILABLE 0
#endif

// std::span compatibility
#ifdef MONITORING_HAS_STD_SPAN
    #include <span>
    namespace monitoring_compat {
        template<typename T, std::size_t Extent = std::dynamic_extent>
        using span = std::span<T, Extent>;
    }
    #define MONITORING_SPAN_AVAILABLE 1
#else
    #include <iterator>
    namespace monitoring_compat {
        // Simple span-like class for C++17 (reuse from logger_system pattern)
        template<typename T, std::size_t Extent = SIZE_MAX>
        class span {
        public:
            using element_type = T;
            using value_type = std::remove_cv_t<T>;
            using size_type = std::size_t;
            using pointer = T*;
            using reference = T&;
            using iterator = T*;
            
            constexpr span() noexcept : data_(nullptr), size_(0) {}
            constexpr span(pointer ptr, size_type count) : data_(ptr), size_(count) {}
            
            template<std::size_t N>
            constexpr span(element_type (&arr)[N]) noexcept : data_(arr), size_(N) {}
            
            constexpr iterator begin() const noexcept { return data_; }
            constexpr iterator end() const noexcept { return data_ + size_; }
            constexpr reference operator[](size_type idx) const { return data_[idx]; }
            constexpr pointer data() const noexcept { return data_; }
            constexpr size_type size() const noexcept { return size_; }
            constexpr bool empty() const noexcept { return size_ == 0; }
            
        private:
            pointer data_;
            size_type size_;
        };
    }
    #define MONITORING_SPAN_AVAILABLE 0
#endif

// Atomic wait/notify compatibility
#ifdef MONITORING_HAS_ATOMIC_WAIT
    namespace monitoring_compat {
        template<typename T>
        void atomic_wait(const std::atomic<T>& atomic_obj, T old_value) {
            atomic_obj.wait(old_value);
        }
        
        template<typename T>
        void atomic_notify_one(std::atomic<T>& atomic_obj) {
            atomic_obj.notify_one();
        }
        
        template<typename T>
        void atomic_notify_all(std::atomic<T>& atomic_obj) {
            atomic_obj.notify_all();
        }
    }
    #define MONITORING_ATOMIC_WAIT_AVAILABLE 1
#else
    #include <condition_variable>
    #include <mutex>
    namespace monitoring_compat {
        // Fallback implementation using condition variables
        template<typename T>
        void atomic_wait(const std::atomic<T>& atomic_obj, T old_value) {
            // Simple spin-wait fallback (not efficient but functional)
            while (atomic_obj.load() == old_value) {
                std::this_thread::yield();
            }
        }
        
        template<typename T>
        void atomic_notify_one(std::atomic<T>& atomic_obj) {
            // No-op for fallback implementation
            (void)atomic_obj;
        }
        
        template<typename T>
        void atomic_notify_all(std::atomic<T>& atomic_obj) {
            // No-op for fallback implementation
            (void)atomic_obj;
        }
    }
    #define MONITORING_ATOMIC_WAIT_AVAILABLE 0
#endif

// std::barrier compatibility
#ifdef MONITORING_HAS_STD_BARRIER
    #include <barrier>
    namespace monitoring_compat {
        template<typename CompletionFunction = std::noop_coroutine_handle>
        using barrier = std::barrier<CompletionFunction>;
    }
    #define MONITORING_BARRIER_AVAILABLE 1
#else
    #include <mutex>
    #include <condition_variable>
    namespace monitoring_compat {
        template<typename CompletionFunction = void(*)()>
        class barrier {
        public:
            explicit barrier(std::ptrdiff_t expected) 
                : expected_(expected), arrived_(0), generation_(0) {}
            
            void arrive_and_wait() {
                std::unique_lock<std::mutex> lock(mutex_);
                auto gen = generation_;
                
                if (++arrived_ == expected_) {
                    arrived_ = 0;
                    ++generation_;
                    cv_.notify_all();
                } else {
                    cv_.wait(lock, [this, gen] { return gen != generation_; });
                }
            }
            
            [[nodiscard]] auto arrive(std::ptrdiff_t update = 1) {
                std::unique_lock<std::mutex> lock(mutex_);
                arrived_ += update;
                if (arrived_ >= expected_) {
                    arrived_ = 0;
                    ++generation_;
                    cv_.notify_all();
                }
            }
            
            void wait() const {
                std::unique_lock<std::mutex> lock(mutex_);
                auto gen = generation_;
                cv_.wait(lock, [this, gen] { return gen != generation_; });
            }
            
        private:
            mutable std::mutex mutex_;
            mutable std::condition_variable cv_;
            std::ptrdiff_t expected_;
            std::ptrdiff_t arrived_;
            std::size_t generation_;
        };
    }
    #define MONITORING_BARRIER_AVAILABLE 0
#endif

// Concepts compatibility
#ifdef MONITORING_HAS_CONCEPTS
    #include <concepts>
    namespace monitoring_compat {
        template<typename T>
        concept Numeric = std::integral<T> || std::floating_point<T>;
        
        template<typename T>
        concept Monitorable = requires(T t) {
            { t.get_metrics() } -> std::convertible_to<std::string>;
        };
        
        template<typename T>
        concept Traceable = requires(T t) {
            { t.trace_id() } -> std::convertible_to<std::string>;
            { t.start_time() } -> Numeric;
            { t.end_time() } -> Numeric;
        };
    }
    #define MONITORING_CONCEPTS_AVAILABLE 1
#else
    namespace monitoring_compat {
        // SFINAE-based concept emulation
        template<typename T, typename = void>
        struct is_numeric : std::false_type {};
        
        template<typename T>
        struct is_numeric<T, std::enable_if_t<std::is_integral_v<T> || std::is_floating_point_v<T>>>
            : std::true_type {};
        
        template<typename T, typename = void>
        struct is_monitorable : std::false_type {};
        
        template<typename T>
        struct is_monitorable<T, std::void_t<decltype(std::declval<T>().get_metrics())>>
            : std::true_type {};
        
        template<typename T, typename = void>
        struct is_traceable : std::false_type {};
        
        template<typename T>
        struct is_traceable<T, std::void_t<
            decltype(std::declval<T>().trace_id()),
            decltype(std::declval<T>().start_time()),
            decltype(std::declval<T>().end_time())
        >> : std::true_type {};
        
        template<typename T>
        inline constexpr bool Numeric = is_numeric<T>::value;
        
        template<typename T>
        inline constexpr bool Monitorable = is_monitorable<T>::value;
        
        template<typename T>
        inline constexpr bool Traceable = is_traceable<T>::value;
    }
    #define MONITORING_CONCEPTS_AVAILABLE 0
#endif

##################################################
# Utility Macros
##################################################

#ifdef MONITORING_CPP17_MODE
    #define MONITORING_IF_CPP20(code)
    #define MONITORING_IF_CPP17(code) code
    #define MONITORING_CONSTEXPR_CPP20
    #define MONITORING_CONSTEVAL_CPP20
#else
    #define MONITORING_IF_CPP20(code) code
    #define MONITORING_IF_CPP17(code)
    #define MONITORING_CONSTEXPR_CPP20 constexpr
    #define MONITORING_CONSTEVAL_CPP20 consteval
#endif

#ifdef MONITORING_HAS_CONCEPTS
    #define MONITORING_REQUIRES(condition) requires condition
    #define MONITORING_CONCEPT_CHECK(concept_name, type) concept_name<type>
#else
    #define MONITORING_REQUIRES(condition)
    #define MONITORING_CONCEPT_CHECK(concept_name, type) std::enable_if_t<monitoring_compat::concept_name<type>>* = nullptr
#endif

// Performance optimization hints
#if defined(__GNUC__) || defined(__clang__)
    #define MONITORING_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define MONITORING_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define MONITORING_LIKELY(x)   (x)
    #define MONITORING_UNLIKELY(x) (x)
#endif

#if defined(_MSC_VER)
    #define MONITORING_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define MONITORING_FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define MONITORING_FORCE_INLINE inline
#endif

#ifdef __has_cpp_attribute
    #if __has_cpp_attribute(nodiscard) >= 201603L
        #define MONITORING_NODISCARD [[nodiscard]]
    #else
        #define MONITORING_NODISCARD
    #endif
#else
    #define MONITORING_NODISCARD
#endif
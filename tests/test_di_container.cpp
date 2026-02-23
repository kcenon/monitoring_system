// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file test_di_container.cpp
 * @brief Unit tests for monitoring_system DI container integration.
 *
 * Tests validate that monitoring_system types work correctly with
 * common_system's service_container, covering registration, resolution,
 * lifetime management, dependency injection, scoping, and thread safety.
 *
 * Part of kcenon/common_system#368
 */

#include <gtest/gtest.h>

#include <kcenon/common/di/service_container.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::common::di;

/**
 * Test interfaces and implementations
 */
class IService {
public:
    virtual ~IService() = default;
    virtual std::string get_name() const = 0;
};

class ServiceA : public IService {
private:
    static std::atomic<int> instance_count_;
    int id_;

public:
    ServiceA() : id_(++instance_count_) {}
    ~ServiceA() override { --instance_count_; }

    std::string get_name() const override
    {
        return "ServiceA_" + std::to_string(id_);
    }

    int get_id() const { return id_; }

    static int get_instance_count() { return instance_count_.load(); }

    static void reset_count() { instance_count_ = 0; }
};

std::atomic<int> ServiceA::instance_count_{0};

class ServiceB : public IService {
private:
    std::shared_ptr<ServiceA> service_a_;

public:
    explicit ServiceB(std::shared_ptr<ServiceA> a) : service_a_(std::move(a)) {}

    std::string get_name() const override
    {
        return "ServiceB_with_" + service_a_->get_name();
    }

    std::shared_ptr<ServiceA> get_dependency() const { return service_a_; }
};

/**
 * Test fixture for DI container tests
 */
class DIContainerTest : public ::testing::Test {
protected:
    service_container container_;

    void SetUp() override { ServiceA::reset_count(); }

    void TearDown() override
    {
        container_.clear();
        ServiceA::reset_count();
    }
};

/**
 * Test basic service registration and resolution with transient lifetime
 */
TEST_F(DIContainerTest, RegisterAndResolveTransient)
{
    auto result = container_.register_simple_factory<IService>(
        []() { return std::make_shared<ServiceA>(); }, service_lifetime::transient);

    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(container_.is_registered<IService>());

    // Resolve service multiple times
    auto service1_result = container_.resolve<IService>();
    ASSERT_TRUE(service1_result.is_ok());
    auto service1 = service1_result.value();
    EXPECT_NE(service1, nullptr);
    EXPECT_EQ(service1->get_name(), "ServiceA_1");

    auto service2_result = container_.resolve<IService>();
    ASSERT_TRUE(service2_result.is_ok());
    auto service2 = service2_result.value();
    EXPECT_NE(service2, nullptr);
    EXPECT_EQ(service2->get_name(), "ServiceA_2");

    // Transient services should be different instances
    EXPECT_NE(service1, service2);
    EXPECT_EQ(ServiceA::get_instance_count(), 2);
}

/**
 * Test singleton lifetime
 */
TEST_F(DIContainerTest, RegisterAndResolveSingleton)
{
    auto result = container_.register_simple_factory<IService>(
        []() { return std::make_shared<ServiceA>(); }, service_lifetime::singleton);

    ASSERT_TRUE(result.is_ok());

    // Resolve multiple times
    auto service1_result = container_.resolve<IService>();
    auto service2_result = container_.resolve<IService>();

    ASSERT_TRUE(service1_result.is_ok());
    ASSERT_TRUE(service2_result.is_ok());

    auto service1 = service1_result.value();
    auto service2 = service2_result.value();

    // Singleton services should be the same instance
    EXPECT_EQ(service1, service2);
    EXPECT_EQ(ServiceA::get_instance_count(), 1);
    EXPECT_EQ(service1->get_name(), "ServiceA_1");
    EXPECT_EQ(service2->get_name(), "ServiceA_1");
}

/**
 * Test direct instance registration
 */
TEST_F(DIContainerTest, RegisterSingletonInstance)
{
    auto instance = std::make_shared<ServiceA>();
    auto initial_name = instance->get_name();

    auto result = container_.register_instance<IService>(instance);
    ASSERT_TRUE(result.is_ok());

    // Resolve should return the same instance
    auto resolved_result = container_.resolve<IService>();
    ASSERT_TRUE(resolved_result.is_ok());
    auto resolved = resolved_result.value();

    EXPECT_EQ(resolved, instance);
    EXPECT_EQ(resolved->get_name(), initial_name);
}

/**
 * Test service with dependencies resolved through the container
 */
TEST_F(DIContainerTest, ServiceWithDependencies)
{
    // Register dependency as singleton
    container_.register_simple_factory<ServiceA>(
        []() { return std::make_shared<ServiceA>(); }, service_lifetime::singleton);

    // Register service that depends on ServiceA, using container to resolve
    container_.register_factory<ServiceB>(
        [](IServiceContainer& c) {
            auto dep_result = c.resolve<ServiceA>();
            if (dep_result.is_err()) {
                throw std::runtime_error("Failed to resolve dependency");
            }
            return std::make_shared<ServiceB>(dep_result.value());
        },
        service_lifetime::transient);

    // Resolve service with dependencies
    auto service_result = container_.resolve<ServiceB>();
    ASSERT_TRUE(service_result.is_ok());
    auto service = service_result.value();

    EXPECT_NE(service, nullptr);
    auto dependency = service->get_dependency();
    EXPECT_NE(dependency, nullptr);

    // Dependency should be the same singleton
    auto dep_result = container_.resolve<ServiceA>();
    ASSERT_TRUE(dep_result.is_ok());
    EXPECT_EQ(dependency, dep_result.value());
}

/**
 * Test scoped container
 */
TEST_F(DIContainerTest, ScopedContainer)
{
    // Register singleton in root container
    container_.register_simple_factory<IService>(
        []() { return std::make_shared<ServiceA>(); }, service_lifetime::singleton);

    // Create scoped container
    auto scope = container_.create_scope();
    ASSERT_NE(scope, nullptr);

    // Scoped container should inherit parent registrations
    EXPECT_TRUE(scope->is_registered<IService>());

    // Resolve in scope should work
    auto service_result = scope->resolve<IService>();
    ASSERT_TRUE(service_result.is_ok());
    EXPECT_NE(service_result.value(), nullptr);
}

/**
 * Test error handling - resolve unregistered service
 */
TEST_F(DIContainerTest, ResolveUnregisteredService)
{
    auto result = container_.resolve<IService>();
    EXPECT_TRUE(result.is_err());
}

/**
 * Test resolve_or_null for unregistered service returns nullptr
 */
TEST_F(DIContainerTest, ResolveOrNullUnregistered)
{
    auto ptr = container_.resolve_or_null<IService>();
    EXPECT_EQ(ptr, nullptr);
}

/**
 * Test clear functionality
 */
TEST_F(DIContainerTest, ClearContainer)
{
    container_.register_simple_factory<IService>(
        []() { return std::make_shared<ServiceA>(); }, service_lifetime::singleton);

    EXPECT_TRUE(container_.is_registered<IService>());

    container_.clear();

    EXPECT_FALSE(container_.is_registered<IService>());

    auto resolve_result = container_.resolve<IService>();
    EXPECT_TRUE(resolve_result.is_err());
}

/**
 * Test unregister specific service
 */
TEST_F(DIContainerTest, UnregisterService)
{
    container_.register_simple_factory<IService>(
        []() { return std::make_shared<ServiceA>(); }, service_lifetime::singleton);

    EXPECT_TRUE(container_.is_registered<IService>());

    auto result = container_.unregister<IService>();
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(container_.is_registered<IService>());
}

/**
 * Test duplicate registration fails
 */
TEST_F(DIContainerTest, DuplicateRegistrationFails)
{
    auto result1 = container_.register_simple_factory<IService>(
        []() { return std::make_shared<ServiceA>(); }, service_lifetime::singleton);
    ASSERT_TRUE(result1.is_ok());

    auto result2 = container_.register_simple_factory<IService>(
        []() { return std::make_shared<ServiceA>(); }, service_lifetime::singleton);
    EXPECT_TRUE(result2.is_err());
}

/**
 * Test thread safety of singleton resolution
 */
TEST_F(DIContainerTest, ThreadSafety)
{
    container_.register_simple_factory<IService>(
        []() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return std::make_shared<ServiceA>();
        },
        service_lifetime::singleton);

    // Resolve from multiple threads
    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<IService>> results(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(
            [this, &results, i]()
            {
                auto result = container_.resolve<IService>();
                if (result.is_ok()) {
                    results[i] = result.value();
                }
            });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All threads should get the same singleton instance
    auto first = results[0];
    EXPECT_NE(first, nullptr);

    for (const auto& result : results) {
        EXPECT_EQ(result, first);
    }

    // Only one instance should have been created
    EXPECT_EQ(ServiceA::get_instance_count(), 1);
}

/**
 * Test registered_services returns descriptors
 */
TEST_F(DIContainerTest, RegisteredServicesDescriptors)
{
    container_.register_simple_factory<IService>(
        []() { return std::make_shared<ServiceA>(); }, service_lifetime::singleton);

    container_.register_simple_factory<ServiceA>(
        []() { return std::make_shared<ServiceA>(); }, service_lifetime::transient);

    auto services = container_.registered_services();
    EXPECT_GE(services.size(), 2u);
}

/**
 * Test freeze prevents new registrations
 */
TEST_F(DIContainerTest, FreezePreventsRegistration)
{
    container_.register_simple_factory<IService>(
        []() { return std::make_shared<ServiceA>(); }, service_lifetime::singleton);

    container_.freeze();
    EXPECT_TRUE(container_.is_frozen());

    auto result = container_.register_simple_factory<ServiceA>(
        []() { return std::make_shared<ServiceA>(); }, service_lifetime::transient);

    EXPECT_TRUE(result.is_err());
}

/**
 * Test freeze still allows resolution
 */
TEST_F(DIContainerTest, FreezeAllowsResolution)
{
    container_.register_simple_factory<IService>(
        []() { return std::make_shared<ServiceA>(); }, service_lifetime::singleton);

    container_.freeze();

    auto result = container_.resolve<IService>();
    EXPECT_TRUE(result.is_ok());
    EXPECT_NE(result.value(), nullptr);
}

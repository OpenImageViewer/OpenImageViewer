#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <LLUtils/Event.h>

#include <atomic>
#include <latch>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

static_assert(std::is_same_v<LLUtils::Event<void(int)>, LLUtils::Event<void(int), false>>);

TEMPLATE_TEST_CASE("Both event modes own and transfer callback registrations", "[event][LLUtils]", std::false_type,
                   std::true_type)
{
    using Event = LLUtils::Event<void(int), TestType::value>;
    Event event;
    typename Event::Subscription empty;
    STATIC_REQUIRE(noexcept(static_cast<bool>(empty)));
    CHECK_FALSE(empty);
    int first = 0, second = 0;
    {
        auto subscription = event.Subscribe([&](int value) { first += value; });
        auto moved        = std::move(subscription);
        CHECK_FALSE(subscription);
        CHECK(moved);
        empty = std::move(moved);
        CHECK_FALSE(moved);
        CHECK(empty);
        auto replacement = event.Subscribe([&](int value) { second += value; });
        replacement      = std::move(empty);
        CHECK_FALSE(empty);
        CHECK(replacement);
        event.Raise(3);
        CHECK(first == 3);
        CHECK(second == 0);
        replacement.Unsubscribe();
        replacement.Unsubscribe();
        CHECK_FALSE(replacement);
        event.Raise(5);
        CHECK(first == 3);
        auto scoped = event.Subscribe([&](int value) { second += value; });
        event.Raise(7);
    }
    event.Raise(11);
    CHECK(second == 7);
}

TEMPLATE_TEST_CASE("Both event modes propagate listener failures and remain usable", "[event][LLUtils]",
                   std::false_type, std::true_type)
{
    LLUtils::Event<void(), TestType::value> event;
    int calls      = 0;
    auto throwing  = event.Subscribe([] { throw std::runtime_error("listener failure"); });
    auto surviving = event.Subscribe([&] { ++calls; });
    CHECK_THROWS_AS(event.Raise(), std::runtime_error);
    CHECK(calls == 0);
    throwing.Unsubscribe();
    event.Raise();
    CHECK(calls == 1);
}

TEMPLATE_TEST_CASE("Both event modes recheck the dispatch predicate after callbacks", "[event][LLUtils]",
                   std::false_type, std::true_type)
{
    LLUtils::Event<void(), TestType::value> event;
    bool active = true;
    int first = 0, second = 0;
    auto one = event.Subscribe(
        [&]
        {
            ++first;
            active = false;
        });
    auto two = event.Subscribe([&] { ++second; });
    event.RaiseWhile([&] { return active; });
    CHECK(first == 1);
    CHECK(second == 0);
    event.RaiseWhile([&] { return active; });
    CHECK(first == 1);
    CHECK_THROWS_AS(event.RaiseWhile([]() -> bool { throw std::runtime_error("predicate failure"); }),
                    std::runtime_error);
    event.Raise();
    CHECK(first == 2);
    CHECK(second == 1);
}

TEST_CASE("Thread-safe event callbacks can change registrations without holding the registry lock", "[event]")
{
    LLUtils::Event<void(), true> event;
    LLUtils::Event<void(), true>::Subscription second, added;
    std::vector<int> order;
    auto first = event.Subscribe(
        [&]
        {
            order.push_back(1);
            second.Unsubscribe();
            if (!added)
                added = event.Subscribe([&] { order.push_back(3); });
        });
    second = event.Subscribe([&] { order.push_back(2); });
    event.Raise();
    CHECK(order == std::vector<int>{1, 2});  // The removed callback was already selected.
    order.clear();
    event.Raise();
    CHECK(order == std::vector<int>{1, 3});
}

TEST_CASE("Thread-safe events allow nested dispatch without exception-specific recursion policy", "[event]")
{
    LLUtils::Event<void(), true> event;
    int first = 0, second = 0;
    auto one = event.Subscribe(
        [&]
        {
            if (++first == 1)
                event.Raise();
        });
    auto two = event.Subscribe([&] { ++second; });
    event.Raise();
    CHECK(first == 2);
    CHECK(second == 2);
}

TEST_CASE("Thread-safe events retain callbacks in flight after unsubscription", "[event][concurrency]")
{
    LLUtils::Event<void(), true> event;
    std::latch entered(1), resume(1);
    auto first = event.Subscribe(
        [&]
        {
            entered.count_down();
            resume.wait();
        });
    auto state                       = std::make_shared<unsigned>(0);
    std::weak_ptr<unsigned> lifetime = state;
    unsigned calls                   = 0;
    auto second                      = event.Subscribe(
        [state, &calls]
        {
            ++*state;
            ++calls;
        });
    std::jthread worker([&] { event.Raise(); });
    entered.wait();
    first.Unsubscribe();
    second.Unsubscribe();
    state.reset();
    CHECK_FALSE(lifetime.expired());
    resume.count_down();
    worker.join();
    CHECK(calls == 1);
    CHECK(lifetime.expired());
    event.Raise();
    CHECK(calls == 1);
}

TEST_CASE("Thread-safe event registration overlaps concurrent notifications", "[event][concurrency]")
{
    LLUtils::Event<void(), true> event;
    std::atomic<unsigned> calls{};
    auto persistent               = event.Subscribe([&] { ++calls; });
    constexpr unsigned iterations = 300;
    const auto raise              = [&]
    {
        for (unsigned i = 0; i < iterations; ++i)
            event.Raise();
    };
    std::jthread first(raise), second(raise);
    for (unsigned i = 0; i < iterations; ++i)
    {
        auto subscription = event.Subscribe([] {});
    }
    first.join();
    second.join();
    CHECK(calls == 2 * iterations);
}

TEST_CASE("Thread-safe event reference arguments are borrowed during dispatch", "[event]")
{
    LLUtils::Event<void(const std::unique_ptr<int>&), true> event;
    auto value                           = std::make_unique<int>(42);
    const std::unique_ptr<int>* observed = nullptr;
    auto subscription                    = event.Subscribe([&](const auto& argument) { observed = &argument; });
    event.Raise(value);
    CHECK(observed == &value);
    CHECK(*value == 42);
}

TEST_CASE("Unsubscribe releases callback captures after unlocking the thread-safe event", "[event]")
{
    LLUtils::Event<void(), true> event;
    int calls = 0;
    LLUtils::Event<void(), true>::Subscription added;
    struct Capture
    {
        LLUtils::Event<void(), true>& event;
        int& calls;
        LLUtils::Event<void(), true>::Subscription& added;
        ~Capture()
        {
            added = event.Subscribe([&value = calls] { ++value; });
        }
    };
    auto capture      = std::make_shared<Capture>(event, calls, added);
    auto subscription = event.Subscribe([capture] {});
    capture.reset();
    subscription.Unsubscribe();
    event.Raise();
    CHECK(calls == 1);
}

namespace
{
    template <class Event>
    concept HasUnownedRegistration = requires(Event& event, typename Event::Func callback) { event.Add(callback); };
    template <class Event>
    concept HasCallbackRemoval = requires(Event& event, typename Event::Func callback) { event.Remove(callback); };

    static_assert(!HasUnownedRegistration<LLUtils::Event<void()>>);
    static_assert(!HasUnownedRegistration<LLUtils::Event<void(), true>>);
    static_assert(!HasCallbackRemoval<LLUtils::Event<void()>>);
    static_assert(!HasCallbackRemoval<LLUtils::Event<void(), true>>);
}  // namespace

TEMPLATE_TEST_CASE("Subscriptions distinguish registrations of the same raw function", "[event]", std::false_type,
                   std::true_type)
{
    LLUtils::Event<void(int*), TestType::value> event;
    const auto matching = +[](int* value) { ++*value; };
    const auto other    = +[](int* value) { *value += 10; };
    auto first          = event.Subscribe(matching);
    auto duplicate      = event.Subscribe(matching);
    auto retained       = event.Subscribe(other);
    int calls           = 0;
    event.Raise(&calls);
    CHECK(calls == 12);
    first.Unsubscribe();
    calls = 0;
    event.Raise(&calls);
    CHECK(calls == 11);
    duplicate.Unsubscribe();
    calls = 0;
    event.Raise(&calls);
    CHECK(calls == 10);
    retained.Unsubscribe();
    calls = 0;
    event.Raise(&calls);
    CHECK(calls == 0);
}

TEMPLATE_TEST_CASE("Subscriptions distinguish copies of a captured callback", "[event]", std::false_type,
                   std::true_type)
{
    using Event = LLUtils::Event<void(), TestType::value>;
    Event event;
    int calls                           = 0;
    const typename Event::Func callback = [&] { ++calls; };
    auto first                          = event.Subscribe(callback);
    auto duplicate                      = event.Subscribe(callback);
    event.Raise();
    CHECK(calls == 2);
    first.Unsubscribe();
    event.Raise();
    CHECK(calls == 3);
    duplicate.Unsubscribe();
    event.Raise();
    CHECK(calls == 3);
}

TEST_CASE("Event subscription can unsubscribe while event is being raised", "[event][LLUtils]")
{
    LLUtils::Event<void()> event;
    LLUtils::Event<void()>::Subscription skippedSubscription;
    LLUtils::Event<void()>::Subscription selfSubscription;
    std::vector<int> order;
    int selfCount = 0;

    auto firstSubscription = event.Subscribe(
        [&]
        {
            order.push_back(1);
            skippedSubscription.Unsubscribe();
        });
    skippedSubscription = event.Subscribe([&] { order.push_back(2); });
    selfSubscription    = event.Subscribe(
        [&]
        {
            ++selfCount;
            selfSubscription.Unsubscribe();
        });

    event.Raise();

    REQUIRE(order == std::vector<int>{1});
    REQUIRE(selfCount == 1);

    order.clear();
    event.Raise();

    REQUIRE(order == std::vector<int>{1});
    REQUIRE(selfCount == 1);
}

TEST_CASE("Event subscription handles nested raises without compacting early", "[event][LLUtils]")
{
    LLUtils::Event<void()> event;
    LLUtils::Event<void()>::Subscription skippedSubscription;
    int firstCount  = 0;
    int secondCount = 0;

    auto firstSubscription = event.Subscribe(
        [&]
        {
            ++firstCount;

            if (firstCount == 1)
            {
                skippedSubscription.Unsubscribe();
                event.Raise();
            }
        });
    skippedSubscription = event.Subscribe([&] { ++secondCount; });

    event.Raise();

    firstSubscription.Unsubscribe();
    event.Raise();

    REQUIRE(firstCount == 2);
    REQUIRE(secondCount == 0);
}

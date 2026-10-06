#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <LLUtils/ExceptionFormatter.h>
#include <LLUtils/Thread.h>
#include "HandledException.h"
#include <LLUtils/Logging/Logger.h>
#include <LLUtils/Logging/LogFileSink.h>
#include <chrono>
#include <filesystem>
#include <fstream>

#include <atomic>
#include <latch>
#include <thread>
#include <type_traits>

namespace
{
    using Exception = LLUtils::Exception;
    using Code      = Exception::ErrorCode;
    using Catch::Matchers::ContainsSubstring;

    Exception Diagnostic(std::string message)
    {
        return Exception(Code::RuntimeError, "Diagnostic", std::move(message), false, Exception::Mode::Exception, 64);
    }

    [[noreturn]] void ThrowSystemCode()
    {
#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32
        SetLastError(ERROR_ACCESS_DENIED);
        LL_EXCEPTION_SYSTEM_ERROR((SetLastError(ERROR_FILE_NOT_FOUND), "system failure"));
#else
        errno = EACCES;
        LL_EXCEPTION_SYSTEM_ERROR((errno = ENOENT, "system failure"));
#endif
    }
}  // namespace

TEST_CASE("Exception snapshots retain UTF-8 data and do not notify on copies", "[exception]")
{
    static_assert(std::is_nothrow_copy_constructible_v<Exception>);
    static_assert(std::is_nothrow_move_constructible_v<Exception>);
    const Exception::EventArgs* observed = nullptr;
    unsigned notifications               = 0;
    auto subscription                    = Exception::OnException.Subscribe(
        [&](const Exception::EventArgs& args)
        {
            observed = &args;
            ++notifications;
        });
    const std::string message = "image \xe2\x82\xac \xf0\x9f\x8c\x8d";
    auto exception            = Diagnostic(message);
    REQUIRE(observed == &exception.GetDetails());
    REQUIRE(exception.what() == message);
    const auto copy = exception;
    auto moved      = std::move(exception);
    CHECK(copy.what() == message);
    CHECK(moved.what() == message);
    CHECK(exception.what() == message);
    CHECK(copy.GetDetails().source.line() != 0);
    try
    {
        throw copy;
    }
    catch (const Exception& error)
    {
        CHECK(error.what() == message);
    }
    CHECK(notifications == 1);
}

TEST_CASE("Exception error codes have complete labels and a safe invalid fallback", "[exception]")
{
    constexpr std::array<std::string_view, 11> labels{"Unspecified",    "Unknown",         "Corrupted value",
                                                      "Logic error",    "Runtime error",   "Duplicate item",
                                                      "Bad parameters", "Not implemented", "Invalid state",
                                                      "System error",   "Not found"};
    STATIC_REQUIRE(labels.size() == static_cast<std::size_t>(Code::Count));
    for (std::size_t index = 0; index < labels.size(); ++index)
        CHECK(Exception::ExceptionErrorCodeToString(static_cast<Code>(index)) == labels[index]);
    CHECK(Exception::ExceptionErrorCodeToString(static_cast<Code>(-1)) == "Unspecified");
}

TEST_CASE("Exception observation contains failures and recursive notification", "[exception]")
{
    unsigned notifications = 0;
    auto throwing          = Exception::OnException.Subscribe(
        [](const Exception::EventArgs&)
        {
            auto nested = Diagnostic("observer failure");
            throw nested;
        });
    auto surviving = Exception::OnException.Subscribe([&](const Exception::EventArgs&) { ++notifications; });
    REQUIRE_NOTHROW(Diagnostic("original"));
    CHECK(notifications == 1);
    surviving.Unsubscribe();
    REQUIRE_NOTHROW(Diagnostic("another"));
    CHECK(notifications == 1);
}

TEST_CASE("Unsubscribe retains already snapshotted callback state", "[exception][lifetime]")
{
    std::latch entered(1), resume(1);
    auto blocker = Exception::OnException.Subscribe(
        [&](const Exception::EventArgs&)
        {
            entered.count_down();
            resume.wait();
        });
    auto state                       = std::make_shared<unsigned>(0);
    std::weak_ptr<unsigned> lifetime = state;
    unsigned notifications           = 0;
    auto subscription                = Exception::OnException.Subscribe(
        [state, &notifications](const Exception::EventArgs&)
        {
            ++*state;
            ++notifications;
        });
    std::jthread worker([] { Exception::OnException.Raise({}); });
    entered.wait();
    subscription.Unsubscribe();
    state.reset();
    CHECK_FALSE(lifetime.expired());
    resume.count_down();
    worker.join();
    CHECK(notifications == 1);
    CHECK(lifetime.expired());
}

TEST_CASE("Exception raises and subscription changes can overlap", "[exception][concurrency]")
{
    std::atomic<unsigned> notifications{};
    auto persistent          = Exception::OnException.Subscribe([&](const Exception::EventArgs&) { ++notifications; });
    constexpr unsigned count = 200;
    std::jthread first(
        []
        {
            for (unsigned i = 0; i < count; ++i)
                Exception::OnException.Raise({});
        });
    std::jthread second(
        []
        {
            for (unsigned i = 0; i < count; ++i)
                Exception::OnException.Raise({});
        });
    for (unsigned i = 0; i < count; ++i)
    {
        auto temporary = Exception::OnException.Subscribe([](const Exception::EventArgs&) {});
    }
    first.join();
    second.join();
    CHECK(notifications == 2 * count);
}

TEST_CASE("Contextual rethrow retains foreign cause lifetime and type", "[exception]")
{
    std::exception_ptr outer;
    try
    {
        try
        {
            throw std::system_error(std::make_error_code(std::errc::permission_denied), "original cause");
        }
        catch (...)
        {
            Exception::Rethrow(Code::RuntimeError, "outer context");
        }
    }
    catch (...)
    {
        outer = std::current_exception();
    }
    const auto report = LLUtils::FormatException(outer);
    CHECK(report.find("outer context") < report.find("original cause"));
    CHECK_THAT(report, ContainsSubstring("generic:13"));
    try
    {
        std::rethrow_exception(outer);
    }
    catch (const Exception& error)
    {
        REQUIRE(error.GetDetails().cause);
        CHECK_THROWS_AS(std::rethrow_exception(error.GetDetails().cause), std::system_error);
    }
    CHECK_THROWS_AS(Exception::Rethrow(Code::RuntimeError, "no catch"), Exception);
    try
    {
        Exception::Rethrow(Code::RuntimeError, "no catch");
    }
    catch (const Exception& error)
    {
        CHECK(error.GetDetails().errorCode == Code::InvalidState);
    }
}

TEST_CASE("Contextual rethrow preserves LLUtils and unknown causes", "[exception]")
{
    for (const bool standard : {false, true})
    {
        try
        {
            try
            {
                if (standard)
                    throw Diagnostic("inner LLUtils");
                throw 42;
            }
            catch (...)
            {
                Exception::Rethrow(Code::RuntimeError, "outer");
            }
        }
        catch (const Exception& error)
        {
            const auto report = LLUtils::FormatException(error.GetDetails());
            if (standard)
            {
                CHECK_THAT(report, ContainsSubstring("inner LLUtils"));
                CHECK_THROWS_AS(std::rethrow_exception(error.GetDetails().cause), Exception);
            }
            else
            {
                CHECK_THAT(report, ContainsSubstring("Unknown/non-standard"));
                try
                {
                    std::rethrow_exception(error.GetDetails().cause);
                }
                catch (int value)
                {
                    CHECK(value == 42);
                }
            }
        }
    }
}

TEST_CASE("System error capture precedes description evaluation", "[exception]")
{
    try
    {
        ThrowSystemCode();
    }
    catch (const Exception& error)
    {
#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32
        CHECK(error.GetDetails().systemError == std::error_code(ERROR_ACCESS_DENIED, std::system_category()));
#else
        CHECK(error.GetDetails().systemError == std::error_code(EACCES, std::generic_category()));
#endif
        CHECK_THAT(error.GetDetails().functionName, ContainsSubstring("ThrowSystemCode"));
        CHECK(error.GetDetails().source.line() != 0);
    }
    const auto error = Exception::FromSystemError({12345, std::generic_category()}, "explicit");
    CHECK(error.GetDetails().systemError.value() == 12345);
    const auto zero = Exception::FromSystemError({0, std::system_category()}, "missing OS error");
    CHECK_THAT(LLUtils::FormatException(zero.GetDetails()), ContainsSubstring("system:0"));
}

TEST_CASE("Exception reports bound huge fields without breaking Unicode", "[exception]")
{
    std::string huge;
    for (unsigned index = 0; index < 20000; ++index)
        huge += "\xf0\x9f\x8c\x8d";
    auto error        = Diagnostic(std::move(huge));
    const auto report = LLUtils::FormatException(error.GetDetails());
    CHECK(report.size() <= LLUtils::ExceptionFormatting::MaxReportBytes);
    CHECK_THAT(report, ContainsSubstring("[report truncated]"));
    CHECK_NOTHROW(LLUtils::StringUtility::ToWString(report));
    error = Diagnostic(std::string("bad \xff text"));
    CHECK_THAT(LLUtils::FormatException(error.GetDetails()), ContainsSubstring("[invalid text]"));
    CHECK_THAT(LLUtils::FormatException(std::exception_ptr{}), ContainsSubstring("without an active exception"));
}

TEST_CASE("Standard nested chains are bounded and empty nested pointers are safe", "[exception]")
{
    std::exception_ptr cause = std::make_exception_ptr(std::runtime_error("root"));
    for (unsigned index = 0; index < 25; ++index)
    {
        try
        {
            try
            {
                std::rethrow_exception(cause);
            }
            catch (...)
            {
                // An explicit nested type also tests the standard interface without the
                // MSVC STL forwarding-constructor wrapper used by throw_with_nested.
                struct NestedError
                    : std::runtime_error
                    , std::nested_exception
                {
                    explicit NestedError(const char* message) : std::runtime_error(message) {}
                };
                throw NestedError("context");
            }
        }
        catch (...)
        {
            cause = std::current_exception();
        }
    }
    CHECK_THAT(LLUtils::FormatException(cause), ContainsSubstring("[cause depth limit reached]"));
    struct EmptyNested
        : std::exception
        , std::nested_exception
    {
    };
    CHECK_NOTHROW(LLUtils::FormatException(std::make_exception_ptr(EmptyNested{})));
}

TEST_CASE("Stack limits handle zero frames and excessive skip counts", "[exception]")
{
    LLUtils::PlatformUtility::StackTrace trace(3);
    trace[0].name = LLUTILS_TEXT("first");
    trace[1].name = LLUTILS_TEXT("second");
    trace[2].name = LLUTILS_TEXT("third");
    CHECK(Exception::FormatStackTrace(trace, 0).empty());
    const auto one = LLUtils::StringUtility::ToAString(Exception::FormatStackTrace(trace, 1));
    CHECK_THAT(one, ContainsSubstring("first"));
    CHECK(one.find("second") == std::string::npos);
    CHECK(LLUtils::PlatformUtility::GetCallStack(100000).empty());
    CHECK(LLUtils::PlatformUtility::GetCallStack(-1).size() <= 64);
}

TEST_CASE("Handled callback reporting stays nonfatal and avoids duplicate LLUtils events", "[exception]")
{
    unsigned notifications = 0;
    auto listener          = Exception::OnException.Subscribe(
        [&](const Exception::EventArgs&)
        {
            ++notifications;
            throw 5;
        });
    std::exception_ptr exception;
    try
    {
        throw Diagnostic("handled");
    }
    catch (...)
    {
        exception = std::current_exception();
    }
    CHECK(notifications == 1);
    CHECK_NOTHROW(OIV::ReportHandledException(exception));
    CHECK(notifications == 1);
    CHECK_NOTHROW(OIV::ReportHandledException(std::make_exception_ptr(std::runtime_error("handled standard"))));
    CHECK(notifications == 2);
    CHECK_NOTHROW(OIV::ReportHandledException({}));
    CHECK(notifications == 2);
}

TEST_CASE("Worker launch preserves ownership and inherited termination policy", "[exception][concurrency]")
{
    const auto expected = std::get_terminate();
    bool inherited      = false;
    auto value          = std::make_unique<int>(17);
    int result          = 0;
    auto worker         = LLUtils::StartThread(
        [&](std::unique_ptr<int> input)
        {
            inherited = std::get_terminate() == expected;
            result    = *input;
        },
        std::move(value));
    worker.join();
    CHECK_FALSE(value);
    CHECK(inherited);
    CHECK(result == 17);
}

TEST_CASE("Logger drains concurrent UTF-8 output and stale exception snapshots release sinks", "[exception][lifetime]")
{
    const auto folder = std::filesystem::temp_directory_path() /
                        ("oiv-exception-log-" +
                         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(folder);
    struct Cleanup
    {
        std::filesystem::path path;
        ~Cleanup()
        {
            if (LLUtils::Logger::IsActive())
                LLUtils::Logger::Shutdown();
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    } cleanup{folder};
    std::latch entered(1), resume(1);
    auto blocker = Exception::OnException.Subscribe(
        [&](const auto&)
        {
            entered.count_down();
            resume.wait();
        });
    auto sink = std::make_shared<LLUtils::FileLogSink>(LLUtils::LogFileOptions{.path = folder / "exception"});
    std::weak_ptr<LLUtils::FileLogSink> lifetime = sink;
    LLUtils::LoggerOptions options;
    options.format.pattern = "{message}";
    options.sinks.push_back({std::move(sink)});
    REQUIRE(LLUtils::Logger::Initialize(std::move(options)) == LLUtils::LogResult::Success);
    const auto category = LLUtils::Logger::RegisterCategory("Test.Exception");
    std::jthread notification([] { Exception::OnException.Raise(Exception::EventArgs{.description = "late"}); });
    entered.wait();
    blocker.Unsubscribe();
    auto secondWriter = LLUtils::StartThread(
        [category]
        {
            for (unsigned index = 0; index < 20; ++index)
                LL_LOG(category, LLUtils::LogLevel::Info, "image \xf0\x9f\x8c\x8d");
        });
    secondWriter.join();
    CHECK(LLUtils::Logger::Shutdown() == LLUtils::LogResult::Success);
    CHECK(lifetime.expired());
    resume.count_down();
    notification.join();
    std::string content;
    for (const auto& entry : std::filesystem::directory_iterator(folder))
        if (entry.path().extension() == ".log")
        {
            std::ifstream stream(entry.path(), std::ios::binary);
            content.append(std::istreambuf_iterator<char>{stream}, {});
        }
    std::string expected;
    for (unsigned index = 0; index < 20; ++index)
        expected += "image \xf0\x9f\x8c\x8d\r\n";
    CHECK(content == expected);
}

TEST_CASE("Exception thread identity survives copies moves and plain rethrows", "[exception][thread-context]")
{
    Exception::RegisterMainThread();
    Exception::RegisterMainThread();
#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32
    const std::uint64_t nativeId = ::GetCurrentThreadId();
#else
    const std::uint64_t nativeId = static_cast<std::uint64_t>(::gettid());
#endif
    CHECK(LLUtils::PlatformUtility::GetCurrentThreadId() == nativeId);
    auto original    = Diagnostic("main origin");
    const auto copy  = original;
    const auto moved = std::move(original);
    for (const auto* error : std::array<const Exception*, 3>{&original, &copy, &moved})
    {
        CHECK(error->GetDetails().threadId == nativeId);
        CHECK(error->GetDetails().threadRole == Exception::ThreadRole::Main);
    }
    const auto expected = "Thread: " + std::to_string(nativeId) + " (main)\nSource:";
    CHECK_THAT(LLUtils::FormatException(copy.GetDetails()), ContainsSubstring(expected));
    try
    {
        try
        {
            throw copy;
        }
        catch (const Exception& error)
        {
            CHECK(&error.GetDetails() == &copy.GetDetails());
            throw;
        }
    }
    catch (const Exception& error)
    {
        CHECK(error.GetDetails().threadId == nativeId);
        CHECK(error.GetDetails().threadRole == Exception::ThreadRole::Main);
        CHECK_FALSE(error.GetDetails().cause);
    }
    CHECK_THAT(LLUtils::FormatException(Exception::EventArgs{}),
               ContainsSubstring("Thread: unavailable (unknown)\nSource:"));
}

TEST_CASE("Main-thread reporting and wrapping retain a worker exception's origin",
          "[exception][thread-context][concurrency]")
{
    Exception::RegisterMainThread();
    const auto mainId      = LLUtils::PlatformUtility::GetCurrentThreadId();
    std::uint64_t workerId = 0, observedId = 0;
    auto observedRole = Exception::ThreadRole::Unknown;
    auto subscription = Exception::OnException.Subscribe(
        [&](const Exception::EventArgs& details)
        {
            observedId   = details.threadId;
            observedRole = details.threadRole;
        });
    std::exception_ptr original;
    // A plain std::thread proves capture does not depend on the StartThread wrapper.
    std::thread worker(
        [&]
        {
            workerId = LLUtils::PlatformUtility::GetCurrentThreadId();
            try
            {
                throw Diagnostic("worker origin");
            }
            catch (...)
            {
                original = std::current_exception();
            }
        });
    worker.join();
    subscription.Unsubscribe();
    REQUIRE(original);
    REQUIRE(workerId != 0);
    REQUIRE(workerId != mainId);
    CHECK(observedId == workerId);
    CHECK(observedRole == Exception::ThreadRole::Worker);
    const auto workerLine = "Thread: " + std::to_string(workerId) + " (worker)";
    const auto mainLine   = "Thread: " + std::to_string(mainId) + " (main)";
    CHECK_THAT(LLUtils::FormatException(original), ContainsSubstring(workerLine));
    std::exception_ptr wrapped;
    try
    {
        try
        {
            std::rethrow_exception(original);
        }
        catch (...)
        {
            Exception::Rethrow(Code::RuntimeError, "main wrapper");
        }
    }
    catch (const Exception& error)
    {
        CHECK(error.GetDetails().threadId == mainId);
        CHECK(error.GetDetails().threadRole == Exception::ThreadRole::Main);
        // MSVC may copy exception objects during rethrow; the retained origin is checked below.
        CHECK(error.GetDetails().cause);
        wrapped = std::current_exception();
    }
    REQUIRE(wrapped);
    const auto report = LLUtils::FormatException(wrapped);
    REQUIRE_THAT(report, ContainsSubstring(mainLine));
    REQUIRE_THAT(report, ContainsSubstring(workerLine));
    REQUIRE_THAT(report, ContainsSubstring("Caused by:"));
    CHECK_THAT(report, ContainsSubstring("worker origin"));
    CHECK(report.find(mainLine) < report.find("Caused by:"));
    CHECK(report.find("Caused by:") < report.find(workerLine));
}

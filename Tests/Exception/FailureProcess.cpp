#include "ExceptionHandler.h"
#include "HandledException.h"
#include <LLUtils/ExceptionFormatter.h>
#include <LLUtils/Thread.h>
#include <LLUtils/Logging/Logger.h>

#include <atomic>
#include <barrier>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#ifdef _WIN32
    #include <crtdbg.h>
#endif

namespace
{
    std::atomic<int> allocationsBeforeFailure{-1};
    std::atomic_bool failureInjected{false};
    constexpr int MaxAllocationBudget = 1024;
    void FailIfRequested()
    {
        auto remaining = allocationsBeforeFailure.load();
        if (remaining >= 0 && allocationsBeforeFailure.fetch_sub(1) <= 0)
        {
            allocationsBeforeFailure = 0;
            failureInjected          = true;
            throw std::bad_alloc();
        }
    }

    [[noreturn]] void ThrowFromNoexcept() noexcept
    {
        // Keep intentional fatal behavior in this executable, never the Catch2 runner.
        const auto fail = [] { throw std::runtime_error("noexcept failure"); };
        fail();
        std::abort();
    }

#ifdef _WIN32
    constexpr DWORD TestExceptionCode = 0xe0421234;

    LONG CALLBACK ResumeTestException(EXCEPTION_POINTERS* pointers) noexcept
    {
        return pointers->ExceptionRecord->ExceptionCode == TestExceptionCode ? EXCEPTION_CONTINUE_EXECUTION
                                                                             : EXCEPTION_CONTINUE_SEARCH;
    }

    bool HandledNativeException()
    {
        // A temporary Win32 handler exercises recovery on both MSVC and MinGW without compiler-specific SEH syntax.
        const auto handler = AddVectoredExceptionHandler(1, ResumeTestException);
        if (handler == nullptr)
            return false;
        RaiseException(TestExceptionCode, 0, 0, nullptr);
        return RemoveVectoredExceptionHandler(handler) != 0;
    }
#endif
}  // namespace

// Fault injection belongs only to this subprocess. The real reporter and observer
// implementations are compiled unchanged, including the actual Windows and Linux dialogs.
void* operator new(std::size_t size)
{
    FailIfRequested();
    if (void* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size)
{
    return ::operator new(size);
}
void operator delete(void* memory) noexcept
{
    std::free(memory);
}
void operator delete[](void* memory) noexcept
{
    std::free(memory);
}
void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}
void operator delete[](void* memory, std::size_t) noexcept
{
    std::free(memory);
}

int main(int argc, char* argv[])
{
#ifndef _WIN32
    if (const auto result = OIV::RunExceptionDialog(argc, argv))
        return *result;
#endif
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    // Keep the CRT debug prompt enabled so the production reporter must suppress it.
    _set_abort_behavior(_WRITE_ABORT_MSG, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    if (argc != 2)
        return 2;
    const std::string_view scenario = argv[1];
    if (scenario == "thread-unregistered")
    {
        // A separate process guarantees that no earlier test registered the main thread.
        const LLUtils::Exception error(LLUtils::Exception::ErrorCode::RuntimeError, "before registration",
                                       "unclassified origin", false, LLUtils::Exception::Mode::Exception, 64);
        const auto id = LLUtils::PlatformUtility::GetCurrentThreadId();
        LLUtils::Exception::RegisterMainThread();
        const auto expected = "Thread: " + std::to_string(id) + " (unknown)";
        return id != 0 && error.GetDetails().threadId == id &&
                       error.GetDetails().threadRole == LLUtils::Exception::ThreadRole::Unknown &&
                       LLUtils::FormatException(error.GetDetails()).find(expected) != std::string::npos
                   ? EXIT_SUCCESS
                   : EXIT_FAILURE;
    }
    LLUtils::Exception::RegisterMainThread();
#ifdef _WIN32
    if (scenario == "abort-policy")
        _set_abort_behavior(_WRITE_ABORT_MSG | _CALL_REPORTFAULT, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    if (const char* emergency = std::getenv("OIV_TEST_EMERGENCY_LOG"))
    {
        LLUtils::LoggerOptions options;
        options.emergencyPath = emergency;
        if (LLUtils::Logger::Initialize(std::move(options)) != LLUtils::LogResult::Success)
            return 3;
    }
    const OIV::ExceptionRegistration handlers;
    try
    {
#ifdef _WIN32
        if (scenario == "abort-policy")
        {
            constexpr unsigned mask = _WRITE_ABORT_MSG | _CALL_REPORTFAULT;
            // Installing handlers alone must not change ordinary abort behavior.
            if ((_set_abort_behavior(0, 0) & mask) != mask)
                return EXIT_FAILURE;
            OIV::ReportUnhandledException(std::make_exception_ptr(std::runtime_error("abort policy check")));
            return (_set_abort_behavior(0, 0) & mask) == _CALL_REPORTFAULT ? EXIT_SUCCESS : EXIT_FAILURE;
        }
#endif
        if (scenario == "long-unicode")
        {
            std::string message = "long report begin " + std::string(255, 'x');
            message += "\xf0\x9f\x8c\x8d";  // Supplementary Unicode scalar near a conversion boundary.
            message += std::string(1800, 'x') + " long report end";
            throw std::runtime_error(message);
        }
        if (scenario == "standard")
            throw std::runtime_error("standard failure");
        if (scenario == "unknown")
            throw 42;
        if (scenario == "system")
            throw std::system_error(std::make_error_code(std::errc::permission_denied), "system failure");
        if (scenario == "llutils")
        {
            try
            {
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotFound, "original image failure");
            }
            catch (...)
            {
                LLUtils::Exception::Rethrow(LLUtils::Exception::ErrorCode::RuntimeError, "outer image context");
            }
        }
        if (scenario == "worker")
        {
            auto worker = LLUtils::StartThread([] { throw std::runtime_error("worker failure"); });
            worker.join();
        }
        if (scenario == "worker-llutils")
        {
            auto worker = LLUtils::StartThread(
                [] { LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "worker LLUtils failure"); });
            worker.join();
        }
        if (scenario == "terminate")
            std::terminate();
        if (scenario == "noexcept")
            ThrowFromNoexcept();
        if (scenario == "allocation" || scenario == "duplicate" || scenario == "concurrent")
        {
            const auto error = std::make_exception_ptr(std::runtime_error("reporter failure"));
            if (scenario == "allocation")
                allocationsBeforeFailure = 0;
            if (scenario == "concurrent")
            {
                std::barrier start(2);
                const auto report = [&]
                {
                    start.arrive_and_wait();
                    OIV::ReportUnhandledException(error);
                };
                std::jthread first(report), second(report);
            }
            else
            {
                OIV::ReportUnhandledException(error);
                OIV::ReportUnhandledException(error);
            }
            return EXIT_FAILURE;
        }
        if (scenario == "default-event-allocation")
        {
            LLUtils::Event<void(int)> event;
            unsigned calls           = 0;
            auto subscription        = event.Subscribe([&](int value) { calls += value; });
            allocationsBeforeFailure = 0;
            event.Raise(3);
            auto moved = std::move(subscription);
            moved.Unsubscribe();
            event.Raise(5);
            allocationsBeforeFailure = -1;
            return calls == 3 ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        if (scenario == "what-allocation")
        {
            LLUtils::Exception error(LLUtils::Exception::ErrorCode::RuntimeError, "accessor test",
                                     "retained description", false, LLUtils::Exception::Mode::Exception, 64);
            allocationsBeforeFailure = 0;
            auto copy                = error;
            auto moved               = std::move(error);
            const bool stable        = copy.what() == moved.what() && error.what() == moved.what() &&
                                       &copy.GetDetails() == &moved.GetDetails();
            allocationsBeforeFailure = -1;
            return stable ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        if (scenario == "handled-allocation")
        {
            const auto error         = std::make_exception_ptr(std::runtime_error("handled allocation failure"));
            allocationsBeforeFailure = 0;
            OIV::ReportHandledException(error);
            allocationsBeforeFailure = -1;
            return EXIT_SUCCESS;
        }
        if (scenario == "handled-allocation-sweep")
        {
            const auto error = std::make_exception_ptr(std::runtime_error("handled allocation failure"));
            bool completed   = false;
            for (int budget = 0; budget < MaxAllocationBudget && !completed; ++budget)
            {
                failureInjected          = false;
                allocationsBeforeFailure = budget;
                OIV::ReportHandledException(error);
                allocationsBeforeFailure = -1;
                completed                = !failureInjected;
            }
            return completed ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        if (scenario == "format-allocation-sweep")
        {
            LLUtils::Exception::EventArgs details;
            details.description = "formatting probe \xf0\x9f\x8c\x8d";
            details.stackTrace.resize(2);
            details.stackTrace[0].moduleName     = LLUTILS_TEXT("module with a long Unicode name \u00e9");
            details.stackTrace[0].sourceFileName = LLUTILS_TEXT("long source filename for conversion.cpp");
            bool completed                       = false;
            for (int budget = 0; budget < MaxAllocationBudget && !completed; ++budget)
            {
                failureInjected          = false;
                allocationsBeforeFailure = budget;
                try
                {
                    const auto report        = LLUtils::FormatException(details);
                    allocationsBeforeFailure = -1;
                    if (report.find("formatting probe") == std::string::npos)
                        return EXIT_FAILURE;
                    completed = !failureInjected;
                }
                catch (const std::bad_alloc&)
                {
                    allocationsBeforeFailure = -1;
                }
            }
            return completed ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        if (scenario == "utf8-validation-allocation")
        {
            const std::string valid = "UTF-8 validation \xf0\x9f\x8c\x8d", invalid = "invalid \xff";
            allocationsBeforeFailure = 0;
            const bool correct       = LLUtils::StringUtility::IsValidUtf8(valid) &&
                                       !LLUtils::StringUtility::IsValidUtf8(invalid);
            allocationsBeforeFailure = -1;
            return correct && !failureInjected ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        if (scenario == "snapshot-allocation")
        {
            unsigned observed = 0;
            auto observer     = LLUtils::Exception::OnException.Subscribe([&](const auto&) { ++observed; });
            const LLUtils::Exception::EventArgs args;
            allocationsBeforeFailure = 0;
            LLUtils::Exception::OnException.Raise(args);
            allocationsBeforeFailure = -1;
            LLUtils::Exception::OnException.Raise(args);
            return observed == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        if (scenario == "optional-allocation")
        {
            // Find an optional capture failure without depending on the vector's allocation size or growth policy.
            bool retained = false;
            for (int budget = 0; budget < MaxAllocationBudget && !retained; ++budget)
            {
                failureInjected          = false;
                allocationsBeforeFailure = budget;
                try
                {
                    const LLUtils::Exception error(LLUtils::Exception::ErrorCode::RuntimeError, "optional capture",
                                                   "original retained failure", false,
                                                   LLUtils::Exception::Mode::Exception);
                    allocationsBeforeFailure = -1;
                    retained                 = failureInjected && error.GetDetails().stackTrace.empty() &&
                                               std::string_view(error.what()) == "original retained failure";
                }
                catch (const std::bad_alloc&)
                {
                    allocationsBeforeFailure = -1;  // Core snapshot allocation may fail before capture begins.
                }
            }
            return retained ? EXIT_SUCCESS : EXIT_FAILURE;
        }
#ifdef _WIN32
        if (scenario == "handled-native")
            return HandledNativeException() ? EXIT_SUCCESS : EXIT_FAILURE;
        if (scenario == "cpp-then-native")
            OIV::ReportUnhandledException(
                std::make_exception_ptr(std::runtime_error("reported before native failure")));
        if (scenario == "native" || scenario == "cpp-then-native")
        {
            auto* pointer = reinterpret_cast<volatile int*>(std::uintptr_t{1});
            *pointer      = 1;
        }
        if (scenario == "native-short-record")
            RaiseException(EXCEPTION_IN_PAGE_ERROR, EXCEPTION_NONCONTINUABLE, 0, nullptr);
#endif
        return 2;
    }
    catch (...)
    {
        OIV::ReportUnhandledException(std::current_exception());
        return EXIT_FAILURE;
    }
}

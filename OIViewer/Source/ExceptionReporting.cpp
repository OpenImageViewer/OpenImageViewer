#include "ExceptionReporting.h"

#include <LLUtils/ExceptionFormatter.h>

#include <atomic>
#include <cstdlib>
#include <utility>

namespace OIV
{
    bool detail::TryBeginExceptionReport() noexcept
    {
        static constinit std::atomic_flag reporting = ATOMIC_FLAG_INIT;
        return !reporting.test_and_set(std::memory_order_relaxed);
    }

    [[noreturn]] void detail::TerminateAfterException() noexcept
    {
        ReportUnhandledException(std::current_exception());
        std::abort();
    }

    void ReportUnhandledException(std::exception_ptr exception) noexcept
    {
        if (!detail::TryBeginExceptionReport())
            return;
        try
        {
            const auto message = LLUtils::FormatException(std::move(exception));
            detail::PresentExceptionReport(message);
        }
        catch (...)
        {
            // Use the native static fallback without allocating again to convert a fallback message.
            detail::PresentExceptionReport({});
        }
    }
}  // namespace OIV

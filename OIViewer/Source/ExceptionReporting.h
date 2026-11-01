#pragma once

#include <exception>
#include <string_view>

namespace OIV
{
    void ReportUnhandledException(std::exception_ptr exception) noexcept;

    namespace detail
    {
        // C++ and native terminal paths share one nonblocking, process-lifetime reporting claim.
        [[nodiscard]] bool TryBeginExceptionReport() noexcept;
        [[noreturn]] void TerminateAfterException() noexcept;

        // Platform sink: borrow null-terminated UTF-8 storage for this call and contain presentation failures.
        // An empty report selects native static text, avoiding diagnostic formatting/conversion allocations.
        void PresentExceptionReport(std::string_view message) noexcept;
    }  // namespace detail
}  // namespace OIV

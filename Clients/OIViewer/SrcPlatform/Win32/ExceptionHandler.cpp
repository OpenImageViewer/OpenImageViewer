#include "ExceptionHandler.h"
#include <LLUtils/StringUtility.h>
#include <LLUtils/Emergency.h>

#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <iterator>
#include <string_view>
#include <tuple>

namespace OIV
{
    namespace
    {
        constexpr wchar_t Title[]    = L"OIViewer - Unhandled exception";
        constexpr wchar_t Fallback[] = L"OIViewer encountered an unhandled exception and must close. Diagnostics are "
                                       L"unavailable.";

        // No application logger or viewer object is safe to call at this boundary.
        void Present(const wchar_t* message) noexcept
        {
            // The emergency path owns file, stderr/Unicode-console and debugger output. Convert complete
            // UTF-16 scalar chunks without allocating so a long native report is not truncated to one notice.
            constexpr auto ChunkUnits = LLUtils::EmergencyDetail::Emergency::MaxMessageBytes / 4;
            static_assert(ChunkUnits >= 2);  // Reserve four UTF-8 bytes per unit and never split a surrogate pair.
            const std::wstring_view text(message);
            for (std::size_t offset = 0; offset < text.size();)
            {
                auto count      = (std::min) (text.size() - offset, ChunkUnits);
                const auto last = text[offset + count - 1];
                if (offset + count < text.size() && last >= 0xd800 && last <= 0xdbff)
                    --count;
                char bytes[LLUtils::EmergencyDetail::Emergency::MaxMessageBytes];
                const int size = WideCharToMultiByte(CP_UTF8, 0, text.data() + offset, static_cast<int>(count), bytes,
                                                     static_cast<int>(std::size(bytes)), nullptr, nullptr);
                if (size > 0)
                    LLUtils::EmergencyDetail::Emergency::Write(std::string_view(bytes, size));
                offset += count;
            }
            // A failed call is not retried: the process may no longer be able to create UI.
            if (MessageBoxW(nullptr, message, Title, MB_OK | MB_ICONERROR | MB_TASKMODAL | MB_SETFOREGROUND) != 0)
            {
#ifdef _MSC_VER
                // The terminate path calls std::abort() after this dialog is dismissed. Clear only
                // its debug-message flag to avoid a second dialog; keep _CALL_REPORTFAULT unchanged.
                // If MessageBoxW fails, this block is skipped and the CRT policy remains intact.
                std::ignore = _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
            }
        }

        const wchar_t* NativeName(DWORD code) noexcept
        {
            switch (code)
            {
                case EXCEPTION_ACCESS_VIOLATION:
                    return L"Access violation";
                case EXCEPTION_IN_PAGE_ERROR:
                    return L"In-page error";
                case EXCEPTION_STACK_OVERFLOW:
                    return L"Stack overflow";
                case EXCEPTION_ILLEGAL_INSTRUCTION:
                    return L"Illegal instruction";
                case EXCEPTION_INT_DIVIDE_BY_ZERO:
                    return L"Integer divide by zero";
                case EXCEPTION_FLT_DIVIDE_BY_ZERO:
                    return L"Floating-point divide by zero";
                default:
                    return L"Native exception";
            }
        }

        LONG WINAPI ReportNativeException(EXCEPTION_POINTERS* pointers) noexcept
        {
            if (detail::TryBeginExceptionReport())
            {
                // Avoid the heap, symbol APIs and ordinary exception/logging callbacks on the faulting thread. These
                // record checks cannot guarantee memory integrity after native corruption.
                wchar_t message[768]{};
                const auto* record = pointers != nullptr ? pointers->ExceptionRecord : nullptr;
                int count          = -1;
                if (record != nullptr)
                {
                    count = std::swprintf(message, std::size(message), L"%ls (0x%08lx) at %p\n",
                                          NativeName(record->ExceptionCode), record->ExceptionCode,
                                          record->ExceptionAddress);
                    if (count > 0 &&
                        (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
                         record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) &&
                        record->NumberParameters >= 2)
                    {
                        const auto operation = record->ExceptionInformation[0];
                        const auto* name     = operation == 0   ? L"Read"
                                               : operation == 1 ? L"Write"
                                               : operation == 8 ? L"Execute"
                                                                : L"Unknown access";
                        const auto added     = std::swprintf(
                            message + count, std::size(message) - count, L"%ls at address 0x%llx\n", name,
                            static_cast<unsigned long long>(record->ExceptionInformation[1]));
                        count = added < 0 ? -1 : count + added;
                    }
                    if (count > 0 && record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR && record->NumberParameters >= 3)
                    {
                        const auto added = std::swprintf(
                            message + count, std::size(message) - count, L"Underlying NTSTATUS: 0x%llx\n",
                            static_cast<unsigned long long>(record->ExceptionInformation[2]));
                        if (added < 0)
                            count = -1;
                    }
                }
                Present(count > 0 ? message : Fallback);
            }
            // Preserve OS crash handling; never attempt to resume the faulting instruction.
            return EXCEPTION_CONTINUE_SEARCH;
        }
    }  // namespace

    ExceptionRegistration::ExceptionRegistration() noexcept
        : fPreviousTerminate(std::set_terminate(detail::TerminateAfterException)),
          fPreviousFilter(SetUnhandledExceptionFilter(ReportNativeException))
    {
    }

    ExceptionRegistration::~ExceptionRegistration()
    {
        SetUnhandledExceptionFilter(fPreviousFilter);
        std::set_terminate(fPreviousTerminate);
    }

    void detail::PresentExceptionReport(std::string_view report) noexcept
    {
        if (report.empty())
        {
            Present(Fallback);
        }
        else
        {
            try
            {
                const auto message = LLUtils::StringUtility::ToWString(report);
                Present(message.c_str());
            }
            catch (...)
            {
                Present(Fallback);
            }
        }
    }
}  // namespace OIV

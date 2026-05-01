#include <windows.h>
//include <typeinfo> for successful compiling with minGW
#include <typeinfo>
#include <eh.h>
#include <Psapi.h>
#include <string>
#include <sstream>
#include <LLUtils/Exception.h>


namespace OIV
{
    class InfoFromSE
    {
    public:
        using exception_code_t = unsigned int;

        static const char* opDescription(const ULONG_PTR opcode)
        {
            switch (opcode) {
            case 0: return "read";
            case 1: return "write";
            case 8: return "user-mode data execution prevention (DEP) violation";
            default: return "unknown";
            }
        }

        static const char* seDescription(const exception_code_t& code)
        {
            switch (code) {
            case EXCEPTION_ACCESS_VIOLATION:         return "EXCEPTION_ACCESS_VIOLATION";
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
            case EXCEPTION_BREAKPOINT:               return "EXCEPTION_BREAKPOINT";
            case EXCEPTION_DATATYPE_MISALIGNMENT:    return "EXCEPTION_DATATYPE_MISALIGNMENT";
            case EXCEPTION_FLT_DENORMAL_OPERAND:     return "EXCEPTION_FLT_DENORMAL_OPERAND";
            case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
            case EXCEPTION_FLT_INEXACT_RESULT:       return "EXCEPTION_FLT_INEXACT_RESULT";
            case EXCEPTION_FLT_INVALID_OPERATION:    return "EXCEPTION_FLT_INVALID_OPERATION";
            case EXCEPTION_FLT_OVERFLOW:             return "EXCEPTION_FLT_OVERFLOW";
            case EXCEPTION_FLT_STACK_CHECK:          return "EXCEPTION_FLT_STACK_CHECK";
            case EXCEPTION_FLT_UNDERFLOW:            return "EXCEPTION_FLT_UNDERFLOW";
            case EXCEPTION_ILLEGAL_INSTRUCTION:      return "EXCEPTION_ILLEGAL_INSTRUCTION";
            case EXCEPTION_IN_PAGE_ERROR:            return "EXCEPTION_IN_PAGE_ERROR";
            case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "EXCEPTION_INT_DIVIDE_BY_ZERO";
            case EXCEPTION_INT_OVERFLOW:             return "EXCEPTION_INT_OVERFLOW";
            case EXCEPTION_INVALID_DISPOSITION:      return "EXCEPTION_INVALID_DISPOSITION";
            case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
            case EXCEPTION_PRIV_INSTRUCTION:         return "EXCEPTION_PRIV_INSTRUCTION";
            case EXCEPTION_SINGLE_STEP:              return "EXCEPTION_SINGLE_STEP";
            case EXCEPTION_STACK_OVERFLOW:           return "EXCEPTION_STACK_OVERFLOW";
            default: return "UNKNOWN EXCEPTION";
            }
        }

        static std::string information(struct _EXCEPTION_POINTERS* ep, bool has_exception_code = false, exception_code_t code = 0)
        {
            HMODULE hm;
            ::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, static_cast<LPCTSTR>(ep->ExceptionRecord->ExceptionAddress), &hm);
            MODULEINFO mi;
            ::GetModuleInformation(::GetCurrentProcess(), hm, &mi, sizeof(mi));
            char fn[MAX_PATH];
            ::GetModuleFileNameExA(::GetCurrentProcess(), hm, fn, MAX_PATH);

            std::ostringstream oss;
            oss << "Exception " << (has_exception_code ? seDescription(code) : "") << " at address 0x" << std::hex << ep->ExceptionRecord->ExceptionAddress << std::dec
                << " inside " << fn << " loaded at base address 0x" << std::hex << mi.lpBaseOfDll << "\n";

            if (has_exception_code && (
                code == EXCEPTION_ACCESS_VIOLATION ||
                code == EXCEPTION_IN_PAGE_ERROR)) {
                oss << "Invalid operation: " << opDescription(ep->ExceptionRecord->ExceptionInformation[0]) << " at address 0x" << std::hex
                << static_cast<size_t>(ep->ExceptionRecord->ExceptionInformation[1]) << std::dec << "\n";
            }

            if (has_exception_code && code == EXCEPTION_IN_PAGE_ERROR) {
                oss << "Underlying NTSTATUS code that resulted in the exception " << ep->ExceptionRecord->ExceptionInformation[2] << "\n";
            }

            return oss.str();
        }
    };

    LONG global_exception_handler(_EXCEPTION_POINTERS* ExceptionInfo)
    {
        auto description = InfoFromSE::information(ExceptionInfo);
        std::ignore = LLUtils::Exception(LLUtils::Exception::ErrorCode::Unknown, __FUNCTION__, description, false, LLUtils::Exception::Mode::Exception, 6);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    PVOID exceptionHandle{};

    void RegisterExceptionhandler()
    {
        if (exceptionHandle == nullptr)
            exceptionHandle = AddVectoredExceptionHandler(0, global_exception_handler);
    }

    void RemoveExceptionHandler()
    {
        if (exceptionHandle != nullptr)
            if (RemoveVectoredExceptionHandler(exceptionHandle) != 0)
                exceptionHandle = nullptr;
    }
}
#pragma once
#include <exception>
#include <string>
#include "Event.h"
#include "PlatformUtility.h"

namespace LLUtils
{
 
    class Exception : public std::exception
    {
    public:
        enum class ErrorCode
        {
              Unspecified
            , Unknown
            , CorruptedValue
            , LogicError
            , RuntimeError
            , DuplicateItem
            , BadParameters
            , NotImplemented
            , SystemError
        };

        struct EventArgs
        {
            ErrorCode errorCode;
            std::wstring description;
            std::wstring systemErrorMessage;
            std::wstring callstack;
            std::wstring functionName;
        };

    
        using OnExceptionEventType = Event<void(EventArgs)>;
        static OnExceptionEventType OnException;
        Exception(ErrorCode errorCode, std::string function, std::string description, bool systemError)
        {
            using namespace std;
            PlatformUtility::StackTrace stackTrace = PlatformUtility::GetCallStack(2);

            std::wstring systemErrorMessage;
            if (systemError == true)
                systemErrorMessage = PlatformUtility::GetLastErrorAsString() ;
            
            wstringstream ss;

            for (const auto& f : stackTrace)
                ss << f.fileName << L" at " << f.name << L" line: " << f.line << L" at address 0x" << hex << f.address << endl;

            std::wstring callStack = ss.str();

            
            EventArgs args =
            {
                  errorCode
                , LLUtils::StringUtility::ToWString(description)
                , systemErrorMessage
                , callStack
                , LLUtils::StringUtility::ToWString(function)
            };

            OnException.Raise(args);
            
        }

        static std::wstring ExceptionErrorCodeToString(ErrorCode errorCode)
        {
            switch (errorCode)
            {
            case ErrorCode::Unspecified:
                return L"Unspecified";
                break;
            case ErrorCode::Unknown:
                return L"Unknown";
                break;
            case ErrorCode::CorruptedValue:
                return L"Corrupted value";
                break;
            case ErrorCode::LogicError:
                return L"Logic error";
                break;
            case ErrorCode::RuntimeError:
                return L"Runtime error";
                break;
            case ErrorCode::DuplicateItem:
                return L"Duplicate item";
                break;
            case ErrorCode::BadParameters:
                return L"Bad parameters";
                break;
            case ErrorCode::NotImplemented:
                return L"Missing implmentation";
                break;
            case ErrorCode::SystemError:
                return L"System error";
                break;
            default:
                return L"Unspecified";

            }
        }
    };


#define LL_EXCEPTION(ERROR_CODE,DESCRIPTION) ( throw LLUtils::Exception(ERROR_CODE, __FUNCTION__, DESCRIPTION, false) )
#define LL_EXCEPTION_SYSTEM_ERROR(DESCRIPTION) ( throw LLUtils::Exception(LLUtils::Exception::ErrorCode::SystemError, __FUNCTION__, DESCRIPTION, true) )
#define LL_EXCEPTION_UNEXPECTED_VALUE ( throw LLUtils::Exception(LLUtils::Exception::ErrorCode::RuntimeError, __FUNCTION__, "unexpected or coruppted value.", false) )

#define LL_EXCEPTION_DECLARE_HANDLER LLUtils::Exception::OnExceptionEventType LLUtils::Exception::OnException;

}
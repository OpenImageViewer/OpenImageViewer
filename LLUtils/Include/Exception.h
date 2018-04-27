#pragma once
#include <exception>
#include <string>
#include <array>
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
            , __Count__
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
        static inline OnExceptionEventType OnException;
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
            static std::array<std::wstring, static_cast<size_t>( ErrorCode::__Count__)> errorcodeToString =
            {
                 L"Unspecified"
                ,L"Unknown"
                ,L"Corrupted value"
                ,L"Logic error"
                ,L"Runtime error"
                ,L"Duplicate item"
                ,L"Bad parameters"
                ,L"Missing implmentation"
                ,L"System error"
            };

            int errorCodeInt = static_cast<int>(errorCode);

            if (errorCodeInt >= 0 && errorCodeInt < errorcodeToString.size())
                return errorcodeToString[errorCodeInt];
            else
                return L"Unspecified";
        }
    };


#define LL_EXCEPTION(ERROR_CODE,DESCRIPTION) ( throw LLUtils::Exception(ERROR_CODE, __FUNCTION__, DESCRIPTION, false) )
#define LL_EXCEPTION_SYSTEM_ERROR(DESCRIPTION) ( throw LLUtils::Exception(LLUtils::Exception::ErrorCode::SystemError, __FUNCTION__, DESCRIPTION, true) )
#define LL_EXCEPTION_UNEXPECTED_VALUE ( throw LLUtils::Exception(LLUtils::Exception::ErrorCode::RuntimeError, __FUNCTION__, "unexpected or coruppted value.", false) )

}
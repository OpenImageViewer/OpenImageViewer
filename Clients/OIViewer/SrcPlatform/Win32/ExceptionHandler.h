#pragma once
#include "ExceptionReporting.h"
#include <Windows.h>
namespace OIV
{
    // Own process handlers for the complete entry-point scope, including its terminal catch.
    class ExceptionRegistration final
    {
      public:

        ExceptionRegistration() noexcept;
        ~ExceptionRegistration();
        ExceptionRegistration(const ExceptionRegistration&)            = delete;
        ExceptionRegistration& operator=(const ExceptionRegistration&) = delete;

      private:

        std::terminate_handler fPreviousTerminate;
        LPTOP_LEVEL_EXCEPTION_FILTER fPreviousFilter;
    };
}  // namespace OIV

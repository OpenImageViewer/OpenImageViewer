#pragma once
#include "ExceptionReporting.h"
#include <optional>

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
    };

    // Handle only the private dialog subprocess before normal startup and handler registration.
    std::optional<int> RunExceptionDialog(int argc, char* argv[]);
}  // namespace OIV

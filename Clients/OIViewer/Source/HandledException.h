#pragma once
#include <LLUtils/Exception.h>

namespace OIV
{
    // Both viewer callbacks and the LWS callback boundary deliberately recover. Even
    // allocating a diagnostic object can fail, so contain the entire reporting operation.
    inline void ReportHandledException(std::exception_ptr exception) noexcept
    {
        if (!exception)
            return;
        try
        {
            try
            {
                std::rethrow_exception(exception);
            }
            catch (const LLUtils::Exception&)
            { /* Already observed at construction. */
            }
            catch (const std::exception& error)
            {
                LL_EXCEPTION_DONT_THROW(LLUtils::Exception::ErrorCode::RuntimeError, error.what());
            }
            catch (...)
            {
                LL_EXCEPTION_DONT_THROW(LLUtils::Exception::ErrorCode::Unknown, "Unhandled UI callback exception");
            }
        }
        catch (...)
        { /* Loss of a handled diagnostic must not terminate the application. */
        }
    }
}  // namespace OIV

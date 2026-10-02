#pragma once
#include <LLUtils/StringDefs.h>
#include <mutex>
#include <string_view>

namespace OIV
{
    // An independently owned sink lets an in-flight exception observer finish after UI teardown.
    class ApplicationLog final
    {
      public:

        ApplicationLog(LLUtils::native_string_type logPath, bool clear);
        ApplicationLog(const ApplicationLog&)            = delete;
        ApplicationLog& operator=(const ApplicationLog&) = delete;

        void Log(std::string_view message);
        [[nodiscard]] const LLUtils::native_string_type& GetLogPath() const noexcept { return fLogPath; }

      private:

        const LLUtils::native_string_type fLogPath;
        std::mutex fMutex;
    };
}  // namespace OIV

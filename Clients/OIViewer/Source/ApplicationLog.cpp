#include "ApplicationLog.h"
#include <filesystem>
#include <fstream>
#include <utility>

namespace OIV
{
    ApplicationLog::ApplicationLog(LLUtils::native_string_type logPath, bool clear) : fLogPath(std::move(logPath))
    {
        const std::filesystem::path path(fLogPath);
        std::filesystem::create_directories(path.parent_path());
        if (clear)
            std::ofstream(path, std::ios::trunc);
    }

    void ApplicationLog::Log(std::string_view message)
    {
        // Paths remain native; message bytes are already UTF-8 and stay borrowed throughout the serialized write.
        const std::lock_guard lock(fMutex);
        std::ofstream stream(std::filesystem::path(fLogPath), std::ios::app | std::ios::binary);
        stream << message;
    }
}  // namespace OIV

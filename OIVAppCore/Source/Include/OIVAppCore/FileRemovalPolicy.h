#pragma once

#include <cstddef>
#include <string>

namespace OIV
{
    enum class RemovedFileAction
    {
        Ignore,
        KeepMissingCurrent,
        TryStart,
        TryNextThenPrevious
    };

    class FileRemovalPolicy
    {
      public:
        static RemovedFileAction Decide(const std::wstring& openedFile,
                                        const std::wstring& removedFile,
                                        const std::wstring& requestedRemovalFile,
                                        bool removeInternalDeletes,
                                        bool removeExternalDeletes,
                                        std::size_t fileCount);
        static bool ShouldUnloadAfterJumps(RemovedFileAction action,
                                           bool firstJumpSucceeded,
                                           bool fallbackJumpSucceeded);
    };
}  // namespace OIV

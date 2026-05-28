#pragma once

#include <cstddef>
#include <LLUtils/StringDefs.h>

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

        static RemovedFileAction Decide(const LLUtils::native_string_type& openedFile,
                                        const LLUtils::native_string_type& removedFile,
                                        const LLUtils::native_string_type& requestedRemovalFile,
                                        bool removeInternalDeletes, bool removeExternalDeletes, std::size_t fileCount);
        static bool ShouldUnloadAfterJumps(RemovedFileAction action, bool firstJumpSucceeded,
                                           bool fallbackJumpSucceeded);
    };
}  // namespace OIV

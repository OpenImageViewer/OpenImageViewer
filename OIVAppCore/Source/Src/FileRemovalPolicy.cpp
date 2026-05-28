#include <LLUtils/StringDefs.h>
#include <OIVAppCore/FileRemovalPolicy.h>

namespace OIV
{
    RemovedFileAction FileRemovalPolicy::Decide(const LLUtils::native_string_type& openedFile,
                                                const LLUtils::native_string_type& removedFile,
                                                const LLUtils::native_string_type& requestedRemovalFile,
                                                bool removeInternalDeletes, bool removeExternalDeletes,
                                                std::size_t fileCount)
    {
        if (removedFile != openedFile)
            return RemovedFileAction::Ignore;

        const bool internallyRemoved = requestedRemovalFile == openedFile;
        const bool shouldRemove      = internallyRemoved ? removeInternalDeletes : removeExternalDeletes;
        if (!shouldRemove)
            return RemovedFileAction::KeepMissingCurrent;

        return fileCount == 1 ? RemovedFileAction::TryStart : RemovedFileAction::TryNextThenPrevious;
    }

    bool FileRemovalPolicy::ShouldUnloadAfterJumps(RemovedFileAction action, bool firstJumpSucceeded,
                                                   bool fallbackJumpSucceeded)
    {
        switch (action)
        {
            case RemovedFileAction::TryStart:
                return !firstJumpSucceeded;
            case RemovedFileAction::TryNextThenPrevious:
                return !firstJumpSucceeded && !fallbackJumpSucceeded;
            case RemovedFileAction::Ignore:
            case RemovedFileAction::KeepMissingCurrent:
                return false;
        }

        return false;
    }
}  // namespace OIV

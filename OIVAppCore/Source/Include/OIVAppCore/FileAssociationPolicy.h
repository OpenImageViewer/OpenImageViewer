#pragma once

#include <LLUtils/StringDefs.h>
#include <cstdint>
#include <filesystem>
#include <functional>

namespace OIV
{
    enum class FileAssociationAction
    {
        Check,
        Add,
        Remove
    };

    enum class FileAssociationStatus
    {
        CurrentViewer,
        OtherOrMissing,
        Unknown
    };
    enum class FileAssociationChoice
    {
        Associate,
        SuppressForCopy,
        SuppressForUser,
        AskNextTime
    };

    struct FileAssociationNotice
    {
        enum class Kind
        {
            Prompt,
            OpenSettings,
            Completed,
            CheckFailed,
            ActionFailed
        };
        Kind kind;
        LLUtils::native_string_type message;
        uint64_t requestId{};
    };

    using FileAssociationNotify = std::function<void(FileAssociationNotice)>;

    class FileAssociationPolicy
    {
      public:

        // Locations survive binary replacement and upgrades. Resolving directory aliases and symlinks keeps
        // alternate spellings of the same installation from producing another prompt.
        static LLUtils::native_string_type ExecutableLocation(const LLUtils::native_string_type& executable)
        {
            return std::filesystem::weakly_canonical(executable).native();
        }
    };
}  // namespace OIV

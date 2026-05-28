#pragma once

#include <LLUtils/Event.h>
#include <LLUtils/StringDefs.h>
#include <cstdint>

namespace OIV
{
    class IFileWatcher
    {
      public:

        using FolderID = std::uint16_t;
        enum class FileChangedOp
        {
            None,
            Add,
            Remove,
            Modified,
            Rename,
            WatchedFolderRemoved
        };

        struct FileChangedEventArgs
        {
            FolderID folderID;
            FileChangedOp fileOp;
            LLUtils::native_string_type folder;
            LLUtils::native_string_type fileName;
            LLUtils::native_string_type fileName2;
        };

        using OnFileChangedEventArgsEvent = LLUtils::Event<void(FileChangedEventArgs)>;

        virtual ~IFileWatcher() = default;

        virtual bool IsFolderRegistered(const LLUtils::native_string_type& folder) const = 0;
        virtual FolderID AddFolder(const LLUtils::native_string_type& folder)            = 0;
        virtual void RemoveFolder(FolderID folderID)                                     = 0;
        virtual void RemoveFolder(const LLUtils::native_string_type& folder)             = 0;
        virtual OnFileChangedEventArgsEvent& GetFileChangedEvent()                       = 0;
    };
}  // namespace OIV

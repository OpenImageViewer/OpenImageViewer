#pragma once

#include <LLUtils/Event.h>

#include <cstdint>
#include <string>

namespace OIV
{
    class IFileWatcher
    {
      public:
        using FolderID = std::uint16_t;
        enum class FileChangedOp { None, Add, Remove, Modified, Rename, WatchedFolderRemoved };

        struct FileChangedEventArgs
        {
            FolderID folderID;
            FileChangedOp fileOp;
            std::wstring folder;
            std::wstring fileName;
            std::wstring fileName2;
        };

        using OnFileChangedEventArgsEvent = LLUtils::Event<void(FileChangedEventArgs)>;

        virtual ~IFileWatcher() = default;

        virtual bool IsFolderRegistered(const std::wstring& folder) const = 0;
        virtual FolderID AddFolder(const std::wstring& folder) = 0;
        virtual void RemoveFolder(FolderID folderID) = 0;
        virtual void RemoveFolder(const std::wstring& folder) = 0;
        virtual OnFileChangedEventArgsEvent& GetFileChangedEvent() = 0;
    };
}  // namespace OIV

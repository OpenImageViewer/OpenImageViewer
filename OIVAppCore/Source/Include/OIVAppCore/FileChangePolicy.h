#pragma once

#include <OIVAppCore/IFileWatcher.h>

#include <string>

namespace OIV
{
    enum class FileChangeAction
    {
        Ignore,
        CurrentFileChanged,
        ClearWatchedFolder,
        ReloadSettings,
        UnexpectedFolder
    };

    class FileChangePolicy
    {
      public:
        static FileChangeAction Decide(const IFileWatcher::FileChangedEventArgs& eventArgs,
                                       bool hasActiveFolder,
                                       IFileWatcher::FolderID activeFolderID,
                                       IFileWatcher::FolderID configurationFolderID,
                                       const std::wstring& openedFile);
    };
}

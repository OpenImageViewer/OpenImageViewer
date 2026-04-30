#include <oivappcore/FileChangePolicy.h>

#include <filesystem>

namespace OIV
{
    FileChangeAction FileChangePolicy::Decide(const IFileWatcher::FileChangedEventArgs& eventArgs,
                                              bool hasActiveFolder,
                                              IFileWatcher::FolderID activeFolderID,
                                              IFileWatcher::FolderID configurationFolderID,
                                              const std::wstring& openedFile)
    {
        if (hasActiveFolder && eventArgs.folderID == activeFolderID)
        {
            const std::wstring changedFile =
                (std::filesystem::path(eventArgs.folder) / eventArgs.fileName).wstring();
            const std::wstring renamedFile =
                (std::filesystem::path(eventArgs.folder) / eventArgs.fileName2).wstring();

            switch (eventArgs.fileOp)
            {
                case IFileWatcher::FileChangedOp::Modified:
                    return openedFile == changedFile ? FileChangeAction::CurrentFileChanged
                                                     : FileChangeAction::Ignore;
                case IFileWatcher::FileChangedOp::Rename:
                    return openedFile == renamedFile ? FileChangeAction::CurrentFileChanged
                                                     : FileChangeAction::Ignore;
                case IFileWatcher::FileChangedOp::WatchedFolderRemoved:
                    return FileChangeAction::ClearWatchedFolder;
                case IFileWatcher::FileChangedOp::None:
                case IFileWatcher::FileChangedOp::Add:
                case IFileWatcher::FileChangedOp::Remove:
                    return FileChangeAction::Ignore;
            }
        }

        if (eventArgs.folderID == configurationFolderID)
            return eventArgs.fileName == L"Settings.json" ? FileChangeAction::ReloadSettings
                                                          : FileChangeAction::Ignore;

        return FileChangeAction::UnexpectedFolder;
    }
}

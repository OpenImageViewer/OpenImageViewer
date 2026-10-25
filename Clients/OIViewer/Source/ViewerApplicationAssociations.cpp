#include "ViewerApplication.h"
#include "FileAssociations.h"
#include <LLUtils/PlatformUtility.h>

namespace OIV
{
    void ViewerApplication::StartFileAssociationOperation(FileAssociationAction action)
    {
        StopFileAssociationOperation();
        const auto requestId = ++fAssociationRequestId;
        auto executable      = LLUtils::PlatformUtility::GetExePath();
        auto manageAssociations =
            [config = fUserDirectories.config, extensions = fSession.KnownFileTypes(),
             executable](FileAssociationAction operation, std::stop_token stop, const FileAssociationNotify& notify)
        {
#ifdef _WIN32
            Win32::ManageFileAssociations(extensions, executable, notify, stop, operation);
#else
            notify(Linux::ManageFileAssociations(config, extensions, executable, operation, stop));
#endif
        };
        fFileAssociations = std::make_unique<FileAssociationSession>(
            fUserDirectories.config, std::move(executable), std::move(manageAssociations),
            [this, requestId](FileAssociationNotice notice)
            {
                notice.requestId = requestId;
                fViewer.QueueUiCompletion(std::move(notice));
            },
            action);
    }

    void ViewerApplication::StopFileAssociationOperation()
    {
        fFileAssociations.reset();
        if (fNativeWindowState != nullptr)
            CloseFileAssociationDialog();
    }

    void ViewerApplication::CMD_FileAssociations(const CommandManager::CommandRequest& request,
                                                 CommandManager::CommandResult& result)
    {
        const auto action = request.args.GetArgValue("action");
        if (action != "add" && action != "remove")
            result.resValue = LLUTILS_TEXT("Unknown file association action.");
        else if (fFileAssociations != nullptr && fFileAssociations->IsRunning())
            result.resValue = LLUTILS_TEXT("A file association operation is already running.");
        else
        {
            StartFileAssociationOperation(action == "add" ? FileAssociationAction::Add : FileAssociationAction::Remove);
            result.resValue = action == "add" ? LLUTILS_TEXT("Adding file associations...")
                                              : LLUTILS_TEXT("Removing file associations...");
        }
    }
}  // namespace OIV

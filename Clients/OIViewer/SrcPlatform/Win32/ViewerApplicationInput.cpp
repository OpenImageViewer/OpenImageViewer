#include "ViewerApplication.h"
#include "Globals.h"
#include <LLUtils/Exception.h>
#include <Windows.h>

namespace OIV
{
    void ViewerApplication::CloseApplication(bool closeToTray)
    {
        // The task dialog has its own modal loop, which does not observe the LWS quit flag.
        StopFileAssociationOperation();
        const HANDLE mutex = CreateMutex(
            nullptr, FALSE, (LLUtils::native_string_type(Globals::ProgramGuid) + LLUTILS_TEXT("_CLOSEAPP")).c_str());
        if (mutex == nullptr)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex cannot be created.");

        const DWORD result = WaitForSingleObject(mutex, INFINITE);
        if (result != WAIT_OBJECT_0)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex ownership cannot be acquired.");

        std::ignore = fViewer.fWindow.GetWindow().SetVisible(false);
        if (!closeToTray || FindTrayBarWindow() != 0)
            fPlatform.RequestQuit();
        else
            fViewer.fWindow.SetIsTrayWindow(true);

        if (!ReleaseMutex(mutex))
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex cannot be released.");
        if (!CloseHandle(mutex))
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex cannot be closed.");
    }
}  // namespace OIV

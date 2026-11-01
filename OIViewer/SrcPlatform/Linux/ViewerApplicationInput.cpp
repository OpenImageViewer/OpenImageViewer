#include "ViewerApplication.h"

#include <LWS/Platform.hpp>

#include "ViewerApplicationPlatformState.h"
#include "ViewerMouseInput.h"

#include <LLUtils/Exception.h>

#include <OIVAppCore/ConfigurationLoader.h>

namespace OIV
{
    void ViewerApplication::CloseApplication(bool closeToTray)
    {
        if (closeToTray)
            LL_EXCEPTION_NOT_IMPLEMENT("Close-to-tray is not implemented on Linux");
        fPlatform.RequestQuit();
    }
}  // namespace OIV

#pragma once

#include "LinuxKeyBindings.h"
#include "ViewerApplication.h"
#include <gtk/gtk.h>

namespace OIV
{
    struct ViewerApplication::RawInputState
    {
        LinuxKeyBindings keyBindings;
    };

    struct ViewerApplication::NativeWindowState
    {
        GtkWidget* associationDialog{};
        std::unique_ptr<LWS::HighPrecisionTimer> associationDialogPump;
    };
}  // namespace OIV

#pragma once

#include "LinuxKeyBindings.h"
#include "Viewer.h"

namespace OIV
{
    struct Viewer::RawInputState
    {
        LinuxKeyBindings keyBindings;
    };

}  // namespace OIV

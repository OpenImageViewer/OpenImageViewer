#pragma once
#include <Windows.h>

// WLX ABI from Double Commander's sdk/wlxplugin.h. Search and printing are intentionally not exported.
inline constexpr int WlxOk            = 0;
inline constexpr int WlxError         = 1;
inline constexpr int WlxCopy          = 1;
inline constexpr int WlxNewParams     = 2;
inline constexpr int WlxSelectAll     = 3;
inline constexpr int WlxSetPercent    = 4;
inline constexpr int WlxFit           = 2;
inline constexpr int WlxFitLargerOnly = 32;
inline constexpr int WlxCenter        = 64;

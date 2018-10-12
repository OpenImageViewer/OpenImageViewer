#pragma once

#include <cstdint>
#include <wtypes.h>

namespace OIV
{
    namespace Win32
    {
        enum class WindowPosOp
        {
              None
            , Resize
            , Move
            , Zorder
            , RefreshFrame
            , Placement
        };


        class WindowPosHelper
        {
        public:
            static UINT GetFlagsForWindowSetPosOp(WindowPosOp op)
            {
                static const UINT BaseFlags =
                    0
                    | SWP_NOMOVE
                    | SWP_NOSIZE
                    | SWP_NOZORDER
                    | SWP_NOOWNERZORDER
                    | SWP_NOACTIVATE
                    | SWP_NOCOPYBITS
                    | SWP_NOREPOSITION
                    | SWP_NOREDRAW
                    | SWP_NOSENDCHANGING
                    | SWP_DEFERERASE
                    ;

                switch (op)
                {
                case WindowPosOp::None:
                    return 0;
                case WindowPosOp::Placement:
                    return BaseFlags & (~(SWP_NOMOVE | SWP_NOSIZE));
                case WindowPosOp::Move:
                    return BaseFlags & ~SWP_NOMOVE;
                    break;
                case WindowPosOp::RefreshFrame:
                    return BaseFlags | SWP_FRAMECHANGED;
                    break;
                case WindowPosOp::Resize:
                    return BaseFlags & ~SWP_NOSIZE;
                    break;
                case WindowPosOp::Zorder:
                    return BaseFlags & ~SWP_NOZORDER;
                    break;
                default:
                    return 0;
                }
            }


            static void SetPosition(HWND handle, int32_t x, int32_t y)
            {
                SetWindowPos(handle, nullptr, x, y, 0, 0, GetFlagsForWindowSetPosOp(WindowPosOp::Move));
            }

            static void SetSize(HWND handle, uint32_t width, uint32_t height)
            {
                SetWindowPos(handle, nullptr, 0, 0, width, height, GetFlagsForWindowSetPosOp(WindowPosOp::Resize));
            }

            static void SetPlacement(HWND handle, int32_t x, int32_t y, uint32_t width, uint32_t height)
            {
                SetWindowPos(handle, nullptr, x, y, width, height, GetFlagsForWindowSetPosOp(WindowPosOp::Placement));
            }
        };
    }
}

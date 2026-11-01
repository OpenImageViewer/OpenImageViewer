#include "HostedInputAdapter.h"
#include "Viewer.h"
#include "ViewerPlatformState.h"
#include "ViewerMouseInput.h"
#include <LWS/Win32/WindowExtensions.hpp>
namespace OIV::Win32
{
    void HostedInputAdapter::HandleInput(HWND window, uint32_t message, uintptr_t parameter,
                                         uint32_t& pressedButtons) noexcept
    {
        viewer.HandleEventCallback(
            [&]
            {
                switch (message)
                {
                    // CS_DBLCLKS replaces the second DOWN with DBLCLK. Preserve capture bookkeeping
                    // while the portable event remains one press and the viewer keeps its own gesture policy.
                    case WM_LBUTTONDOWN:
                    case WM_LBUTTONDBLCLK:
                    case WM_RBUTTONDOWN:
                    case WM_RBUTTONDBLCLK:
                    case WM_MBUTTONDOWN:
                    case WM_MBUTTONDBLCLK:
                    case WM_XBUTTONDOWN:
                    case WM_XBUTTONDBLCLK:
                        pressedButtons |= (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK)   ? 1U
                                          : (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK) ? 2U
                                          : (message == WM_MBUTTONDOWN || message == WM_MBUTTONDBLCLK)
                                              ? 4U
                                              : (GET_XBUTTON_WPARAM(parameter) == XBUTTON1 ? 8U : 16U);
                        SetFocus(*LWS::Win32::GetHwnd(viewer.fWindow.GetWindow()));
                        SetCapture(window);
                        break;
                    case WM_LBUTTONUP:
                    case WM_RBUTTONUP:
                    case WM_MBUTTONUP:
                    case WM_XBUTTONUP:
                        pressedButtons &= ~(message == WM_LBUTTONUP   ? 1U
                                            : message == WM_RBUTTONUP ? 2U
                                            : message == WM_MBUTTONUP
                                                ? 4U
                                                : (GET_XBUTTON_WPARAM(parameter) == XBUTTON1 ? 8U : 16U));
                        break;
                    case WM_CAPTURECHANGED:
                        // LWS delivers cancellation through EventMouseCaptureLost after this native adapter returns.
                        pressedButtons = 0;
                        break;
                    case WM_SETFOCUS:
                        viewer.SetAppActive(true);
                        break;
                    case WM_KILLFOCUS:
                    {
                        const HWND focus = reinterpret_cast<HWND>(parameter);
                        const HWND root  = *LWS::Win32::GetHwnd(viewer.fWindow.GetWindow());
                        if (focus != root && !IsChild(root, focus))
                        {
                            viewer.fMouseInput->Cancel();
                            viewer.fRawInputState->hostKeys.reset();
                            viewer.SetAppActive(false);
                        }
                        break;
                    }
                }
                return false;
            });
    }

}  // namespace OIV::Win32

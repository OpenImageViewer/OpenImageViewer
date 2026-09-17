#include "MainWindow.h"

#include <LLUtils/Exception.h>
#include "Resource.h"

#include <Windows.h>

#include <LWS/Win32/WindowExtensions.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace OIV
{
    namespace
    {
        struct IconResources
        {
            ~IconResources()
            {
                if (color != nullptr)
                    DeleteObject(color);
                if (mask != nullptr)
                    DeleteObject(mask);
                if (icon != nullptr)
                    DestroyIcon(icon);
            }

            HICON icon{};
            HBITMAP color{};
            HBITMAP mask{};
        };
    }  // namespace

    struct MainWindow::NativeState
    {
        HWND statusBar = nullptr;
    };

    MainWindow::MainWindow(LWS::PlatformContext& platform)
        : fWindow(platform), fCanvasWindow(platform), fImageControl(platform),
          fNativeState(std::make_unique<NativeState>())
    {
        auto connection = fWindow.Listen(
            [this](const LWS::AnyEvent& eventData)
            { return HandleWindowEvent(eventData) ? LWS::EventResponse::Handled : LWS::EventResponse::Unhandled; });
        if (!connection.has_value())
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to register required window listener");
        fEventConnection = std::move(*connection);
    }

    MainWindow::~MainWindow() = default;

    bool MainWindow::UseMainWindowAsCanvas() const
    {
        return false;
    }

    int32_t MainWindow::GetImageControlClientWidth(int32_t layoutWidth) const
    {
        const HWND window = *LWS::Win32::GetHwnd(fImageControl.GetWindow());
        RECT windowRect{};
        RECT clientRect{};
        GetWindowRect(window, &windowRect);
        GetClientRect(window, &clientRect);
        const auto clientArea        = fImageControl.GetWindow().GetClientAreaMetrics();
        const double scale           = clientArea.Scale().value_or(LWS::ContentScale{}).x;
        const int32_t nonClientWidth = static_cast<int32_t>(
            std::lround(((windowRect.right - windowRect.left) - (clientRect.right - clientRect.left)) / scale));
        return std::max(layoutWidth - nonClientWidth, 1);
    }

    void MainWindow::PrepareImageControlLayout()
    {
        const HWND window = *LWS::Win32::GetHwnd(fImageControl.GetWindow());
        SetWindowLongPtrW(window, GWL_STYLE, GetWindowLongPtrW(window, GWL_STYLE) | WS_VSCROLL);
        SetWindowPos(window, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        SCROLLINFO info{};
        info.cbSize = sizeof(info);
        info.fMask  = SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL;
        info.nPage  = 1;
        SetScrollInfo(window, SB_VERT, &info, FALSE);
    }

    void MainWindow::SetApplicationIcon()
    {
        IconResources handles;
        handles.icon = static_cast<HICON>(
            LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
        if (handles.icon == nullptr)
            return;

        ICONINFO info{};
        const BOOL infoResult = GetIconInfo(handles.icon, &info);
        handles.color         = info.hbmColor;
        handles.mask          = info.hbmMask;
        BITMAP bitmap{};
        if (infoResult == FALSE || handles.color == nullptr || GetObjectW(handles.color, sizeof(bitmap), &bitmap) == 0)
            return;

        BITMAPINFO bitmapInfo{};
        bitmapInfo.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth       = bitmap.bmWidth;
        bitmapInfo.bmiHeader.biHeight      = -bitmap.bmHeight;
        bitmapInfo.bmiHeader.biPlanes      = 1;
        bitmapInfo.bmiHeader.biBitCount    = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;
        std::vector<std::byte> pixels(static_cast<size_t>(bitmap.bmWidth) * bitmap.bmHeight * 4U);
        HDC screen       = GetDC(nullptr);
        const int copied = GetDIBits(screen, handles.color, 0, static_cast<UINT>(bitmap.bmHeight), pixels.data(),
                                     &bitmapInfo, DIB_RGB_COLORS);
        ReleaseDC(nullptr, screen);
        if (copied == bitmap.bmHeight)
        {
            auto resource = LWS::WindowIcon::FromBitmap({
                .pixels   = pixels,
                .format   = LWS::BitmapPixelFormat::Bgra8,
                .width    = static_cast<uint32_t>(bitmap.bmWidth),
                .height   = static_cast<uint32_t>(bitmap.bmHeight),
                .rowPitch = static_cast<uint32_t>(bitmap.bmWidth) * 4U,
            });
            if (resource.has_value())
                std::ignore = fWindow.SetWindowIcon(std::move(*resource));
        }
    }

    void MainWindow::UpdateNativeStatusBar(LWS::LogicalSize& canvasSize)
    {
        if (fNativeState->statusBar == nullptr)
            return;

        const bool visible = GetShowStatusBar() && fWindow.GetWindowMode() == LWS::WindowMode::Windowed;
        ShowWindow(fNativeState->statusBar, visible ? SW_SHOW : SW_HIDE);
        if (visible)
        {
            RECT statusBarRect{};
            GetWindowRect(fNativeState->statusBar, &statusBarRect);
            const auto clientArea = fWindow.GetClientAreaMetrics();
            const double scale    = clientArea.Scale().value_or(LWS::ContentScale{}).y;
            canvasSize.y -= static_cast<int32_t>(std::lround((statusBarRect.bottom - statusBarRect.top) / scale));
        }
    }

    void MainWindow::SetStatusBarText(LLUtils::native_string_type message, int part, int type)
    {
        if (fNativeState->statusBar != nullptr)
            ::SendMessage(fNativeState->statusBar, SB_SETTEXT, MAKEWORD(part, type),
                          reinterpret_cast<LPARAM>(message.c_str()));
    }

    bool MainWindow::HandleWindowEvent(const LWS::AnyEvent& eventData)
    {
        if (std::holds_alternative<LWS::EventClientAreaSizeChanged>(eventData))
            UpdateLayout();
        return false;
    }

    void MainWindow::SetIsTrayWindow(bool isTrayWindow)
    {
        SetProp(*LWS::Win32::GetHwnd(fWindow), LLUTILS_TEXT("isTrayWindow"),
                isTrayWindow ? reinterpret_cast<HANDLE>(1) : nullptr);
    }

    bool MainWindow::GetIsTrayWindow(LWS::Handle windowHandle)
    {
        return GetProp(reinterpret_cast<HWND>(windowHandle), LLUTILS_TEXT("isTrayWindow")) != nullptr;
    }
}  // namespace OIV

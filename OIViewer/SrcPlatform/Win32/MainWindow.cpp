#include "MainWindow.h"
#include "HostedWindowController.h"

#include <LLUtils/Exception.h>
#include <LWSUI/UIHost.hpp>
#include "Resource.h"

#include <Windows.h>

#include <LWS/Win32/WindowExtensions.hpp>

#include <algorithm>
#include <cmath>
#include <vector>
#include <utility>

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
        std::unique_ptr<Win32::HostedWindowController> hosted;
    };

    MainWindow::MainWindow(LWS::PlatformContext& platform, const std::filesystem::path& resources)
        : fResources(resources), fWindow(platform), fCanvasWindow(platform),
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

    LWS::Result MainWindow::Destroy()
    {
        return fNativeState->hosted ? fNativeState->hosted->Destroy() : fWindow.Destroy();
    }
    LWS::WindowMode MainWindow::GetWindowMode() const
    {
        return fNativeState->hosted ? fNativeState->hosted->GetWindowMode() : fWindow.GetWindowMode();
    }
    LWS::Result MainWindow::SetWindowMode(LWS::WindowMode mode)
    {
        if (fWindow.GetParent() && !fNativeState->hosted)
            fNativeState->hosted = std::make_unique<Win32::HostedWindowController>(*this);
        const auto result = fNativeState->hosted ? fNativeState->hosted->SetWindowMode(mode)
                                                 : fWindow.SetWindowMode(mode);
        if (result == LWS::Result::Success)
            UpdateLayout();
        return result;
    }

    void MainWindow::SetApplicationIcon()
    {
        IconResources handles;
        HMODULE module{};
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&MainWindow::GetIsTrayWindow), &module);
        handles.icon = static_cast<HICON>(
            LoadImageW(module, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
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
            const LWS::BitmapBuffer buffer{
                .pixels   = pixels,
                .format   = LWS::BitmapPixelFormat::Bgra8,
                .width    = static_cast<uint32_t>(bitmap.bmWidth),
                .height   = static_cast<uint32_t>(bitmap.bmHeight),
                .rowPitch = static_cast<uint32_t>(bitmap.bmWidth) * 4U,
            };
            fApplicationIcon = std::make_shared<LWS::Bitmap>(buffer);
            auto resource    = LWS::WindowIcon::FromBitmap(buffer);
            if (resource.has_value())
                std::ignore = fWindow.SetWindowIcon(std::move(*resource));
        }
    }

    void MainWindow::UpdateNativeWindowBorders(bool visible)
    {
        // Keep native sizing behavior even when the client paints over every resize border.
        if (fWindow.GetWindowStyles() != LWS::WindowStyle::ResizableBorder)
            std::ignore = fWindow.SetWindowStyles(LWS::WindowStyle::ResizableBorder);
        std::ignore = LWS::Win32::SetResizeBorderVisible(fWindow, visible);
    }

    void MainWindow::UpdateNativeStatusBar(LWS::LogicalSize& canvasSize)
    {
        if (fNativeState->statusBar == nullptr)
            return;

        const bool visible = GetShowStatusBar() && GetWindowMode() == LWS::WindowMode::Windowed;
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
        PrepareMenu(eventData);
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

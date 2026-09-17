#include "MainWindow.h"

#include <LWS/Platform.hpp>
#include <LWS/Wayland/WindowExtensions.hpp>
#include <LLUtils/Exception.h>

namespace OIV
{
    struct MainWindow::NativeState
    {
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
        const auto& platform = fWindow.GetPlatformContext();
        return platform.Supports(LWS::PlatformFeature::ServerSideDecorations).value_or(false) ||
               platform.Supports(LWS::PlatformFeature::HostWindowFrame).value_or(false);
    }

    int32_t MainWindow::GetImageControlClientWidth(int32_t layoutWidth) const
    {
        return layoutWidth;
    }

    void MainWindow::PrepareImageControlLayout() {}

    void MainWindow::SetApplicationIcon()
    {
        std::ignore = LWS::Wayland::SetAppId(fWindow, "io.github.openimageviewer.OpenImageViewer");
    }
    void MainWindow::UpdateNativeStatusBar([[maybe_unused]] LWS::LogicalSize& canvasSize) {}

    void MainWindow::SetStatusBarText([[maybe_unused]] LLUtils::native_string_type message, [[maybe_unused]] int part,
                                      [[maybe_unused]] int type)
    {
    }

    bool MainWindow::HandleWindowEvent(const LWS::AnyEvent& eventData)
    {
        if (std::holds_alternative<LWS::EventClientAreaSizeChanged>(eventData))
            UpdateLayout();
        return false;
    }

    void MainWindow::SetIsTrayWindow([[maybe_unused]] bool isTrayWindow) {}

    bool MainWindow::GetIsTrayWindow([[maybe_unused]] LWS::Handle windowHandle)
    {
        LL_EXCEPTION_NOT_IMPLEMENT("Tray-window properties are not implemented on Linux");
    }
}  // namespace OIV

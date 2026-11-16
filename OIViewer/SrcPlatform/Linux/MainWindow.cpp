#include "MainWindow.h"

#include <LWS/Platform.hpp>
#include <LWS/Wayland/WindowExtensions.hpp>
#include <LLUtils/Exception.h>
#include <LWSUI/UIHost.hpp>
#include "ImageBitmap.h"
#include <ImageLoader.h>
#include <ImageUtil/ImageUtil.h>
#include <exception>

namespace OIV
{
    struct MainWindow::NativeState
    {
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
        return fWindow.Destroy();
    }

    LWS::WindowMode MainWindow::GetWindowMode() const
    {
        return fWindow.GetWindowMode();
    }
    LWS::Result MainWindow::SetWindowMode(LWS::WindowMode mode)
    {
        const auto result = fWindow.SetWindowMode(mode);
        if (result == LWS::Result::Success)
            UpdateLayout();
        return result;
    }
    void MainWindow::SetApplicationIcon()
    {
        std::ignore = LWS::Wayland::SetAppId(fWindow, "io.github.openimageviewer.OpenImageViewer");
        try
        {
            IMCodec::ImageLoader loader;
            IMCodec::ImageSharedPtr image;
            if (loader.Decode((fResources / "OIViewer.ico").native(), IMCodec::ImageLoadFlags::None, {},
                              IMCodec::PluginTraverseMode::AnyFileType, image) == IMCodec::ImageResult::Success &&
                image)
            {
                image = IMUtil::ImageUtil::ConvertImageWithNormalization(image, IMCodec::TexelFormat::I_B8_G8_R8_A8,
                                                                         false);
                if (image)
                    fApplicationIcon = std::make_shared<LWS::Bitmap>(MakeBgraBitmapView(*image));
            }
        }
        catch (const std::exception&)
        {
            // A missing or unreadable optional icon must not prevent the viewer from opening.
        }
    }
    void MainWindow::UpdateNativeWindowBorders(bool visible)
    {
        const auto styles = LWS::WindowStyleFlags(visible ? LWS::WindowStyle::ResizableBorder
                                                          : LWS::WindowStyle::NoStyle);
        if (fWindow.GetWindowStyles() != styles)
            std::ignore = fWindow.SetWindowStyles(styles);
    }

    void MainWindow::UpdateNativeStatusBar([[maybe_unused]] LWS::LogicalSize& canvasSize) {}

    void MainWindow::SetStatusBarText([[maybe_unused]] LLUtils::native_string_type message, [[maybe_unused]] int part,
                                      [[maybe_unused]] int type)
    {
    }

    bool MainWindow::HandleWindowEvent(const LWS::AnyEvent& eventData)
    {
        PrepareMenu(eventData);
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

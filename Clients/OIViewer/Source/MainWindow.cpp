#include "MainWindow.h"

#include <cmath>
#include <utility>

namespace OIV
{
    LWS::Result MainWindow::Create(const LWS::WindowConfig& config)
    {
        SetApplicationIcon();
        const LWS::Result result = fWindow.Create(config);
        if (result == LWS::Result::Success)
            return OnCreate();
        return result;
    }

    void MainWindow::SetCursorType(CursorType type)
    {
        if (type == fCurrentCursorType || type < CursorType::SystemDefault || type >= CursorType::Count)
            return;

        fCurrentCursorType = type;
        if (type == CursorType::SystemDefault)
            std::ignore = fWindow.SetMouseCursor(LWS::Cursor::FromShape(LWS::CursorShape::Arrow));
        else
            std::ignore = fWindow.SetMouseCursor(fCursors[static_cast<size_t>(type) - 1]);
    }

    LWS::Result MainWindow::OnCreate()
    {
        fUseMainWindowAsCanvas = UseMainWindowAsCanvas();
        if (!fUseMainWindowAsCanvas)
        {
            const LWS::WindowConfig canvasConfig{
                .parent      = &fWindow,
                .position    = LWS::Point{0, 0},
                .transparent = true,
            };
            const LWS::Result result = fCanvasWindow.Create(canvasConfig);
            if (result != LWS::Result::Success)
            {
                std::ignore = fWindow.Destroy();
                return result;
            }
        }
        UpdateLayout();
        return LWS::Result::Success;
    }

    bool MainWindow::GetShowImageControl() const
    {
        return fShowImageControl;
    }

    bool MainWindow::GetShowStatusBar() const
    {
        return fShowStatusBar &&
               ((fWindow.GetWindowStyles() & (LWS::WindowStyle::Caption | LWS::WindowStyle::CloseButton |
                                              LWS::WindowStyle::MinimizeButton | LWS::WindowStyle::MaximizeButton)) !=
                LWS::WindowStyle::NoStyle);
    }

    void MainWindow::UpdateLayout()
    {
        LWS::LogicalSize canvasSize      = fWindow.GetClientAreaMetrics().logical;
        constexpr int32_t imageListWidth = 160;
        if (fShowImageControl)
            canvasSize.x -= imageListWidth;

        UpdateNativeStatusBar(canvasSize);
        if (!fUseMainWindowAsCanvas)
            std::ignore = fCanvasWindow.RequestPlacement({.position = LWS::Point{0, 0}, .clientSize = canvasSize});

        if (fImageControl.GetWindow().IsCreated())
        {
            if (fShowImageControl)
                std::ignore = fImageControl.GetWindow().RequestPlacement(
                    {.position   = LWS::Point{canvasSize.x, 0},
                     .clientSize = LWS::LogicalSize{GetImageControlClientWidth(imageListWidth), canvasSize.y}});
            std::ignore = fImageControl.GetWindow().SetVisible(fShowImageControl);
        }
    }

    void MainWindow::ShowStatusBar(bool show)
    {
        if (show != fShowStatusBar)
        {
            fShowStatusBar = show;
            UpdateLayout();
        }
    }

    void MainWindow::SetShowImageControl(bool show)
    {
        if (show == fShowImageControl)
            return;

        if (show && !fImageControl.GetWindow().IsCreated())
        {
            const LWS::WindowConfig imageControlConfig{
                .parent          = &fWindow,
                .position        = LWS::Point{0, 0},
                .eraseBackground = fImageControl.GetWindow().GetPlatformContext().GetBackendId() !=
                                   LWS::BackendId::Wayland,
            };
            if (fImageControl.GetWindow().Create(imageControlConfig) != LWS::Result::Success)
                return;
            PrepareImageControlLayout();
        }
        fShowImageControl = show;
        UpdateLayout();
    }

    LWS::PixelSize MainWindow::GetCanvasPixelSize() const
    {
        const auto& canvas = fUseMainWindowAsCanvas ? fWindow : fCanvasWindow;
        const auto size    = canvas.GetClientAreaMetrics();
        return size.pixels.value_or(LWS::PixelSize{size.logical.x, size.logical.y});
    }

    LWS::Point MainWindow::GetCanvasMousePosition() const
    {
        const auto& canvas            = fUseMainWindowAsCanvas ? fWindow : fCanvasWindow;
        const auto size               = canvas.GetClientAreaMetrics();
        const LWS::Point position     = canvas.GetMousePosition();
        const LWS::ContentScale scale = size.Scale().value_or(LWS::ContentScale{});
        return {static_cast<int32_t>(std::lround(position.x * scale.x)),
                static_cast<int32_t>(std::lround(position.y * scale.y))};
    }

    LWS::Point MainWindow::GetWindowMousePosition() const
    {
        const auto size               = fWindow.GetClientAreaMetrics();
        const LWS::Point position     = fWindow.GetMousePosition();
        const LWS::ContentScale scale = size.Scale().value_or(LWS::ContentScale{});
        return {static_cast<int32_t>(std::lround(position.x * scale.x)),
                static_cast<int32_t>(std::lround(position.y * scale.y))};
    }

    ImageControl& MainWindow::GetImageControl()
    {
        return fImageControl;
    }

    LWS::Window& MainWindow::GetCanvasWindow()
    {
        return fUseMainWindowAsCanvas ? fWindow : fCanvasWindow;
    }

    void MainWindow::ShowCanvas()
    {
        if (!fUseMainWindowAsCanvas)
            std::ignore = fCanvasWindow.SetVisible(true);
    }
}  // namespace OIV

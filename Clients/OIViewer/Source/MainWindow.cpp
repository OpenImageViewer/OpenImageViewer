#include "MainWindow.h"

#include <ImageLoader.h>
#include <ImageUtil/ImageUtil.h>
#include <LLUtils/PlatformUtility.h>

#include <cmath>
#include <exception>
#include <filesystem>
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

        if (type != CursorType::SystemDefault && !fCursorsInitialized)
            InitializeCursors();

        fCurrentCursorType = type;
        if (type == CursorType::SystemDefault)
            std::ignore = fWindow.SetMouseCursor(LWS::Cursor::FromShape(LWS::CursorShape::Arrow));
        else
            std::ignore = fWindow.SetMouseCursor(fCursors[static_cast<size_t>(type) - 1]);
    }

    void MainWindow::InitializeCursors()
    {
        // Load once on first use so ordinary startup does not decode auto-scroll artwork.
        // These bundled cursors all have centered hotspots. Keep the system shape if loading fails.
        constexpr std::array names{"ArrowC.cur", "ArrowE.cur",  "ArrowNE.cur", "ArrowN.cur", "ArrowNW.cur",
                                   "ArrowW.cur", "ArrowSW.cur", "ArrowS.cur",  "ArrowSE.cur"};
        const auto folder = std::filesystem::path(LLUtils::PlatformUtility::GetExeFolder()) / "Resources" / "Cursors";
        IMCodec::ImageLoader loader;
        for (size_t index = 0; index < names.size(); ++index)
        {
            try
            {
                IMCodec::ImageSharedPtr image;
                if (loader.Decode((folder / names[index]).native(), IMCodec::ImageLoadFlags::None, {},
                                  IMCodec::PluginTraverseMode::AnyFileType, image) == IMCodec::ImageResult::Success &&
                    image)
                {
                    image = IMUtil::ImageUtil::ConvertImageWithNormalization(image, IMCodec::TexelFormat::I_B8_G8_R8_A8,
                                                                             false);
                    if (image != nullptr)
                    {
                        const LWS::BitmapBuffer bitmap{
                            .pixels   = {image->GetBuffer(),
                                         static_cast<size_t>(image->GetRowPitchInBytes()) * image->GetHeight()},
                            .format   = LWS::BitmapPixelFormat::Bgra8,
                            .rowOrder = LWS::BitmapRowOrder::TopDown,
                            .width    = image->GetWidth(),
                            .height   = image->GetHeight(),
                            .rowPitch = image->GetRowPitchInBytes(),
                        };
                        // CUR files embed a hotspot, but we currently assume the cursor's center.
                        // This matches the bundled assets. A cross-platform loader should preserve the
                        // embedded hotspot alongside the decoded bitmap and pass it to LWS explicitly.
                        if (auto cursor = LWS::Cursor::FromBitmap(bitmap, {static_cast<int32_t>(bitmap.width / 2),
                                                                           static_cast<int32_t>(bitmap.height / 2)}))
                        {
                            fCursors[index] = *cursor;
                            if (index == 0)
                                fCursors[static_cast<size_t>(CursorType::SizeAll) - 1] = *cursor;
                        }
                    }
                }
            }
            catch (const std::exception&)
            {
                // Cursor artwork is optional. Leave this entry on its system fallback.
            }
        }
        fCursorsInitialized = true;
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

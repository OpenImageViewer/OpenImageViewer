#include <OIVAppCore/MessageHelper.h>
#include <LLUtils/StringUtility.h>
#include "ViewerApplication.h"
#include "ImageBitmap.h"
#include "SettingsController.h"
#include "ViewerResources.h"
#include "ViewerApplicationPlatformState.h"
#include <LLUtils/Exception.h>
#include <ImageUtil/ImageUtil.h>
#include <algorithm>
#include <cmath>
namespace OIV
{
    ViewerApplication::ViewerApplication(LWS::PlatformContext& platform, const UserDirectories& directories)
        : fPlatform(platform), fUserDirectories(directories),
          fSession(CreateViewerFileWatcher(), [&platform](std::move_only_function<void()> task)
                   { return platform.PostTask(std::move(task)) == LWS::Result::Success; }),
          fTimerTopMostRetention(platform),
          fViewer(platform, fSession, directories, ResolveViewerResources(directories),
                  {
                      .created = [this]
                      { std::ignore = fViewer.fWindow.GetWindow().Center(LWS::CenterTarget::PrimaryMonitor); },
                      .ready =
                          [this]
                      {
#ifdef LWS_HAS_WIN32_BACKEND
                          fViewer.EnableRawMouse();
#endif
                          InitializeNotificationIcons();
                          fNotificationContextMenu = std::make_unique<ContextMenu<int>>(fViewer.fWindow.GetWindow());
                          fNotificationContextMenu->AddItem(LLUTILS_TEXT("Quit"), int{});
                          std::ignore = fTimerTopMostRetention.SetTargetWindow(&fViewer.fWindow.GetWindow());
                          fTimerTopMostRetention.SetCallback(fViewer.MakeSafeCallback([this] { ProcessTopMost(); }));
                          StartFileAssociationOperation(FileAssociationAction::Check);
                      },
                      // Unsubscribe native desktop callbacks while the viewer still exists. These services precede
                      // the viewer in ownership order and can otherwise outlive its C++ subobject during teardown.
                      .stop =
                          [this]
                      {
                          StopFileAssociationOperation();
                          fResizeLifetime.reset();
                          fTimerTopMostRetention.SetInterval(0);
#ifdef LWS_HAS_WIN32_BACKEND
                          if (fNativeWindowState)
                              fNativeWindowState->notificationSubscription.Unsubscribe();
#endif
                          fNotificationContextMenu.reset();
                      },
                      .close        = [this](bool tray) { CloseApplication(tray); },
                      .destroyed    = [this] { fPlatform.RequestQuit(); },
                      .showSettings = [this] { ShowSettings(); },
                      .settingsReloaded =
                          [this]
                      {
                          if (fSettings)
                              fSettings->ApplyPending([this](const auto& id, const auto& value)
                                                      { ApplyEditorSetting(id, LWSSettings::FormatValue(value)); });
                          if (fResizeForm)
                              fResizeForm->SetThumbwheelSettings(fViewer.fResizeDialogThumbwheel);
                      },
                      .topMost =
                          [this]
                      {
                          if (fViewer.fWindow.GetWindow().SetAlwaysOnTop(true) == LWS::Result::Success)
                          {
                              fTopMostCounter = 3;
                              SetTopMostUserMesage();
                              fTimerTopMostRetention.SetInterval(1000);
                          }
                      },
                      .showResizeForm = [this] { ShowResizeForm(); },
                      .showAbout =
                          [this]
                      {
                          if (!fAbout)
                              fAbout = std::make_unique<AboutForm>(fPlatform);
                          if (fAbout->Show(fViewer.fWindow.GetWindow(), fViewer.fWindow.ApplicationIcon(),
                                           LLUtils::StringUtility::ConvertString<std::string>(
                                               MessageHelper::GetFileTime(fViewer.GetApplicationModulePath()))) !=
                              LWS::Result::Success)
                              fViewer.ReportSettingsError("Unable to open About OIViewer");
                      },
                      .saveInterfacePreferences =
                          [this](const InterfacePreferences& preferences)
                      {
                          if (auto result = SaveInterfacePreferences(fViewer.fSettingsFile, preferences); !result)
                              fViewer.ReportSettingsError(result.error());
                      },
                      .openExternalFile =
                          [this](LLUtils::native_string_type path)
                      {
                          fViewer.LoadFile(std::move(path), IMCodec::PluginTraverseMode::SuitablePlugin);
                          std::ignore = fViewer.fWindow.SetVisible(true);
                      },
                      .fileAssociations = [this](const auto& request, auto& result)
                      { CMD_FileAssociations(request, result); },
                      .associationNotice = [this](const auto& notice) { HandleFileAssociationNotice(notice); },
                  })
    {
        InitializeShellState();
    }
    ViewerApplication::~ViewerApplication()
    {
        fResizeLifetime.reset();
        if (fResizeWorker.joinable())
            fResizeWorker.join();
        fAssociationPrompt.reset();
        fAbout.reset();
        fSettings.reset();
        fResizeForm.reset();
        fViewer.Stop();
    }

    void ViewerApplication::ApplyEditorSetting(const std::string& id, const std::string& value)
    {
        fViewer.ApplyEditorSetting(id, value);
        if (fResizeForm && id.starts_with("resizedialog/thumbwheel"))
            fResizeForm->SetThumbwheelSettings(fViewer.fResizeDialogThumbwheel);
    }
    bool ViewerApplication::ShowSettings(std::string_view target)
    {
        if (!fSettings)
        {
            std::set<std::string> extensions;
            for (const auto& extension : fSession.KnownFileTypes())
                extensions.insert(LLUtils::StringUtility::ToAString(extension));
            fSettings = std::make_unique<SettingsController>(
                fPlatform, fViewer.fResources, fViewer.fSettingsFile,
                [this](const auto& id, const auto& value) { ApplyEditorSetting(id, LWSSettings::FormatValue(value)); },
                std::move(extensions), fUserDirectories.config);
        }
        const auto result = fSettings->Show(&fViewer.GetWindow().GetWindow(), target);
        if (!result)
            fViewer.ReportSettingsError(result.error().location + ": " + result.error().message);
        return bool(result);
    }

    void ViewerApplication::ShowResizeForm(bool updateExisting)
    {
        if (fResizeBusy && fResizeForm)
        {
            fResizeForm->Show(&fViewer.GetWindow().GetWindow());
            return;
        }
        ResizeFormInput input;
        input.thumbwheel = fViewer.fResizeDialogThumbwheel;
        IMCodec::ImageSharedPtr icon;
        if (fSession.Loader().Decode((fViewer.fResources / "resize-picture.png").native(), IMCodec::ImageLoadFlags{},
                                     {}, IMCodec::PluginTraverseMode::SuitablePlugin,
                                     icon) == IMCodec::ImageResult::Success)
        {
            auto bgra = IMUtil::ImageUtil::Convert(icon, IMCodec::TexelFormat::I_B8_G8_R8_A8);
            if (bgra)
                input.iconBitmap = std::make_shared<LWS::Bitmap>(MakeBgraBitmapView(*bgra));
        }
        input.onDismiss  = [this] { ++fResizeRequestId; };
        input.onFileDrop = [this](const LWS::EventDragDropFile& drop)
        {
            // Finish the native drop before loading and resetting the existing controls.
            std::ignore = fPlatform.PostTask(
                [this, lifetime = std::weak_ptr<int>(fResizeLifetime), drop]
                {
                    if (lifetime.expired() || fResizeBusy)
                        return;
                    const auto generation = fSession.Image().GetSourceGeneration();
                    if (fViewer.HandleFileDragDropEvent(drop) && generation != fSession.Image().GetSourceGeneration())
                        ShowResizeForm(true);
                });
        };
        const auto& image = fViewer.fSession.Image().GetRetainedImage(ImageProcessingStage::RenderFormat);
        if (image)
        {
            const auto width  = static_cast<int32_t>(image->GetWidth());
            const auto height = static_cast<int32_t>(image->GetHeight());
            const LLUtils::RectI32 imageBounds{{0, 0}, {width, height}};
            auto bounds          = imageBounds;
            const auto selection = fViewer.fSelectionRect.GetSelectionRect();
            if (!selection.IsEmpty())
            {
                const auto selected    = fViewer.ClientToImageRounded(selection);
                const auto topLeft     = selected.GetCorner(LLUtils::TopLeft);
                const auto bottomRight = selected.GetCorner(LLUtils::BottomRight);
                bounds                 = {{std::clamp(topLeft.x, 0, width), std::clamp(topLeft.y, 0, height)},
                                          {std::clamp(bottomRight.x, 0, width), std::clamp(bottomRight.y, 0, height)}};
                if (bounds.IsEmpty())
                    bounds = imageBounds;
            }

            // Keep the form's preview small while using full-resolution image coordinates for its size math.
            constexpr int32_t maxPreviewWidth  = 560;
            constexpr int32_t maxPreviewHeight = 480;
            const double scale                 = std::min(static_cast<double>(maxPreviewWidth) / bounds.GetWidth(),
                                                          static_cast<double>(maxPreviewHeight) / bounds.GetHeight());
            const LLUtils::PointI32 previewSize{
                std::max(1, static_cast<int32_t>(std::lround(bounds.GetWidth() * scale))),
                std::max(1, static_cast<int32_t>(std::lround(bounds.GetHeight() * scale))),
            };
            // Filter alpha with the image and retain its association for the UI bitmap adapter.
            auto resized = ResizeImage(image, bounds,
                                       {.size       = previewSize,
                                        .aspect     = ResizeAspectRatio::DoNotPreserve,
                                        .autoFilter = false,
                                        .filter     = ResizeFilter::Box});
            if (!resized)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "Unable to resize preview image");
            auto previewImage = IMUtil::ImageUtil::ConvertImageWithNormalization(*resized,
                                                                                 IMCodec::TexelFormat::I_B8_G8_R8_A8,
                                                                                 false);
            if (previewImage)
            {
                input.sourceBitmap   = std::make_shared<LWS::Bitmap>(MakeBgraBitmapView(*previewImage));
                input.originalBounds = bounds;
            }
            // Use oriented, transfer-prepared pixels rather than the display-normalized preview for final output.
            auto source        = fSession.Image().GetRetainedImage(ImageProcessingStage::TransferFunction);
            const auto* opened = fSession.Image().GetOpenedImage();
            if (source && opened)
            {
                input.originalBounds = bounds;
                input.onResize       = [this, source = std::move(source), bounds, opened = *opened,
                                  generation = fSession.Image().GetSourceGeneration()](ImageResizeRequest request)
                { StartResize(source, bounds, opened, generation, request); };
            }
        }

        if (updateExisting && fResizeForm)
            fResizeForm->Reinitialize(std::move(input));
        else
        {
            fResizeForm = std::make_unique<ResizeFormController>(fPlatform, fViewer.fResources, std::move(input));
            fResizeForm->Show(&fViewer.GetWindow().GetWindow());
        }
    }

    void ViewerApplication::StartResize(IMCodec::ImageSharedPtr source, LLUtils::RectI32 bounds, ViewerImage opened,
                                        uint64_t generation, ImageResizeRequest request)
    {
        if (fResizeBusy)
            return;
        if (generation != fSession.Image().GetSourceGeneration() ||
            source != fSession.Image().GetRetainedImage(ImageProcessingStage::TransferFunction))
        {
            fResizeForm->SetError("The image changed. Reopen Resize image to use the current image.");
            return;
        }
        if (fResizeWorker.joinable())
            fResizeWorker.join();
        const auto requestId              = ++fResizeRequestId;
        const std::weak_ptr<int> lifetime = fResizeLifetime;
        fResizeBusy                       = true;
        fResizeForm->SetBusy(true);
        try
        {
            fResizeWorker = std::thread(
                [this, lifetime, source = std::move(source), bounds, opened = std::move(opened), generation, request,
                 requestId]() mutable
                {
                    auto resized = ResizeImage(source, bounds, request);
                    // PostTask is thread-safe; all viewer and form mutations remain on the UI thread.
                    std::ignore = fPlatform.PostTask(
                        [this, lifetime, source = std::move(source), resized = std::move(resized),
                         opened = std::move(opened), generation, requestId]() mutable
                        {
                            if (lifetime.expired())
                                return;
                            fResizeBusy = false;
                            fResizeForm->SetBusy(false);
                            if (requestId != fResizeRequestId)
                                return;
                            if (generation != fSession.Image().GetSourceGeneration() ||
                                source != fSession.Image().GetRetainedImage(ImageProcessingStage::TransferFunction))
                            {
                                fResizeForm->SetError(
                                    "The image changed. Reopen Resize image to use the current image.");
                                return;
                            }
                            if (!resized)
                            {
                                fResizeForm->SetError(resized.error());
                                return;
                            }
                            auto result = ViewerImage{.pixels     = std::move(*resized),
                                                      .metadata   = opened.metadata,
                                                      .source     = ImageSource::GeneratedByLib,
                                                      .sourceName = opened.sourceName};
                            fResizeForm->Hide();
                            fViewer.LoadOivImage(std::move(result));
                            fViewer.CancelSelection();
                            fViewer.FitToClientAreaAndCenter();
                        });
                });
        }
        catch (const std::exception& error)
        {
            fResizeBusy = false;
            fResizeForm->SetBusy(false);
            fResizeForm->SetError(error.what());
        }
    }

    void ViewerApplication::Init(LLUtils::native_string_type path, const RendererOptions& options)
    {
        fViewer.Init(std::move(path), options, WindowConfig());
    }
    LWS::LoopResult ViewerApplication::Run()
    {
        return fPlatform.RunMessageLoop();
    }
    LWS::WindowConfig ViewerApplication::WindowConfig() const
    {
        // initialize the windowing system of the window
        // The historical 1200x800 native window was calibrated at 125% scaling. LWS uses 96-DPI logical units, so
        // this client estimate preserves that reference footprint while remaining portable.
        LWS::LogicalSize initialClientSize{946, 602};
        if (const auto monitor = fPlatform.GetPrimaryMonitor(); monitor.has_value())
        {
            double workWidth  = monitor->workRect.GetWidth();
            double workHeight = monitor->workRect.GetHeight();
#ifdef LWS_HAS_WIN32_BACKEND
            workWidth /= monitor->contentScale.x;
            workHeight /= monitor->contentScale.y;
#endif
            constexpr double edgeMargins = 64.0;
            const double scale           = std::min({1.0, std::max(workWidth - edgeMargins, 1.0) / initialClientSize.x,
                                                     std::max(workHeight - edgeMargins, 1.0) / initialClientSize.y});
            initialClientSize            = {
                std::max(1, static_cast<int32_t>(std::lround(initialClientSize.x * scale))),
                std::max(1, static_cast<int32_t>(std::lround(initialClientSize.y * scale))),
            };
        }
        return {
            .clientSize         = initialClientSize,
            .styles             = LWS::WindowStyleFlags(Viewer::WindowChromeStyles),
            .backgroundColor    = LLUtils::Color(45, 45, 48),
            .dragAndDropEnabled = fPlatform.Supports(LWS::PlatformFeature::DragAndDrop).value_or(false),
        };
    }
}  // namespace OIV

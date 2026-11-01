#include "ImageBitmap.h"
#include <LLUtils/Thread.h>
#include <array>
#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "Viewer.h"

#include <Version.h>

#include <LLUtils/Exception.h>
#include <LLUtils/FileHelper.h>
#include <LLUtils/PlatformUtility.h>
#include <LLUtils/StringUtility.h>
#include <LLUtils/FileSystemHelper.h>
#include <LLUtils/Rect.h>

#include <OIVAppCore/OIVHelper.h>
#include "Helpers/ClipboardSetup.h"
#include <OIVAppCore/MessageFormatter.h>
#include <OIVAppCore/MessageHelper.h>
#include <OIVAppCore/ShellIntegrationHelper.h>
#include "Helpers/ShellCommandHandler.h"

#include <ImageProcessing/TextRasterizer.h>
#include <ImageProcessing/RawImage.h>
#include <ImageUtil/ImageUtil.h>
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"

#include "ContextMenu.h"
#include "Globals.h"
#include <OIVAppCore/ConfigurationLoader.h>
#include "CommandRegistry.h"
#include <OIVAppCore/ColorCorrectionCommandPolicy.h>
#include <OIVAppCore/FileChangePolicy.h>
#include <OIVAppCore/FrameLimiterPolicy.h>
#include <OIVAppCore/ImageEditPolicy.h>
#include <OIVAppCore/ImageFormatCatalogPolicy.h>
#include <OIVAppCore/ImageLoadPresentationPolicy.h>
#include <OIVAppCore/ImageTransformCommandPolicy.h>
#include <OIVAppCore/InputGesturePolicy.h>
#include <OIVAppCore/SelectionWorkflowPolicy.h>
#include <OIVAppCore/SequencerPolicy.h>
#include <OIVAppCore/SortCommandPolicy.h>
#include <OIVAppCore/SubImagePolicy.h>
#include <OIVAppCore/ViewActionController.h>
#include <OIVAppCore/ViewCommandPolicy.h>
#include <OIVAppCore/ViewerPresentationPolicy.h>
#include <OIVAppCore/ViewerCompletion.h>

namespace OIV
{
    void Viewer::HandleReloadAction(ReloadAction action, const LLUtils::native_string_type& requestedFile)
    {
        // Native dialogs can reactivate the viewer before returning. Keep confirmation in the shared
        // policy so that reentrant activation cannot submit a second reload while a decision is pending.
        if (action == ReloadAction::AskUser)
            action = fFileReloadPolicy.ConfirmReload(ConfirmFileReload(requestedFile));
        if (action == ReloadAction::RequestNow && fSession.fBrowseSessionController != nullptr)
            fSession.fBrowseSessionController->RequestCurrentFileReload();
    }

    void Viewer::DrainUiCompletions()
    {
        auto completions = fSession.TakeCompletions();
        for (const auto& completion : completions)
        {
            if (!fIsShuttingDown)
            {
                const LLUtils::OperationScope operation(completion.operationId);
                OnMessageFromBackgroundThread(completion);
            }
        }
    }

    void Viewer::AddImageToControl(IMCodec::ImageSharedPtr image, uint16_t imageSlot, uint16_t totalImages)
    {
        // Sidebar bitmaps bypass ImageState, so apply the same sample and display conversions here.
        image = IMUtil::ImageUtil::EncodeLinearImageForDisplay(IMUtil::ImageUtil::ApplyTransferFunction(image));
        auto bitmapImage = IMUtil::ImageUtil::ConvertImageWithNormalization(image, IMCodec::TexelFormat::I_B8_G8_R8_A8,
                                                                            false);
        if (bitmapImage == nullptr)
            return;

        const auto bitmapBuffer = MakeBgraBitmapView(*bitmapImage);

        LLUtils::native_stringstream title;
        title << imageSlot + 1 << LLUTILS_TEXT('/') << totalImages << LLUTILS_TEXT("  ") << bitmapBuffer.width
              << LLUTILS_TEXT(" x ") << bitmapBuffer.height << LLUTILS_TEXT(" x 32 BPP");
        fWindow.GetImageControl().GetImageList().SetImage(
            {.index = imageSlot, .title = title.str(), .bitmap = std::make_shared<LWS::Bitmap>(bitmapBuffer)});
    }

    void Viewer::UnloadOpenedImaged()
    {
        if (fSession.fBrowseSessionController != nullptr)
            fSession.fBrowseSessionController->InvalidateCurrent();
        fSequencerTimer.SetInterval(0);
        fSession.fSequencerPolicy.Reset();
        fSession.fImageState.ClearAll();
        UpdateMainRenderImages();
        fRefreshOperation.Queue();
        UpdateOpenImageUI();
    }

    void Viewer::RefreshImage()
    {
        fRefreshOperation.Begin();
        fSession.fImageState.Refresh();
        UpdateMainRenderImages();
        if (fSession.fImageState.IsDownscaled())
            QueueResampling();
        fRefreshOperation.End(true);
    }

    void Viewer::DisplayOpenedFileName()
    {
        if (IsOpenedImageIsAFile())
            SetUserMessage(ViewerPresentationPolicy::FormatOpenedFileMessage(
                               MessageFormatter::FormatFilePath(GetOpenedFileName())),
                           static_cast<GroupID>(UserMessageGroups::SuccessfulFileLoad),
                           MessageFlags::Interchangeable | MessageFlags::Moveable);
    }

    bool Viewer::IsSubImagesVisible() const
    {
        using namespace IMCodec;
        if (fSession.fImageState.GetOpenedImage() != nullptr)
        {
            const auto mainImage = fSession.fImageState.GetOpenedImage()->pixels;
            return mainImage != nullptr &&
                   SubImagePolicy::IsVisible(mainImage->GetSubImageGroupType(), mainImage->GetNumSubImages());
        }
        else
        {
            return false;
        }
    }

    void Viewer::LoadSubImages(bool restoreSelection)
    {
        using namespace IMCodec;
        auto mainImage    = fSession.fImageState.GetOpenedImage();
        auto numSubImages = mainImage->pixels->GetNumSubImages();
        if (IsSubImagesVisible())
        {
            const auto isMainAnActualImage = SubImagePolicy::IncludeMainImage(mainImage->pixels->GetItemType());
            const uint16_t totalImages     = SubImagePolicy::TotalDisplayedImages(mainImage->pixels->GetNumSubImages(),
                                                                                  isMainAnActualImage);
            // Add the first image.
            uint16_t currentImage = 0;
            if (isMainAnActualImage)
                AddImageToControl(mainImage->pixels, static_cast<uint16_t>(currentImage++), totalImages);

            std::vector<uint64_t> subImagePixels;
            subImagePixels.reserve(numSubImages);
            for (uint16_t i = 0; i < numSubImages; i++)
            {
                auto currentSubImage = mainImage->pixels->GetSubImage(i);
                subImagePixels.push_back(currentSubImage->GetTotalPixels());
                AddImageToControl(currentSubImage, static_cast<uint16_t>(currentImage++), totalImages);
            }
            // Reset selected sub image when loading new set of subimages
            const int selectionIndex = fSession.fDisplayBiggestSubImageOnLoad
                                           ? SubImagePolicy::InitialSelectionIndex(isMainAnActualImage,
                                                                                   mainImage->pixels->GetTotalPixels(),
                                                                                   subImagePixels)
                                           : SubImagePolicy::MainImageIndex;
            int selected             = selectionIndex;
            if (restoreSelection)
            {
                selected = SubImagePolicy::MainImageIndex;
                for (uint16_t i = 0; i < numSubImages; ++i)
                    if (mainImage->pixels->GetSubImage(i) == fSession.fImageState.GetSelectedSource())
                        selected = i + (isMainAnActualImage ? 1 : 0);
            }
            fWindow.GetImageControl().GetImageList().SetSelected(selected);
            fWindow.GetImageControl().RefreshScrollInfo();
        }
        else
        {
            fWindow.GetImageControl().GetImageList().Clear();
        }
    }

    bool Viewer::LoadFile(LLUtils::native_string_type filePath, IMCodec::PluginTraverseMode loaderFlags)
    {
        const auto clientSize = fWindow.GetWindow().GetClientAreaMetrics().logical;
        return ProcessImageLoadResult(fSession.fImageOpenController->LoadFile(
            filePath, loaderFlags, ImageLoadContext{static_cast<int>(clientSize.x), static_cast<int>(clientSize.y)}));
    }

    bool Viewer::ProcessImageLoadResult(const ImageLoadResult& loadResult)
    {
        const LLUtils::OperationScope operation(loadResult.operationId.value ? loadResult.operationId
                                                                             : LLUtils::OperationScope::Current());
        if (loadResult.status == ImageLoadStatus::FolderLoadQueued)
            return true;

        if (loadResult.status == ImageLoadStatus::NoSupportedFiles)
            return false;

        auto formattedFilePath                   = MessageFormatter::FormatFilePath(loadResult.normalizedPath) +
                                                   LLUTILS_TEXT("<textcolor=#ff8930>");
        const ImageLoadPresentation presentation = ImageLoadPresentationPolicy::Decide(loadResult, formattedFilePath);

        if (presentation.shouldLoadImage)
        {
            if (fSession.fBrowseSessionController != nullptr && loadResult.image.pixels != nullptr &&
                loadResult.image.source == ImageSource::File)
            {
                const auto commitResult = fSession.fBrowseSessionController->CommitCurrentFile(
                    loadResult.normalizedPath);
                if (!commitResult)
                {
                    const ImageLoadResult commitFailure{ImageLoadStatus::UnknownError, loadResult.normalizedPath, {}};
                    const auto failurePresentation = ImageLoadPresentationPolicy::Decide(commitFailure,
                                                                                         formattedFilePath);
                    if (failurePresentation.shouldShowMessage)
                    {
                        SetUserMessage(failurePresentation.message,
                                       static_cast<GroupID>(UserMessageGroups::FailedFileLoad),
                                       MessageFlags::Persistent);
                    }
                    return false;
                }
            }

            LoadOivImage(loadResult.image);
        }

        if (presentation.shouldShowMessage)
            SetUserMessage(presentation.message, static_cast<GroupID>(UserMessageGroups::FailedFileLoad),
                           MessageFlags::Persistent);

        return presentation.succeeded;
    }

    LLUtils::native_string_type Viewer::DescribeImage(const IMCodec::ImageSharedPtr& pixels)
    {
        LLUtils::native_stringstream stream;
        stream << pixels->GetWidth() << LLUTILS_TEXT(" X ") << pixels->GetHeight() << LLUTILS_TEXT(" X ")
               << pixels->GetBitsPerTexel() << LLUTILS_TEXT(" BPP | loaded in ") << std::fixed << std::setprecision(1)
               << pixels->GetImageItem()->processData.processTime << LLUTILS_TEXT(" ms");
        return stream.str();
    }

    void Viewer::LoadOivImage(ViewerImage oivImage)
    {
        // Enter this function only from the main thread.
        assert("Viewer::FinalizeImageLoad() can be called only from the main thread" && IsMainThread());

        if (oivImage.pixels == nullptr)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Expected a valid image");

        fFileDisplayTimer.Start();

        fSequencerTimer.SetInterval(0);
        fQueueImageInfoLoad = GetImageInfoVisible();
        SetImageInfoVisible(false);
        SetResamplingEnabled(false);
        fSession.OpenImage(oivImage);

        fRefreshOperation.Begin();

        fSession.fImageState.Refresh(ImageProcessingStage::GeometricTransform);

        if (fSession.fResetTransformationMode == ResetTransformationMode::ResetAll)
            FitToClientAreaAndCenter();

        AutoPlaceImage();

        fSession.fImageState.Refresh();
        UpdateMainRenderImages();
        fWindow.SetShowImageControl(IsSubImagesVisible());
        UnloadWelcomeMessage();
        DisplayOpenedFileName();

        if (fContextMenu != nullptr)
        {
            fContextMenu->EnableItem(LLUTILS_TEXT("Open containing folder"), oivImage.source == ImageSource::File);
            fContextMenu->EnableItem(LLUTILS_TEXT("Open in photoshop"), oivImage.source == ImageSource::File);
        }

        fRefreshOperation.End();
        fFileDisplayTimer.Stop();

        LoadSubImages();

        fDisplayTime = fFileDisplayTimer.GetElapsedTimeReal(LLUtils::StopWatch::TimeUnit::Milliseconds);

        fLastImageLoadTimeStamp.Start();

        if (fIsTryToLoadInitialFile == true)
        {
            std::ignore             = fWindow.GetWindow().SetVisible(true);
            fIsTryToLoadInitialFile = false;
        }

        UpdateOpenImageUI();

        SetResamplingEnabled(true);

        if (fQueueImageInfoLoad == true)
        {
            SetImageInfoVisible(true);
            fQueueImageInfoLoad = false;
        }

        if (const auto interval = fSession.StartAnimation(); interval != 0)
            fSequencerTimer.SetInterval(interval);
    }

    void Viewer::UpdateOpenImageUI()
    {
        if (IsImageOpen())
        {
            UpdateTitle();
            fVirtualStatusBar.SetText("imageDescription", DescribeImage(fSession.fImageState.GetRetainedImage(
                                                              ImageProcessingStage::GeometricTransform)));
        }
    }

    const LLUtils::native_string_type& Viewer::GetOpenedFileName() const
    {
        static const LLUtils::native_string_type emptyString;
        const auto* file = fSession.fImageState.GetOpenedImage();
        return file && file->source == ImageSource::File ? file->path : emptyString;
    }

    bool Viewer::IsImageOpen() const
    {
        return fSession.fImageState.GetOpenedImage() != nullptr;
    }

    bool Viewer::IsOpenedImageIsAFile() const
    {
        return fSession.fImageState.GetOpenedImage() != nullptr &&
               fSession.fImageState.GetOpenedImage()->source == ImageSource::File;
    }

    void Viewer::SortFolderFileList()
    {
        if (fSession.fBrowseSessionController != nullptr)
        {
            fSession.fBrowseSessionController->SortFolderFileList();
            UpdateTitle();
        }
    }

    void Viewer::ApplyBrowseSessionResult(const BrowseSessionController::BrowseSessionResult& result)
    {
        switch (result.action)
        {
            case BrowseSessionController::BrowseSessionAction::Ignore:
                break;
            case BrowseSessionController::BrowseSessionAction::DisplayImage:
                OnFileIndexResidencyReady(result.fileName, result.image);
                break;
            case BrowseSessionController::BrowseSessionAction::ShowFailure:
                ProcessImageLoadResult({ImageLoadStatus::UnknownError, result.fileName, {}});
                break;
            case BrowseSessionController::BrowseSessionAction::CurrentFileRemoved:
            case BrowseSessionController::BrowseSessionAction::CurrentFileUnsupportedRename:
                ProcessRemovalOfOpenedFile(result.fileName);
                break;
        }
    }

    void Viewer::OnImageSelectionChanged(const ImageList::ImageSelectionChangeArgs& ImageSelectionChangeArgs)
    {
        if (fRestoringSession)
            return;
        auto imageIndex = ImageSelectionChangeArgs.imageIndex;
        if (imageIndex >= 0)
        {
            auto image = GetImageByIndex(imageIndex);

            fSession.fImageState.SetSelectedSource(image);
            fSession.fImageState.Refresh(ImageProcessingStage::GeometricTransform);
            fRefreshOperation.Begin();
            FitToClientAreaAndCenter();
            RefreshImage();

            if (GetImageInfoVisible() == true)
                ShowImageInfo();

            UpdateOpenImageUI();

            fRefreshOperation.End();
        }
    }

    void Viewer::ProcessRemovalOfOpenedFile(const LLUtils::native_string_type& fileName)
    {
        const bool removeInternalDeletes = (fDeletedFileRemovalMode & DeletedFileRemovalMode::DeletedInternally) ==
                                           DeletedFileRemovalMode::DeletedInternally;
        const bool removeExternalDeletes = (fDeletedFileRemovalMode & DeletedFileRemovalMode::DeletedExternally) ==
                                           DeletedFileRemovalMode::DeletedExternally;
        const size_t fileCount           = fSession.fBrowseSessionController != nullptr
                                               ? fSession.fBrowseSessionController->GetFolderFileList().GetSize()
                                               : 0;

        const auto action = FileRemovalPolicy::Decide(GetOpenedFileName(), fileName, fRequestedFileForRemoval,
                                                      removeInternalDeletes, removeExternalDeletes, fileCount);
        if (action != RemovedFileAction::Ignore)
        {
            const bool replacementRequested = (action == RemovedFileAction::TryStart ||
                                               action == RemovedFileAction::TryNextThenPrevious) &&
                                              fSession.fBrowseSessionController != nullptr &&
                                              fSession.fBrowseSessionController->RequestRemovedFileReplacement(
                                                  fileName);

            if (FileRemovalPolicy::ShouldUnloadAfterJumps(action, replacementRequested, false))
            {
                UnloadOpenedImaged();
                ShowWelcomeMessage();
            }

            fRequestedFileForRemoval = {};
        }

        UpdateTitle();
    }

    void Viewer::OnFileChangedImpl(const IFileWatcher::FileChangedEventArgs* fileChangedEventArgsPtr)
    {
        auto fileChangedEventArgs  = *fileChangedEventArgsPtr;
        const bool hasActiveFolder = fSession.fBrowseSessionController != nullptr;
        const auto activeFolderID  = hasActiveFolder ? fSession.fBrowseSessionController->GetActiveFolderID()
                                                     : IFileWatcher::FolderID{};

        switch (FileChangePolicy::Decide(fileChangedEventArgs, hasActiveFolder, activeFolderID,
                                         fSession.fConfigurationFolderID,
                                         std::filesystem::path(GetOpenedFileName()).native()))
        {
            case FileChangeAction::CurrentFileChanged:
                ProcessCurrentFileChanged();
                break;
            case FileChangeAction::ClearWatchedFolder:
                fCurrentFolderWatched.clear();
                break;
            case FileChangeAction::ReloadSettings:
                LoadSettings();
                break;
            case FileChangeAction::Ignore:
                break;
            case FileChangeAction::UnexpectedFolder:
                LL_EXCEPTION_UNEXPECTED_VALUE;
                break;
        }
    }

    void Viewer::OnFileIndexResidencyReady(const LLUtils::native_string_type& fileName, IMCodec::ImageSharedPtr image)
    {
        if (fSession.fBrowseSessionController == nullptr || !fSession.fBrowseSessionController->IsCurrentFile(fileName))
        {
            return;
        }

        if (image == nullptr)
        {
            ProcessImageLoadResult({ImageLoadStatus::UnknownError, fileName, {}});
            return;
        }

        ViewerImage file{.pixels = std::move(image),
                         .source = ImageSource::File,
                         .path   = std::filesystem::path(fileName).lexically_normal().native()};
        file.metadata = file.pixels->GetImageItem()->metaData;
        LoadOivImage(std::move(file));
    }

    void Viewer::OnCountingColorsCompleted(const CountColorsData& result)
    {
        if (fSession.CompleteColorCount(result, GetImageInfoVisible()) && GetImageInfoVisible())
            ShowImageInfo();
    }

    void Viewer::OnMessageFromBackgroundThread(const ViewerCompletion& completion)
    {
        std::visit(
            [this](const auto& data)
            {
                using T = std::decay_t<decltype(data)>;
                if constexpr (std::is_same_v<T, FileAssociationNotice>)
                {
                    if (fShell.associationNotice)
                        fShell.associationNotice(data);
                }
                else if constexpr (std::is_same_v<T, IFileWatcher::FileChangedEventArgs>)
                {
                    OnFileChangedImpl(&data);
                    BrowseSessionController::BrowseSessionResult result;
                    if (fSession.fBrowseSessionController)
                        result = fSession.fBrowseSessionController->OnFileChanged(data);
                    UpdateTitle();
                    ApplyBrowseSessionResult(result);
                }
                else if constexpr (std::is_same_v<T, FileIndexResidencyReadyData>)
                    OnFileIndexResidencyReady(data.fileName, data.image);
                else if constexpr (std::is_same_v<T, CandidateResidencyReadyData>)
                {
                    const auto result = !fSession.fBrowseSessionController
                                            ? BrowseSessionController::BrowseSessionResult{}
                                        : data.folderLoad
                                            ? fSession.fBrowseSessionController->OnFolderOpenCandidateReady(data)
                                            : fSession.fBrowseSessionController->OnBrowseCandidateReady(data);
                    ApplyBrowseSessionResult(result);
                }
                else if constexpr (std::is_same_v<T, CountColorsData>)
                    OnCountingColorsCompleted(data);
            },
            completion.data);
    }

    bool Viewer::JumpFiles(FolderFileList::index_type step)
    {
        return fSession.fBrowseSessionController != nullptr && fSession.fBrowseSessionController->JumpFiles(step);
    }

    void Viewer::LoadRaw(const std::byte* buffer, uint32_t width, uint32_t height, uint32_t rowPitch,
                         IMCodec::TexelFormat texelFormat)
    {
        const RawBufferParams params{buffer, width, height, rowPitch, texelFormat};
        auto pixels = CreateRawImage(params, {IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Vertical});
        LoadOivImage(ViewerImage{.pixels = std::move(pixels), .source = ImageSource::Clipboard});
    }

    void Viewer::LoadClipboardText(const LLUtils::native_string_type& text)
    {
        auto textImage = std::make_unique<TextRasterizer>(fFreeType.get());
        textImage->SetFontPath(fLabelManager.GetFontPath());
        textImage->SetText(text);
        textImage->SetDPI(static_cast<uint32_t>(std::lround(fDPIadjustmentFactor.x * 96.0)),
                          static_cast<uint32_t>(std::lround(fDPIadjustmentFactor.y * 96.0)));
        textImage->SetFontSize(10);
        textImage->SetOutlineWidth(0);
        textImage->SetTextColor({48, 48, 48, 255});
        textImage->SetUseMetaText(false);
        textImage->SetBackgroundColor(LLUtils::Color(255, 255, 255, 255));
        if (auto pixels = textImage->GetBitmap())
            LoadOivImage(ViewerImage{.pixels = std::move(pixels), .source = ImageSource::ClipboardText});
    }

    OperationResult Viewer::CopyVisibleToClipBoard()
    {
        OperationResult result = ImageEditPolicy::ValidateSelectionOperation(
            IsImageOpen(), fSelectionRect.GetSelectionRect().IsEmpty());
        if (result == OperationResult::Success)
        {
            result                               = OperationResult::UnkownError;
            LLUtils::RectI32 imageSpaceSelection = ClientToImageRounded(fSelectionRect.GetSelectionRect());
            auto cropped                         = IMUtil::ImageUtil::CropImage(
                fSession.fImageState.GetRetainedImage(ImageProcessingStage::RenderFormat), imageSpaceSelection);

            if (cropped != nullptr)
            {
                // 2. Flip the image vertically and convert it to BGRA for the clipboard.
                auto flipped = IMUtil::ImageUtil::Transform(
                    {IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Vertical}, cropped);
                if (flipped != nullptr && SetClipboardImage(flipped))
                    result = OperationResult::Success;
            }
        }
        return result;
    }

    OperationResult Viewer::CropVisibleImage()
    {
        OperationResult result = ImageEditPolicy::ValidateSelectionOperation(
            IsImageOpen(), fSelectionRect.GetSelectionRect().IsEmpty());
        if (result == OperationResult::Success)
        {
            result                        = OperationResult::UnkownError;
            LLUtils::RectI32 imageRectInt = ClientToImageRounded(fSelectionRect.GetSelectionRect());
            auto cropped                  = IMUtil::ImageUtil::CropImage(
                fSession.fImageState.GetRetainedImage(ImageProcessingStage::GeometricTransform), imageRectInt);

            if (cropped != nullptr)
            {
                auto oivCropped = ViewerImage{.pixels     = cropped,
                                              .metadata   = fSession.fImageState.GetOpenedImage()->metadata,
                                              .source     = ImageSource::GeneratedByLib,
                                              .sourceName = fSession.fImageState.GetOpenedImage()->sourceName};
                LoadOivImage(oivCropped);
                CancelSelection();
                result = OperationResult::Success;
            }
        }
        return result;
    }

    OperationResult Viewer::CutSelectedArea()
    {
        // Please note that currently this function works on the rasterized image, a more general solution is needed
        // to work on a previous stage image.
        OperationResult result = ImageEditPolicy::ValidateSelectionOperation(
            IsImageOpen(), fSelectionRect.GetSelectionRect().IsEmpty());
        if (result == OperationResult::Success)
        {
            result                        = OperationResult::UnkownError;
            auto rasterized               = fSession.fImageState.GetRetainedImage(ImageProcessingStage::RenderFormat);
            LLUtils::RectI32 subImageRect = ClientToImageRounded(fSelectionRect.GetSelectionRect());

            const LLUtils::RectI32 imageRect = {{0, 0},
                                                {static_cast<int32_t>(rasterized->GetWidth()),
                                                 static_cast<int32_t>(rasterized->GetHeight())}};

            subImageRect = subImageRect.Intersection(imageRect);

            if (subImageRect.IsEmpty() == false)
            {
                SetClipboardImage(IMUtil::ImageUtil::GetSubImage(rasterized, subImageRect));
                auto& texelInfo = IMCodec::GetTexelInfo(
                    fSession.fImageState.GetRetainedImage(ImageProcessingStage::RenderFormat)->GetOriginalTexelFormat());
                bool hasOpacityChannel = false;
                for (auto& channel : texelInfo.channles)
                    if (channel.semantic == IMCodec::ChannelSemantic::Opacity)
                    {
                        hasOpacityChannel = true;
                        break;
                    }

                const auto fillColor = hasOpacityChannel ? LLUtils::Color(0, 0, 0, 0) : LLUtils::Color(0, 0, 0, 255);
                auto colorFilled     = IMUtil::ImageUtil::FillColor(
                    fSession.fImageState.GetRetainedImage(ImageProcessingStage::RenderFormat), subImageRect, fillColor);

                if (colorFilled != nullptr)
                {
                    auto oivColorFilled = ViewerImage{.pixels     = colorFilled,
                                                      .metadata   = fSession.fImageState.GetOpenedImage()->metadata,
                                                      .source     = ImageSource::GeneratedByLib,
                                                      .sourceName = fSession.fImageState.GetOpenedImage()->sourceName};
                    auto lastState      = fSession.fResetTransformationMode;
                    fSession.fResetTransformationMode = ResetTransformationMode::DoNothing;
                    LoadOivImage(oivColorFilled);
                    fSession.fResetTransformationMode = lastState;
                    CancelSelection();
                    result = OperationResult::Success;
                }
            }
        }
        return result;
    }

    void Viewer::CompleteInitialization()
    {
        if (!fIsFirstFrameDisplayed)
        {
            fIsFirstFrameDisplayed = true;
            PostInitOperations();
        }
    }

    void Viewer::PerformReloadFile(const LLUtils::native_string_type& requestedFile)
    {
        HandleReloadAction(fFileReloadPolicy.OnPendingReloadRequested(requestedFile), requestedFile);
    }

    void Viewer::ProcessCurrentFileChanged()
    {
        HandleReloadAction(fFileReloadPolicy.OnCurrentFileChanged(GetOpenedFileName(), GetAppActive()),
                           GetOpenedFileName());
    }

    bool Viewer::LoadFileOrFolder(const LLUtils::native_string_type& filePath, IMCodec::PluginTraverseMode traverseMode)
    {
        const auto clientSize = fWindow.GetWindow().GetClientAreaMetrics().logical;
        return ProcessImageLoadResult(fSession.fImageOpenController->LoadFileOrFolder(
            filePath, traverseMode, ImageLoadContext{static_cast<int>(clientSize.x), static_cast<int>(clientSize.y)}));
    }

}  // namespace OIV

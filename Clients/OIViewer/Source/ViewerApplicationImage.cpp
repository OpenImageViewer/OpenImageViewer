#include <LLUtils/Thread.h>
#include <array>
#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "ViewerApplication.h"

#include <Version.h>

#include <Functions.h>
#include <ApiGlobal.h>

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

#include "OIVCommands.h"

#include <ImageProcessing/TextRasterizer.h>
#include <ImageProcessing/RawImage.h>
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"

#include "ContextMenu.h"
#include "Globals.h"
#include <OIVAppCore/ConfigurationLoader.h>
#include "CommandRegistry.h"
#include <OIVAppCore/ColorCountPolicy.h>
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
#include <OIVShared/PixelHelper.h>
#include <ImageUtil/ImageUtil.h>
#include "InterThreadMessages.h"

namespace OIV
{
    void ViewerApplication::DrainUiCompletions()
    {
        std::vector<EventData> completions;
        {
            const std::scoped_lock lock(fUiCompletionMutex);
            completions.swap(fUiCompletions);
            fUiDrainScheduled = false;
        }
        for (const auto& completion : completions)
        {
            if (!fIsShuttingDown)
            {
                const LLUtils::OperationScope operation(completion.operationId);
                OnMessageFromBackgroundThread(completion);
            }
        }
    }

    void ViewerApplication::AddImageToControl(IMCodec::ImageSharedPtr image, uint16_t imageSlot, uint16_t totalImages)
    {
        auto bitmapImage = IMUtil::ImageUtil::ConvertImageWithNormalization(image, IMCodec::TexelFormat::I_B8_G8_R8_A8,
                                                                            false);
        if (bitmapImage == nullptr)
            return;

        const LWS::BitmapBuffer bitmapBuffer{
            .pixels   = {bitmapImage->GetBuffer(),
                         static_cast<size_t>(bitmapImage->GetRowPitchInBytes()) * bitmapImage->GetHeight()},
            .format   = LWS::BitmapPixelFormat::Bgra8,
            .rowOrder = LWS::BitmapRowOrder::TopDown,
            .width    = bitmapImage->GetWidth(),
            .height   = bitmapImage->GetHeight(),
            .rowPitch = bitmapImage->GetRowPitchInBytes(),
        };

        LLUtils::native_stringstream title;
        title << imageSlot + 1 << LLUTILS_TEXT('/') << totalImages << LLUTILS_TEXT("  ") << bitmapBuffer.width
              << LLUTILS_TEXT(" x ") << bitmapBuffer.height << LLUTILS_TEXT(" x 32 BPP");
        fWindow.GetImageControl().GetImageList().SetImage(
            {.index = imageSlot, .title = title.str(), .bitmap = std::make_shared<LWS::Bitmap>(bitmapBuffer)});
    }

    void ViewerApplication::UnloadOpenedImaged()
    {
        if (fBrowseSessionController != nullptr)
            fBrowseSessionController->InvalidateCurrent();
        fSequencerTimer.SetInterval(0);
        fSequencerPolicy.Reset();
        fImageState.ClearAll();
        UpdateMainRenderImages();
        fRefreshOperation.Queue();
        UpdateOpenImageUI();
    }

    void ViewerApplication::RefreshImage()
    {
        fRefreshOperation.Begin();
        fImageState.Refresh();
        UpdateMainRenderImages();
        fRefreshOperation.End(true);
    }

    void ViewerApplication::DisplayOpenedFileName()
    {
        if (IsOpenedImageIsAFile())
            SetUserMessage(ViewerPresentationPolicy::FormatOpenedFileMessage(
                               MessageFormatter::FormatFilePath(GetOpenedFileName())),
                           static_cast<GroupID>(UserMessageGroups::SuccessfulFileLoad),
                           MessageFlags::Interchangeable | MessageFlags::Moveable);
    }

    bool ViewerApplication::IsSubImagesVisible() const
    {
        using namespace IMCodec;
        if (fImageState.GetOpenedImage() != nullptr)
        {
            const auto mainImage = fImageState.GetOpenedImage()->pixels;
            return mainImage != nullptr &&
                   SubImagePolicy::IsVisible(mainImage->GetSubImageGroupType(), mainImage->GetNumSubImages());
        }
        else
        {
            return false;
        }
    }

    void ViewerApplication::LoadSubImages()
    {
        using namespace IMCodec;
        auto mainImage    = fImageState.GetOpenedImage();
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
            const int selectionIndex = fDisplayBiggestSubImageOnLoad
                                           ? SubImagePolicy::InitialSelectionIndex(isMainAnActualImage,
                                                                                   mainImage->pixels->GetTotalPixels(),
                                                                                   subImagePixels)
                                           : SubImagePolicy::MainImageIndex;
            fWindow.GetImageControl().GetImageList().SetSelected(selectionIndex);
            fWindow.GetImageControl().RefreshScrollInfo();
        }
        else
        {
            fWindow.GetImageControl().GetImageList().Clear();
        }
    }

    bool ViewerApplication::LoadFile(LLUtils::native_string_type filePath, IMCodec::PluginTraverseMode loaderFlags)
    {
        const auto clientSize = fWindow.GetWindow().GetClientAreaMetrics().logical;
        return ProcessImageLoadResult(fImageOpenController->LoadFile(
            filePath, loaderFlags, ImageLoadContext{static_cast<int>(clientSize.x), static_cast<int>(clientSize.y)}));
    }

    bool ViewerApplication::ProcessImageLoadResult(const ImageLoadResult& loadResult)
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
            if (fBrowseSessionController != nullptr && loadResult.image.pixels != nullptr &&
                loadResult.image.source == ImageSource::File)
            {
                const ResultCode commitResult = fBrowseSessionController->CommitCurrentFile(loadResult.normalizedPath);
                if (commitResult != ResultCode::RC_Success)
                {
                    const ImageLoadResult commitFailure{ImageLoadStatus::UnknownError,
                                                        commitResult,
                                                        loadResult.normalizedPath,
                                                        {}};
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

    LLUtils::native_string_type ViewerApplication::DescribeImage(const IMCodec::ImageSharedPtr& pixels)
    {
        LLUtils::native_stringstream stream;
        stream << pixels->GetWidth() << LLUTILS_TEXT(" X ") << pixels->GetHeight() << LLUTILS_TEXT(" X ")
               << pixels->GetBitsPerTexel() << LLUTILS_TEXT(" BPP | loaded in ") << std::fixed << std::setprecision(1)
               << pixels->GetImageItem()->processData.processTime << LLUTILS_TEXT(" ms");
        return stream.str();
    }

    void ViewerApplication::LoadOivImage(ViewerImage oivImage)
    {
        // Enter this function only from the main thread.
        assert("ViewerApplication::FinalizeImageLoad() can be called only from the main thread" && IsMainThread());

        if (oivImage.pixels == nullptr)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Expected a valid image");

        fFileDisplayTimer.Start();

        fSequencerTimer.SetInterval(0);
        fCurrentFrame = 0;
        fSequencerPolicy.Reset(oivImage.pixels);
        fQueueImageInfoLoad = GetImageInfoVisible();
        SetImageInfoVisible(false);
        SetResamplingEnabled(false);
        fImageState.SetOpenedImage(oivImage);

        fRefreshOperation.Begin();

        fImageState.ResetUserState();
        fImageState.Refresh(ImageProcessingStage::GeometricTransform);

        if (fResetTransformationMode == ResetTransformationMode::ResetAll)
            FitToClientAreaAndCenter();

        AutoPlaceImage();

        fImageState.Refresh();
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

        // Frame zero is already displayed by ImageState. Time its full duration and advance to
        // frame one on the first tick; even speed input before that tick now preserves progress.
        if (oivImage.pixels->GetSubImageGroupType() == IMCodec::ImageItemType::AnimationFrame &&
            oivImage.pixels->GetNumSubImages() != 0)
        {
            const auto& firstFrame = oivImage.pixels->GetSubImage(0);
            fCurrentFrame          = SequencerPolicy::NextFrame(0, oivImage.pixels->GetNumSubImages());
            fSequencerTimer.SetInterval(fSequencerPolicy.StartFrame(firstFrame->GetAnimationData().delayMilliseconds));
        }
    }

    void ViewerApplication::UpdateOpenImageUI()
    {
        if (IsImageOpen())
        {
            UpdateTitle();
            fVirtualStatusBar.SetText("imageDescription", DescribeImage(fImageState.GetRetainedImage(
                                                              ImageProcessingStage::GeometricTransform)));
        }
    }

    const LLUtils::native_string_type& ViewerApplication::GetOpenedFileName() const
    {
        static const LLUtils::native_string_type emptyString;
        const auto* file = fImageState.GetOpenedImage();
        return file && file->source == ImageSource::File ? file->path : emptyString;
    }

    bool ViewerApplication::IsImageOpen() const
    {
        return fImageState.GetOpenedImage() != nullptr;
    }

    bool ViewerApplication::IsOpenedImageIsAFile() const
    {
        return fImageState.GetOpenedImage() != nullptr && fImageState.GetOpenedImage()->source == ImageSource::File;
    }

    void ViewerApplication::SortFolderFileList()
    {
        if (fBrowseSessionController != nullptr)
        {
            fBrowseSessionController->SortFolderFileList();
            UpdateTitle();
        }
    }

    void ViewerApplication::ApplyBrowseSessionResult(const BrowseSessionController::BrowseSessionResult& result)
    {
        switch (result.action)
        {
            case BrowseSessionController::BrowseSessionAction::Ignore:
                break;
            case BrowseSessionController::BrowseSessionAction::DisplayImage:
                OnFileIndexResidencyReady(result.fileName, result.image);
                break;
            case BrowseSessionController::BrowseSessionAction::ShowFailure:
                ProcessImageLoadResult(
                    {ImageLoadStatus::UnknownError, ResultCode::RC_UknownError, result.fileName, {}});
                break;
            case BrowseSessionController::BrowseSessionAction::CurrentFileRemoved:
            case BrowseSessionController::BrowseSessionAction::CurrentFileUnsupportedRename:
                ProcessRemovalOfOpenedFile(result.fileName);
                break;
        }
    }

    void ViewerApplication::OnImageSelectionChanged(const ImageList::ImageSelectionChangeArgs& ImageSelectionChangeArgs)
    {
        auto imageIndex = ImageSelectionChangeArgs.imageIndex;
        if (imageIndex >= 0)
        {
            auto image = GetImageByIndex(imageIndex);

            fImageState.SetSelectedSource(image);
            fImageState.Refresh(ImageProcessingStage::GeometricTransform);
            fRefreshOperation.Begin();
            FitToClientAreaAndCenter();
            RefreshImage();

            if (GetImageInfoVisible() == true)
                ShowImageInfo();

            UpdateOpenImageUI();

            fRefreshOperation.End();
        }
    }

    void ViewerApplication::ProcessRemovalOfOpenedFile(const LLUtils::native_string_type& fileName)
    {
        const bool removeInternalDeletes = (fDeletedFileRemovalMode & DeletedFileRemovalMode::DeletedInternally) ==
                                           DeletedFileRemovalMode::DeletedInternally;
        const bool removeExternalDeletes = (fDeletedFileRemovalMode & DeletedFileRemovalMode::DeletedExternally) ==
                                           DeletedFileRemovalMode::DeletedExternally;
        const size_t fileCount           = fBrowseSessionController != nullptr
                                               ? fBrowseSessionController->GetFolderFileList().GetSize()
                                               : 0;

        const auto action = FileRemovalPolicy::Decide(GetOpenedFileName(), fileName, fRequestedFileForRemoval,
                                                      removeInternalDeletes, removeExternalDeletes, fileCount);
        if (action != RemovedFileAction::Ignore)
        {
            const bool replacementRequested = (action == RemovedFileAction::TryStart ||
                                               action == RemovedFileAction::TryNextThenPrevious) &&
                                              fBrowseSessionController != nullptr &&
                                              fBrowseSessionController->RequestRemovedFileReplacement(fileName);

            if (FileRemovalPolicy::ShouldUnloadAfterJumps(action, replacementRequested, false))
            {
                UnloadOpenedImaged();
                ShowWelcomeMessage();
            }

            fRequestedFileForRemoval = {};
        }

        UpdateTitle();
    }

    void ViewerApplication::OnFileChangedImpl(const IFileWatcher::FileChangedEventArgs* fileChangedEventArgsPtr)
    {
        auto fileChangedEventArgs  = *fileChangedEventArgsPtr;
        const bool hasActiveFolder = fBrowseSessionController != nullptr;
        const auto activeFolderID  = hasActiveFolder ? fBrowseSessionController->GetActiveFolderID()
                                                     : IFileWatcher::FolderID{};

        switch (FileChangePolicy::Decide(fileChangedEventArgs, hasActiveFolder, activeFolderID, fCOnfigurationFolderID,
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

    void ViewerApplication::OnFileChanged(IFileWatcher::FileChangedEventArgs fileChangedEventArgs)
    {
        if (fIsShuttingDown == false)
        {
            QueueUiCompletion(static_cast<std::underlying_type_t<InterThreadMessages>>(
                                  InterThreadMessages::FileChanged),
                              fileChangedEventArgs);
        }
    }

    void ViewerApplication::OnFileIndexResidencyReady(const LLUtils::native_string_type& fileName,
                                                      IMCodec::ImageSharedPtr image)
    {
        if (fBrowseSessionController == nullptr || !fBrowseSessionController->IsCurrentFile(fileName))
        {
            return;
        }

        if (image == nullptr)
        {
            ProcessImageLoadResult({ImageLoadStatus::UnknownError, ResultCode::RC_UknownError, fileName, {}});
            return;
        }

        ViewerImage file{.pixels = std::move(image),
                         .source = ImageSource::File,
                         .path   = std::filesystem::path(fileName).lexically_normal().native()};
        IMCodec::ItemMetaDataSharedPtr metaData;
        if (fImageLoader.LoadMetaData(fileName, metaData) == IMCodec::ImageResult::Success && metaData != nullptr)
        {
            file.metadata = std::move(metaData);
        }
        LoadOivImage(std::move(file));
    }

    void ViewerApplication::OnCountingColorsCompleted(const CountColorsData& result)
    {
        fIsColorThreadRunning = false;
        // Worker bookkeeping is completed even when the selected source changed.
        switch (ColorCountPolicy::DecideCompletion(result.generation, fImageState.GetSourceGeneration(),
                                                   GetImageInfoVisible()))
        {
            case ColorCountCompletionAction::ApplyToCurrentImage:
                fImageState.CompleteColorCount(ColorCountPolicy::NormalizeCountResult(
                    result.colorCount, UniqueColorsUninitialized - 1, UniqueColorsFailed));
                if (GetImageInfoVisible())
                    ShowImageInfo();
                break;
            case ColorCountCompletionAction::CountVisibleImage:
                CountColorsAsync();
                break;
            case ColorCountCompletionAction::Ignore:
                break;
        }
    }

    void ViewerApplication::OnMessageFromBackgroundThread(const EventData& sharedData)
    {
        if (sharedData.data.has_value() == false)
            LL_EXCEPTION_UNEXPECTED_VALUE;

        switch (static_cast<InterThreadMessages>(sharedData.id))
        {
            case InterThreadMessages::FileChanged:
            {
                const IFileWatcher::FileChangedEventArgs* fileChangedEventArgs =
                    std::any_cast<IFileWatcher::FileChangedEventArgs>(&(sharedData.data));

                if (fileChangedEventArgs == nullptr)
                    LL_EXCEPTION_UNEXPECTED_VALUE;

                OnFileChangedImpl(fileChangedEventArgs);
                BrowseSessionController::BrowseSessionResult result;
                if (fBrowseSessionController != nullptr)
                    result = fBrowseSessionController->OnFileChanged(*fileChangedEventArgs);
                UpdateTitle();
                ApplyBrowseSessionResult(result);

                break;
            }
            case InterThreadMessages::FileIndexResidencyReady:
            {
                const auto& fileIndexResidencyReadyData = std::any_cast<const FileIndexResidencyReadyData&>(
                    sharedData.data);
                OnFileIndexResidencyReady(fileIndexResidencyReadyData.fileName, fileIndexResidencyReadyData.image);
                break;
            }
            case InterThreadMessages::CandidateResidencyReady:
            {
                const auto& candidateResidencyReadyData = std::any_cast<const CandidateResidencyReadyData&>(
                    sharedData.data);
                const auto result =
                    fBrowseSessionController != nullptr
                        ? candidateResidencyReadyData.folderLoad
                              ? fBrowseSessionController->OnFolderOpenCandidateReady(candidateResidencyReadyData)
                              : fBrowseSessionController->OnBrowseCandidateReady(candidateResidencyReadyData)
                        : BrowseSessionController::BrowseSessionResult{};
                ApplyBrowseSessionResult(result);
                break;
            }
            case InterThreadMessages::AutoScroll:
                LL_EXCEPTION_NOT_IMPLEMENT("Auto scroll not implemented");
            case InterThreadMessages::CountColors:
            {
                const auto& colorsDAta = std::any_cast<const CountColorsData&>(sharedData.data);
                OnCountingColorsCompleted(colorsDAta);
                break;
            }

                LL_EXCEPTION_NOT_IMPLEMENT("Count colors not implemented");
            case InterThreadMessages::FirstFrameDisplayed:
                LL_EXCEPTION_NOT_IMPLEMENT("First frame displayed not implemented");
            case InterThreadMessages::LoadFileExternally:
                LL_EXCEPTION_NOT_IMPLEMENT("Load file externally not implemented");
            default:
                break;
        }
    }

    bool ViewerApplication::JumpFiles(FolderFileList::index_type step)
    {
        return fBrowseSessionController != nullptr && fBrowseSessionController->JumpFiles(step);
    }

    void ViewerApplication::OnImageReady(IMCodec::ImageSharedPtr image) {}

    void ViewerApplication::LoadRaw(const std::byte* buffer, uint32_t width, uint32_t height, uint32_t rowPitch,
                                    IMCodec::TexelFormat texelFormat)
    {
        const RawBufferParams params{buffer, width, height, rowPitch, texelFormat};
        auto pixels = CreateRawImage(params, {IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Vertical});
        LoadOivImage(ViewerImage{.pixels = std::move(pixels), .source = ImageSource::Clipboard});
    }

    void ViewerApplication::LoadClipboardText(const LLUtils::native_string_type& text)
    {
        auto textImage = std::make_unique<TextRasterizer>(fFreeType.get());
        textImage->SetText(text);
        textImage->SetDPI(static_cast<uint32_t>(std::lround(fDPIadjustmentFactor.x * 96.0)),
                          static_cast<uint32_t>(std::lround(fDPIadjustmentFactor.y * 96.0)));
        textImage->SetFontPath(LabelManager::sFontPath);
        textImage->SetFontSize(10);
        textImage->SetOutlineWidth(0);
        textImage->SetTextColor({48, 48, 48, 255});
        textImage->SetUseMetaText(false);
        textImage->SetBackgroundColor(LLUtils::Color(255, 255, 255, 255));
        if (auto pixels = textImage->GetBitmap())
            LoadOivImage(ViewerImage{.pixels = std::move(pixels), .source = ImageSource::ClipboardText});
    }

    OperationResult ViewerApplication::CopyVisibleToClipBoard()
    {
        OperationResult result = ImageEditPolicy::ValidateSelectionOperation(
            IsImageOpen(), fSelectionRect.GetSelectionRect().IsEmpty());
        if (result == OperationResult::Success)
        {
            result                               = OperationResult::UnkownError;
            LLUtils::RectI32 imageSpaceSelection = ClientToImageRounded(fSelectionRect.GetSelectionRect());
            auto cropped                         = IMUtil::ImageUtil::CropImage(
                fImageState.GetRetainedImage(ImageProcessingStage::RenderFormat), imageSpaceSelection);

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

    OperationResult ViewerApplication::CropVisibleImage()
    {
        OperationResult result = ImageEditPolicy::ValidateSelectionOperation(
            IsImageOpen(), fSelectionRect.GetSelectionRect().IsEmpty());
        if (result == OperationResult::Success)
        {
            result                        = OperationResult::UnkownError;
            LLUtils::RectI32 imageRectInt = ClientToImageRounded(fSelectionRect.GetSelectionRect());
            auto cropped                  = IMUtil::ImageUtil::CropImage(
                fImageState.GetRetainedImage(ImageProcessingStage::GeometricTransform), imageRectInt);

            if (cropped != nullptr)
            {
                auto oivCropped = ViewerImage{.pixels = cropped, .source = ImageSource::GeneratedByLib};
                LoadOivImage(oivCropped);
                CancelSelection();
                result = OperationResult::Success;
            }
        }
        return result;
    }

    OperationResult ViewerApplication::CutSelectedArea()
    {
        // Please note that currently this function works on the rasterized image, a more general solution is needed
        // to work on a previous stage image.
        OperationResult result = ImageEditPolicy::ValidateSelectionOperation(
            IsImageOpen(), fSelectionRect.GetSelectionRect().IsEmpty());
        if (result == OperationResult::Success)
        {
            result                        = OperationResult::UnkownError;
            auto rasterized               = fImageState.GetRetainedImage(ImageProcessingStage::RenderFormat);
            LLUtils::RectI32 subImageRect = ClientToImageRounded(fSelectionRect.GetSelectionRect());

            const LLUtils::RectI32 imageRect = {{0, 0},
                                                {static_cast<int32_t>(rasterized->GetWidth()),
                                                 static_cast<int32_t>(rasterized->GetHeight())}};

            subImageRect = subImageRect.Intersection(imageRect);

            if (subImageRect.IsEmpty() == false)
            {
                SetClipboardImage(IMUtil::ImageUtil::GetSubImage(rasterized, subImageRect));
                auto& texelInfo = IMCodec::GetTexelInfo(
                    fImageState.GetRetainedImage(ImageProcessingStage::RenderFormat)->GetOriginalTexelFormat());
                bool hasOpacityChannel = false;
                for (auto& channel : texelInfo.channles)
                    if (channel.semantic == IMCodec::ChannelSemantic::Opacity)
                    {
                        hasOpacityChannel = true;
                        break;
                    }

                const auto fillColor = hasOpacityChannel ? LLUtils::Color(0, 0, 0, 0) : LLUtils::Color(0, 0, 0, 255);
                auto colorFilled     = IMUtil::ImageUtil::FillColor(
                    fImageState.GetRetainedImage(ImageProcessingStage::RenderFormat), subImageRect, fillColor);

                if (colorFilled != nullptr)
                {
                    auto oivColorFilled = ViewerImage{.pixels = colorFilled, .source = ImageSource::GeneratedByLib};
                    auto lastState      = fResetTransformationMode;
                    fResetTransformationMode = ResetTransformationMode::DoNothing;
                    LoadOivImage(oivColorFilled);
                    fResetTransformationMode = lastState;
                    CancelSelection();
                    result = OperationResult::Success;
                }
            }
        }
        return result;
    }

    void ViewerApplication::AfterFirstFrameDisplayed()
    {
        PostInitOperations();
    }

    void ViewerApplication::PerformReloadFile(const LLUtils::native_string_type& requestedFile)
    {
        HandleReloadAction(fFileReloadPolicy.OnPendingReloadRequested(requestedFile), requestedFile);
    }

    void ViewerApplication::ProcessCurrentFileChanged()
    {
        HandleReloadAction(fFileReloadPolicy.OnCurrentFileChanged(GetOpenedFileName(), GetAppActive()),
                           GetOpenedFileName());
    }

    bool ViewerApplication::LoadFileOrFolder(const LLUtils::native_string_type& filePath,
                                             IMCodec::PluginTraverseMode traverseMode)
    {
        const auto clientSize = fWindow.GetWindow().GetClientAreaMetrics().logical;
        return ProcessImageLoadResult(fImageOpenController->LoadFileOrFolder(
            filePath, traverseMode, ImageLoadContext{static_cast<int>(clientSize.x), static_cast<int>(clientSize.y)}));
    }

    void ViewerApplication::CountColorsAsync()
    {
        if (fImageState.GetSelectedSource() && fImageState.GetColorCountState() == ColorCountState::NotRequested &&
            !fIsColorThreadRunning)
        {
            if (fCountingColorsThread.joinable())
                fCountingColorsThread.join();
            const auto generation = fImageState.GetSourceGeneration();
            auto pixels           = fImageState.GetSelectedSource();
            fCountingColorsThread = LLUtils::StartThread(
                [this](IMCodec::ImageSharedPtr image, uint64_t sourceGeneration)
                {
                    const auto count = PixelHelper::CountUniqueValues(image);
                    QueueUiCompletion(static_cast<std::underlying_type_t<InterThreadMessages>>(
                                          InterThreadMessages::CountColors),
                                      CountColorsData{sourceGeneration, count});
                },
                std::move(pixels), generation);
            fIsColorThreadRunning = true;
            fImageState.SetColorCountPending();
        }
    }
}  // namespace OIV

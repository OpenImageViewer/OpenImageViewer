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
#include <LLUtils/UniqueIDProvider.h>
#include <LLUtils/FileSystemHelper.h>
#include <LLUtils/Rect.h>

#include <OIVAppCore/OIVHelper.h>
#include "Helpers/ClipboardSetup.h"
#include <OIVAppCore/MessageFormatter.h>
#include <OIVAppCore/MessageHelper.h>
#include <OIVAppCore/ShellIntegrationHelper.h>
#include "Helpers/ShellCommandHandler.h"

#include "OIVCommands.h"

#include "OIVImage/OIVFileImage.h"
#include "OIVImage/OIVRawImage.h"
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
#include <OIVAppCore/OIVImageHelper.h>
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
        }
        for (const auto& completion : completions)
            OnMessageFromBackgroundThread(completion);
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
        fImageState.ClearAll();
        fRefreshOperation.Queue();
        UpdateOpenImageUI();
    }

    void ViewerApplication::RefreshImage()
    {
        fRefreshOperation.Begin();
        fImageState.Refresh();
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
            const auto mainImage = fImageState.GetOpenedImage()->GetImage();
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
        auto numSubImages = mainImage->GetImage()->GetNumSubImages();
        if (IsSubImagesVisible())
        {
            const auto isMainAnActualImage = SubImagePolicy::IncludeMainImage(mainImage->GetImage()->GetItemType());
            const uint16_t totalImages = SubImagePolicy::TotalDisplayedImages(mainImage->GetImage()->GetNumSubImages(),
                                                                              isMainAnActualImage);
            // Add the first image.
            uint16_t currentImage = 0;
            if (isMainAnActualImage)
                AddImageToControl(mainImage->GetImage(), static_cast<uint16_t>(currentImage++), totalImages);

            std::vector<uint64_t> subImagePixels;
            subImagePixels.reserve(numSubImages);
            for (uint16_t i = 0; i < numSubImages; i++)
            {
                auto currentSubImage = mainImage->GetImage()->GetSubImage(i);
                subImagePixels.push_back(currentSubImage->GetTotalPixels());
                AddImageToControl(currentSubImage, static_cast<uint16_t>(currentImage++), totalImages);
            }
            // Reset selected sub image when loading new set of subimages
            const int selectionIndex = fDisplayBiggestSubImageOnLoad
                                           ? SubImagePolicy::InitialSelectionIndex(
                                                 isMainAnActualImage, mainImage->GetImage()->GetTotalPixels(),
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
        const auto clientSize = fWindow.GetWindow().GetClientSize();
        return ProcessImageLoadResult(fImageOpenController->LoadFile(
            filePath, loaderFlags, ImageLoadContext{static_cast<int>(clientSize.x), static_cast<int>(clientSize.y)}));
    }

    bool ViewerApplication::ProcessImageLoadResult(const ImageLoadResult& loadResult)
    {
        if (loadResult.status == ImageLoadStatus::FolderLoadQueued)
            return true;

        if (loadResult.status == ImageLoadStatus::NoSupportedFiles)
            return false;

        auto formattedFilePath                   = MessageFormatter::FormatFilePath(loadResult.normalizedPath) +
                                                   LLUTILS_TEXT("<textcolor=#ff8930>");
        const ImageLoadPresentation presentation = ImageLoadPresentationPolicy::Decide(loadResult, formattedFilePath);

        if (presentation.shouldLoadImage)
        {
            if (fBrowseSessionController != nullptr && loadResult.image != nullptr &&
                loadResult.image->GetImageSource() == ImageSource::File)
            {
                const ResultCode commitResult = fBrowseSessionController->CommitCurrentFile(loadResult.normalizedPath);
                if (commitResult != ResultCode::RC_Success)
                {
                    const ImageLoadResult commitFailure{ImageLoadStatus::UnknownError, commitResult,
                                                        loadResult.normalizedPath, nullptr};
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

    void ViewerApplication::LoadOivImage(OIVBaseImageSharedPtr oivImage)
    {
        // Enter this function only from the main thread.
        assert("ViewerApplication::FinalizeImageLoad() can be called only from the main thread" && IsMainThread());

        if (oivImage->GetImage() == nullptr)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Expected a valid image");

        fFileDisplayTimer.Start();

        fCurrentFrame          = 0;
        fCurrentSequencerSpeed = 1.0;
        fQueueImageInfoLoad    = GetImageInfoVisible();
        SetImageInfoVisible(false);
        SetResamplingEnabled(false);
        fImageState.SetOpenedImage(oivImage);

        fRefreshOperation.Begin();

        fImageState.ResetUserState();

        if (fResetTransformationMode == ResetTransformationMode::ResetAll)
            FitToClientAreaAndCenter();

        AutoPlaceImage();

        fImageState.Refresh();
        fWindow.SetShowImageControl(IsSubImagesVisible());
        UnloadWelcomeMessage();
        DisplayOpenedFileName();

        if (fContextMenu != nullptr)
        {
            fContextMenu->EnableItem(LLUTILS_TEXT("Open containing folder"),
                                     oivImage->GetImageSource() == ImageSource::File);
            fContextMenu->EnableItem(LLUTILS_TEXT("Open in photoshop"),
                                     oivImage->GetImageSource() == ImageSource::File);
        }

        fRefreshOperation.End();
        fFileDisplayTimer.Stop();

        LoadSubImages();

        fImageState.GetOpenedImage()->SetDisplayTime(
            fFileDisplayTimer.GetElapsedTimeReal(LLUtils::StopWatch::TimeUnit::Milliseconds));

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

        // if sub images of main image are animation frame, start sequencer, otherwise make sure it's stopped
        fSequencerTimer.SetInterval(fImageState.GetOpenedImage()->GetImage()->GetSubImageGroupType() ==
                                            IMCodec::ImageItemType::AnimationFrame
                                        ? 1
                                        : 0);
    }

    void ViewerApplication::UpdateOpenImageUI()
    {
        if (IsImageOpen())
        {
            UpdateTitle();
            fVirtualStatusBar.SetText("imageDescription",
                                      fImageState.GetImage(ImageChainStage::Deformed)->GetDescription());
        }
    }

    const LLUtils::native_string_type& ViewerApplication::GetOpenedFileName() const
    {
        static const LLUtils::native_string_type emptyString;
        std::shared_ptr<OIVFileImage> file = std::dynamic_pointer_cast<OIVFileImage>(fImageState.GetOpenedImage());
        return file != nullptr ? file->GetFileName() : emptyString;
    }

    bool ViewerApplication::IsImageOpen() const
    {
        return fImageState.GetOpenedImage() != nullptr;
    }

    bool ViewerApplication::IsOpenedImageIsAFile() const
    {
        return fImageState.GetOpenedImage() != nullptr &&
               fImageState.GetOpenedImage()->GetImageSource() == ImageSource::File;
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
                    {ImageLoadStatus::UnknownError, ResultCode::RC_UknownError, result.fileName, nullptr});
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
            auto image = std::make_shared<OIVBaseImage>(ImageSource::GeneratedByLib, GetImageByIndex(imageIndex));

            fImageState.SetImageChainRoot(image);
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
            bool firstJumpSucceeded    = false;
            bool fallbackJumpSucceeded = false;

            if (action == RemovedFileAction::TryStart)
            {
                firstJumpSucceeded = JumpFiles(FolderFileList::IndexStart);
            }
            else if (action == RemovedFileAction::TryNextThenPrevious)
            {
                firstJumpSucceeded = JumpFiles(1);
                if (!firstJumpSucceeded)
                    fallbackJumpSucceeded = JumpFiles(-1);
            }

            if (FileRemovalPolicy::ShouldUnloadAfterJumps(action, firstJumpSucceeded, fallbackJumpSucceeded))
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
            ProcessImageLoadResult({ImageLoadStatus::UnknownError, ResultCode::RC_UknownError, fileName, nullptr});
            return;
        }

        std::shared_ptr<OIVFileImage> file = std::make_shared<OIVFileImage>(fileName, std::move(image));
        IMCodec::ItemMetaDataSharedPtr metaData;
        if (fImageLoader.LoadMetaData(fileName, metaData) == IMCodec::ImageResult::Success && metaData != nullptr)
        {
            file->SetMetaData(metaData);
        }
        LoadOivImage(file);
    }

    void ViewerApplication::OnCountingColorsCompleted(const CountColorsData& countColorsData)
    {
        fIsColorThreadRunning = false;

        switch (ColorCountPolicy::DecideCompletion(
            countColorsData.image, fImageState.GetImage(ImageChainStage::SourceImage).get(), GetImageInfoVisible()))
        {
            case ColorCountCompletionAction::ApplyToCurrentImage:
                // Still the same image on display, assing number of colors and refresh ImageInfo

                // if counting unique colors has failed, assign UniqueColorsFailed, so counting colors won't
                // restart for this image.
                fCountingImageColor.reset();
                fImageState.GetImage(ImageChainStage::SourceImage)
                    ->SetNumUniqueColors(ColorCountPolicy::NormalizeCountResult(
                        countColorsData.colorCount, UniqueColorsUninitialized - 1, UniqueColorsFailed));

                if (GetImageInfoVisible() == true)
                    ShowImageInfo();
                break;
            case ColorCountCompletionAction::CountVisibleImage:
                // If a different image on display Just count colors
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
        std::shared_ptr<OIVRawImage> rawImage = std::make_shared<OIVRawImage>(ImageSource::Clipboard);
        RawBufferParams params;
        params.width       = width;
        params.height      = height;
        params.rowPitch    = rowPitch;
        params.texelFormat = texelFormat;
        params.buffer      = buffer;
        // TODO: uncouple vertical flip from 'LoadRaw'
        ResultCode result = rawImage->Load(params,
                                           {IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Vertical});

        if (result == RC_Success)
            LoadOivImage(rawImage);
    }

    void ViewerApplication::LoadClipboardText(const LLUtils::native_string_type& text)
    {
        auto textImage = std::make_shared<OIVTextImage>(ImageSource::ClipboardText, fFreeType.get());
        textImage->SetText(text);
        textImage->SetPosition(LLUtils::PointF64::Zero);
        textImage->SetScale(LLUtils::PointF64::One);
        textImage->SetFilterType(OIV_Filter_type::FT_None);
        textImage->SetImageRenderMode(OIV_Image_Render_mode::IRM_MainImage);
        textImage->SetVisible(true);
        textImage->SetOpacity(1.0);
        textImage->SetDPI(static_cast<uint32_t>(std::lround(fDPIadjustmentFactor.x * 96.0)),
                          static_cast<uint32_t>(std::lround(fDPIadjustmentFactor.y * 96.0)));
        textImage->SetFontPath(LabelManager::sFontPath);
        textImage->SetFontSize(10);
        textImage->SetOutlineWidth(0);
        textImage->SetTextColor({48, 48, 48, 255});
        textImage->SetUseMetaText(false);
        textImage->SetBackgroundColor(LLUtils::Color(255, 255, 255, 255));
        textImage->Create();
        LoadOivImage(textImage);
    }

    OperationResult ViewerApplication::CopyVisibleToClipBoard()
    {
        OperationResult result = ImageEditPolicy::ValidateSelectionOperation(
            IsImageOpen(), fSelectionRect.GetSelectionRect().IsEmpty());
        if (result == OperationResult::Success)
        {
            result                               = OperationResult::UnkownError;
            LLUtils::RectI32 imageSpaceSelection = ClientToImageRounded(fSelectionRect.GetSelectionRect());
            auto cropped = IMUtil::ImageUtil::CropImage(fImageState.GetImage(ImageChainStage::Rasterized)->GetImage(),
                                                        imageSpaceSelection);

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
            auto cropped = IMUtil::ImageUtil::CropImage(fImageState.GetImage(ImageChainStage::Deformed)->GetImage(),
                                                        imageRectInt);

            if (cropped != nullptr)
            {
                auto oivCropped = std::make_shared<OIVBaseImage>(ImageSource::GeneratedByLib, cropped);
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
            auto rasterized               = fImageState.GetImage(ImageChainStage::Rasterized)->GetImage();
            LLUtils::RectI32 subImageRect = ClientToImageRounded(fSelectionRect.GetSelectionRect());

            const LLUtils::RectI32 imageRect = {{0, 0},
                                                {static_cast<int32_t>(rasterized->GetWidth()),
                                                 static_cast<int32_t>(rasterized->GetHeight())}};

            subImageRect = subImageRect.Intersection(imageRect);

            if (subImageRect.IsEmpty() == false)
            {
                SetClipboardImage(IMUtil::ImageUtil::GetSubImage(rasterized, subImageRect));
                auto& texelInfo = IMCodec::GetTexelInfo(
                    fImageState.GetImage(ImageChainStage::Rasterized)->GetImage()->GetOriginalTexelFormat());
                bool hasOpacityChannel = false;
                for (auto& channel : texelInfo.channles)
                    if (channel.semantic == IMCodec::ChannelSemantic::Opacity)
                    {
                        hasOpacityChannel = true;
                        break;
                    }

                const auto fillColor = hasOpacityChannel ? LLUtils::Color(0, 0, 0, 0) : LLUtils::Color(0, 0, 0, 255);
                auto colorFilled     = IMUtil::ImageUtil::FillColor(
                    fImageState.GetImage(ImageChainStage::Rasterized)->GetImage(), subImageRect, fillColor);

                if (colorFilled != nullptr)
                {
                    auto oivColorFilled      = std::make_shared<OIVBaseImage>(ImageSource::GeneratedByLib, colorFilled);
                    auto lastState           = fResetTransformationMode;
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
        const auto clientSize = fWindow.GetWindow().GetClientSize();
        return ProcessImageLoadResult(fImageOpenController->LoadFileOrFolder(
            filePath, traverseMode, ImageLoadContext{static_cast<int>(clientSize.x), static_cast<int>(clientSize.y)}));
    }

    void ViewerApplication::CountColorsAsync()
    {
        // Ensure shared tr refcount doesn't get to zero
        //  by assiging it to a private memeber field.

        auto openedImage = fImageState.GetImage(ImageChainStage::SourceImage);

        // Count colors ONLY if non initialized, meaning it's the first time of trying to count colors
        if (openedImage->GetNumUniqueColors() == UniqueColorsUninitialized)
        {
            if (fIsColorThreadRunning == false)
            {
                fIsColorThreadRunning = true;
                if (fCountingColorsThread.joinable())
                    fCountingColorsThread.join();

                // Keep the wrapper alive on the UI thread. The worker owns only pixel data
                // and returns the wrapper pointer as an identity token without dereferencing it.
                fCountingImageColor   = openedImage;
                fCountingColorsThread = std::thread(
                    [this](IMCodec::ImageSharedPtr image, OIVBaseImage* source) -> void
                    {
                        int64_t uniqueValues = PixelHelper::CountUniqueValues(image);
                        QueueUiCompletion(static_cast<std::underlying_type_t<InterThreadMessages>>(
                                              InterThreadMessages::CountColors),
                                          CountColorsData{source, uniqueValues});
                    },
                    fCountingImageColor->GetImage(), fCountingImageColor.get());
            }
        }
    }

}  // namespace OIV

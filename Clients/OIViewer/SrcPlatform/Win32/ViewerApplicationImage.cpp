#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "ViewerApplication.h"

#include <Windows.h>
#include <Version.h>

#include <Functions.h>
#include <ApiGlobal.h>
#include <Win32/Win32Window.h>
#include <Win32/Win32Helper.h>
#include <Win32/MonitorInfo.h>
#include <Win32/FileDialog.h>

#include <LInput/Keys/KeyCombination.h>
#include <LInput/Keys/KeyBindings.h>
#include <LInput/Buttons/Extensions/ButtonsStdExtension.h>
#include <LInput/Mouse/MouseButton.h>

#include <LLUtils/Exception.h>
#include <LLUtils/FileHelper.h>
#include <LLUtils/PlatformUtility.h>
#include <LLUtils/StringUtility.h>
#include <LLUtils/UniqueIDProvider.h>
#include <LLUtils/Logging/LogPredefined.h>
#include <LLUtils/Logging/Logger.h>
#include <LLUtils/FileSystemHelper.h>
#include <LLUtils/Rect.h>

#include "Helpers/OIVHelper.h"
#include "Helpers/ClipboardSetup.h"
#include "Helpers/MessageHelper.h"
#include "Helpers/ShellIntegrationHelper.h"
#include "Helpers/ShellCommandHandler.h"

#include "Win32/UserMessages.h"
#include "OIVCommands.h"

#include "OIVImage/OIVFileImage.h"
#include "OIVImage/OIVRawImage.h"
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"

#include "ContextMenu.h"
#include "Globals.h"
#include "ConfigurationLoader.h"
#include "CommandRegistry.h"
#include "ExceptionHandler.h"
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

#include "Resource.h"

namespace OIV
{
    void ViewerApplication::UnloadOpenedImaged()
    {
        if (fBrowseSessionController != nullptr)
            fBrowseSessionController->InvalidateCurrent();
        fImageState.ClearAll();
        fRefreshOperation.Queue();
        UpdateOpenImageUI();
    }

    void ViewerApplication::DeleteOpenedFile(bool permanently)
    {
        size_t stringLength = GetOpenedFileName().length();
        auto buffer         = std::make_unique<wchar_t[]>(stringLength + 2);

        memcpy(buffer.get(), GetOpenedFileName().c_str(), (stringLength + 1) * sizeof(wchar_t));

        buffer.get()[stringLength + 1] = '\0';

        SHFILEOPSTRUCT file_op = {GetWindowHandle(),
                                  FO_DELETE,
                                  buffer.get(),
                                  nullptr,
                                  static_cast<FILEOP_FLAGS>(permanently ? 0 : FOF_ALLOWUNDO),
                                  FALSE,
                                  nullptr,
                                  nullptr};

        auto fileNameToRemove    = GetOpenedFileName();
        fRequestedFileForRemoval = fileNameToRemove;

        int shResult = SHFileOperation(&file_op);

        if (shResult != 0)
        {
            // handle error
        }
        else
        {
            ProcessRemovalOfOpenedFile(fileNameToRemove);
        }
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

    void ViewerApplication::AddImageToControl(IMCodec::ImageSharedPtr image, uint16_t imageSlot, uint16_t totalImages)
    {
        // add an image to a windows control, so create a system compatiable image - flipped BGRA  in windows.

        // Convert to BGRA bitmap.
        auto bgraImage = IMUtil::ImageUtil::ConvertImageWithNormalization(image, IMCodec::TexelFormat::I_B8_G8_R8_A8,
                                                                          false);

        // Flip vertically.
        bgraImage = IMUtil::ImageUtil::Transform({IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Vertical},
                                                 bgraImage);

        // Create 32 bit BGRA color image

        ::Win32::BitmapBuffer bitmapBuffer{};
        bitmapBuffer.bitsPerPixel = bgraImage->GetBitsPerTexel();
        bitmapBuffer.rowPitch     = LLUtils::Utility::Align<uint32_t>(bgraImage->GetRowPitchInBytes(), sizeof(DWORD));
        LLUtils::Buffer colorBuffer(bgraImage->GetHeight() * bitmapBuffer.rowPitch);
        bitmapBuffer.buffer = colorBuffer.data();
        bitmapBuffer.height = bgraImage->GetHeight();
        bitmapBuffer.width  = bgraImage->GetWidth();

        // Create 24 bit mask image.
        ::Win32::BitmapBuffer maskBuffer{};
        maskBuffer.bitsPerPixel = 24;
        maskBuffer.height       = bgraImage->GetHeight();
        maskBuffer.width        = bgraImage->GetWidth();
        maskBuffer.rowPitch = LLUtils::Utility::Align<uint32_t>(maskBuffer.width * maskBuffer.bitsPerPixel / CHAR_BIT,
                                                                sizeof(DWORD));
        LLUtils::Buffer maskPixelsBuffer(maskBuffer.height * maskBuffer.rowPitch);
        maskBuffer.buffer = maskPixelsBuffer.data();

#pragma pack(push, 1)

        struct Color32
        {
            uint8_t R;
            uint8_t G;
            uint8_t B;
            uint8_t A;
        };

        struct Color24
        {
            uint8_t R;
            uint8_t G;
            uint8_t B;
        };
#pragma pack(pop)
        for (uint32_t l = 0; l < maskBuffer.height; l++)
        {
            const uint32_t sourceOffset = l * bgraImage->GetRowPitchInBytes();
            const uint32_t colorOffset  = l * bitmapBuffer.rowPitch;
            const uint32_t maskOffset   = l * maskBuffer.rowPitch;
            // Create mask/color pairs for GDI painting.
            for (size_t x = 0; x < maskBuffer.width; x++)
            {
                Color24& destMask  = reinterpret_cast<Color24*>(reinterpret_cast<uint8_t*>(maskPixelsBuffer.data()) +
                                                                maskOffset)[x];
                Color32& destImage = reinterpret_cast<Color32*>(reinterpret_cast<uint8_t*>(colorBuffer.data()) +
                                                                colorOffset)[x];
                const Color32& sourceColor = reinterpret_cast<const Color32*>(
                    reinterpret_cast<const uint8_t*>(bgraImage->GetBuffer()) + sourceOffset)[x];

                const uint8_t AlphaChannel = sourceColor.A;
                const uint8_t invAlpha     = 0xFF - AlphaChannel;
                // Mask is painted using ot OR operation.
                // White is fully opaque, black is fully transparent
                destMask.R = AlphaChannel;
                destMask.G = AlphaChannel;
                destMask.B = AlphaChannel;

                // Color image is painted using AND operation
                // Blend inverse alpha to adjust pixel color accoring to alpha.
                destImage.R = sourceColor.R | invAlpha;
                destImage.G = sourceColor.G | invAlpha;
                destImage.B = sourceColor.B | invAlpha;
            }
        }

        std::wstringstream ss;
        ss << imageSlot + 1 << L'/' << totalImages << L"  " << bitmapBuffer.width << L" x " << bitmapBuffer.height
           << L" x " << bitmapBuffer.bitsPerPixel << L" BPP";

        fWindow.GetImageControl().GetImageList().SetImage(
            {imageSlot, ss.str(), std::make_shared<::Win32::BitmapSharedPtr::element_type>(bitmapBuffer),
             std::make_shared<::Win32::BitmapSharedPtr::element_type>(maskBuffer)});
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

    bool ViewerApplication::LoadFile(std::wstring filePath, IMCodec::PluginTraverseMode loaderFlags)
    {
        const auto clientSize = fWindow.GetClientSize();
        return ProcessImageLoadResult(fImageOpenController->LoadFile(
            filePath, loaderFlags, ImageLoadContext{static_cast<int>(clientSize.cx), static_cast<int>(clientSize.cy)}));
    }

    bool ViewerApplication::ProcessImageLoadResult(const ImageLoadResult& loadResult)
    {
        if (loadResult.status == ImageLoadStatus::FolderLoadQueued)
            return true;

        if (loadResult.status == ImageLoadStatus::NoSupportedFiles)
            return false;

        auto formattedFilePath = MessageFormatter::FormatFilePath(loadResult.normalizedPath) + L"<textcolor=#ff8930>";
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
            fWindow.SetVisible(true);
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

    const std::wstring& ViewerApplication::GetOpenedFileName() const
    {
        static const std::wstring emptyString;
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

    void ViewerApplication::ProcessRemovalOfOpenedFile(const std::wstring& fileName)
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
                                         std::filesystem::path(GetOpenedFileName()).wstring()))
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
            fEventSync.AddData(static_cast<std::underlying_type_t<InterThreadMessages>>(
                                   InterThreadMessages::FileChanged),
                               fileChangedEventArgs);
        }
    }

    void ViewerApplication::OnFileIndexResidencyReady(const std::wstring& fileName, IMCodec::ImageSharedPtr image)
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

    ClipboardDataType ViewerApplication::PasteFromClipBoard()
    {
        ClipboardDataType clipboardType  = ClipboardDataType::None;
        const auto& [formatType, buffer] = fClipboardHelper.GetClipboardData();

        if (formatType == CF_DIB || formatType == CF_DIBV5)
        {
            const tagBITMAPINFO* bitmapInfo = reinterpret_cast<const tagBITMAPINFO*>(buffer.data());
            const BITMAPINFOHEADER* info    = &(bitmapInfo->bmiHeader);
            uint32_t rowPitch = LLUtils::Utility::Align<uint32_t>(info->biWidth * (info->biBitCount / CHAR_BIT), 4);

            const std::byte* bitmapBitsconst = reinterpret_cast<const std::byte*>(info) + info->biSize;
            std::byte* bitmapBits            = const_cast<std::byte*>(bitmapBitsconst);

            switch (info->biCompression)
            {
                case BI_RGB:
                    break;
                case BI_BITFIELDS:
                    bitmapBits += 3 * sizeof(DWORD);
                    break;
                default:
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented,
                                 std::string("Unsupported clipboard bitmap compression type :") +
                                     std::to_string(info->biCompression));
            }

            using namespace IMCodec;
            ImageItemSharedPtr imageItem = std::make_shared<ImageItem>();
            ImageDescriptor& props       = imageItem->descriptor;

            imageItem->itemType           = ImageItemType::Image;
            props.height                  = info->biHeight;
            props.width                   = info->biWidth;
            props.texelFormatStorage      = info->biBitCount == 24 ? IMCodec::TexelFormat::I_B8_G8_R8
                                                                   : IMCodec::TexelFormat::I_B8_G8_R8_A8;
            props.texelFormatDecompressed = info->biBitCount == 24 ? IMCodec::TexelFormat::I_B8_G8_R8
                                                                   : IMCodec::TexelFormat::I_B8_G8_R8_A8;
            props.rowPitchInBytes         = rowPitch;
            const size_t bufferSize       = props.rowPitchInBytes * props.height;
            imageItem->data.Allocate(bufferSize);
            imageItem->data.Write(bitmapBits, 0, bufferSize);
            auto image = std::make_shared<Image>(imageItem, ImageItemType::Unknown);

            if (info->biCompression == BI_BITFIELDS)  // no support for alpha channel, convert to BGR
                image = IMUtil::ImageUtil::Convert(image, IMCodec::TexelFormat::I_B8_G8_R8);

            image = IMUtil::ImageUtil::Transform({IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Vertical},
                                                 image);

            std::shared_ptr<OIVBaseImage> rawImage = std::make_shared<OIVBaseImage>(ImageSource::Clipboard, image);

            LoadOivImage(rawImage);
            clipboardType = ClipboardDataType::Image;
        }

        else if (formatType == CF_UNICODETEXT || formatType == CF_TEXT)
        {
            std::wstring text;
            /*if (isHTMLFormat)
                text = LLUtils::StringUtility::ToWString((char*)clipboardBuffer);
            if (isRTFText)
                text = LLUtils::StringUtility::ToWString((char*)clipboardBuffer);*/
            if (formatType == CF_UNICODETEXT)
                text = (wchar_t*) buffer.data();
            else if (formatType == CF_TEXT)
                text = LLUtils::StringUtility::ToWString((const char*) buffer.data());

            if (text.empty() == false)
            {
                OIVTextImageSharedPtr textImage = std::make_shared<OIVTextImage>(ImageSource::ClipboardText,
                                                                                 fFreeType.get());
                textImage->SetText(text);
                textImage->SetPosition(LLUtils::PointF64::Zero);
                textImage->SetScale(LLUtils::PointF64::One);
                textImage->SetFilterType(OIV_Filter_type::FT_None);
                textImage->SetImageRenderMode(OIV_Image_Render_mode::IRM_MainImage);
                textImage->SetVisible(true);
                textImage->SetOpacity(1.0);

                textImage->SetDPI(fCurrentMonitorProperties.DPIx, fCurrentMonitorProperties.DPIy);
                textImage->SetDPI(fCurrentMonitorProperties.DPIx, fCurrentMonitorProperties.DPIy);
                textImage->SetFontPath(LabelManager::sFontPath);
                textImage->SetFontSize(10);
                textImage->SetOutlineWidth(0);
                textImage->SetTextColor({48, 48, 48, 255});
                textImage->SetUseMetaText(false);
                // text->SetRenderMode(OIV_PROP_CreateText_Mode::CTM_AntiAliased);
                textImage->SetBackgroundColor(LLUtils::Color(255, 255, 255, 255));
                // textImage->Create();
                textImage->Create();
                // textImage->GetImage();
                LoadOivImage(textImage);
                clipboardType = ClipboardDataType::Text;
            }
        }
        return clipboardType;
    }

    bool ViewerApplication::SetClipboardImage(IMCodec::ImageSharedPtr image)
    {
        auto clipboardCompatibleImage = IMUtil::ImageUtil::ConvertImageWithNormalization(
            image, IMCodec::TexelFormat::I_B8_G8_R8_A8, false);
        if (clipboardCompatibleImage != nullptr)
        {
            uint32_t width  = clipboardCompatibleImage->GetWidth();
            uint32_t height = clipboardCompatibleImage->GetHeight();
            uint8_t bpp     = clipboardCompatibleImage->GetBitsPerTexel();
            auto dibBUffer  = LLUtils::PlatformUtility::CreateDIB<1>(width, height, bpp,
                                                                     clipboardCompatibleImage->GetRowPitchInBytes(),
                                                                     clipboardCompatibleImage->GetBuffer());
            auto result     = fClipboardHelper.SetClipboardData(CF_DIB, dibBUffer);

            if (result == ::Win32::ClipboardResult::Success)
            {
                auto dibV5BUffer = LLUtils::PlatformUtility::CreateDIB<5>(
                    width, height, bpp, clipboardCompatibleImage->GetRowPitchInBytes(),
                    clipboardCompatibleImage->GetBuffer());
                result = fClipboardHelper.SetClipboardData(CF_DIBV5, dibV5BUffer);
            }

            if (result == ::Win32::ClipboardResult::Success)
                return true;
        }
        return false;
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

    void ViewerApplication::PerformReloadFile(const std::wstring& requestedFile)
    {
        HandleReloadAction(fFileReloadPolicy.OnPendingReloadRequested(requestedFile), requestedFile);
    }

    void ViewerApplication::HandleReloadAction(ReloadAction action, const std::wstring& requestedFile)
    {
        if (action == ReloadAction::AskUser)
        {
            using namespace std::string_literals;
            int mbResult = MessageBox(fWindow.GetHandle(), (L"Reload the file: "s + requestedFile).c_str(),
                                      L"File is changed outside of OIV", MB_YESNO);
            action       = fFileReloadPolicy.ConfirmReload(mbResult == IDYES);
        }

        if (action == ReloadAction::RequestNow && fBrowseSessionController != nullptr)
            fBrowseSessionController->RequestCurrentFileReload();
    }

    void ViewerApplication::ProcessCurrentFileChanged()
    {
        HandleReloadAction(fFileReloadPolicy.OnCurrentFileChanged(GetOpenedFileName(), GetAppActive()),
                           GetOpenedFileName());
    }

    bool ViewerApplication::LoadFileOrFolder(const std::wstring& filePath, IMCodec::PluginTraverseMode traverseMode)
    {
        const auto clientSize = fWindow.GetClientSize();
        return ProcessImageLoadResult(fImageOpenController->LoadFileOrFolder(
            filePath, traverseMode,
            ImageLoadContext{static_cast<int>(clientSize.cx), static_cast<int>(clientSize.cy)}));
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

                fCountingImageColor   = openedImage;
                fCountingColorsThread = std::thread(
                    [&](OIVBaseImageSharedPtr image) -> void
                    {
                        int64_t uniqueValues = PixelHelper::CountUniqueValues(image->GetImage());
                        fEventSync.AddData(static_cast<std::underlying_type_t<InterThreadMessages>>(
                                               InterThreadMessages::CountColors),
                                           CountColorsData{image.get(), uniqueValues});
                    },
                    fCountingImageColor);
            }
        }
    }

}  // namespace OIV

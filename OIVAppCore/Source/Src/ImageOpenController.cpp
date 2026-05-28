#include <OIVAppCore/ImageOpenController.h>

#include <LLUtils/StringDefs.h>

#include <filesystem>
#include <utility>

namespace OIV
{
    bool ImageLoadResult::DecodeSucceeded() const
    {
        return resultCode == ResultCode::RC_Success;
    }

    OIVImageFileLoader::OIVImageFileLoader(IMCodec::ImageLoader& imageLoader) : fImageLoader(imageLoader) {}

    ImageFileLoadResult OIVImageFileLoader::LoadFile(const LLUtils::native_string_type& normalizedFilePath,
                                                     IMCodec::PluginTraverseMode traverseMode,
                                                     const ImageLoadContext& context)
    {
        auto file = std::make_shared<OIVFileImage>(normalizedFilePath);

        IMCodec::Parameters params;
        params.SetCustom(LLUTILS_TEXT("canvasWidth"), context.canvasWidth);
        params.SetCustom(LLUTILS_TEXT("canvasHeight"), context.canvasHeight);

        const ResultCode result = file->Load(&fImageLoader, traverseMode, IMCodec::ImageLoadFlags::None, params);
        return ImageFileLoadResult{result, file};
    }

    ImageOpenController::ImageOpenController(std::unique_ptr<IImageFileLoader> imageFileLoader,
                                             BrowseSessionController* fileSessionController)
        : fBrowseSessionController(fileSessionController), fImageFileLoader(std::move(imageFileLoader))
    {
    }

    void ImageOpenController::SetBrowseSessionController(BrowseSessionController* fileSessionController)
    {
        fBrowseSessionController = fileSessionController;
    }

    ImageLoadResult ImageOpenController::LoadFile(const LLUtils::native_string_type& filePath,
                                                  IMCodec::PluginTraverseMode traverseMode,
                                                  const ImageLoadContext& context)
    {
        const LLUtils::native_string_type normalizedPath = std::filesystem::path(filePath).lexically_normal().native();
        if (fBrowseSessionController != nullptr)
            fBrowseSessionController->BeginDirectOpen(normalizedPath);

        auto fileLoadResult = fImageFileLoader->LoadFile(normalizedPath, traverseMode, context);
        const auto image    = fileLoadResult.image != nullptr ? fileLoadResult.image->GetImage() : nullptr;

        return ImageLoadResult{ClassifyLoadResult(fileLoadResult.resultCode, image), fileLoadResult.resultCode,
                               normalizedPath, std::move(fileLoadResult.image)};
    }

    ImageLoadResult ImageOpenController::LoadFileOrFolder(const LLUtils::native_string_type& filePath,
                                                          IMCodec::PluginTraverseMode traverseMode,
                                                          const ImageLoadContext& context)
    {
        if (std::filesystem::is_directory(filePath))
        {
            const bool folderLoadQueued = fBrowseSessionController != nullptr &&
                                          fBrowseSessionController->RequestFolderLoadResidency(filePath);

            return ImageLoadResult{folderLoadQueued ? ImageLoadStatus::FolderLoadQueued
                                                    : ImageLoadStatus::NoSupportedFiles,
                                   folderLoadQueued ? ResultCode::RC_Success : ResultCode::RC_EmptyData,
                                   std::filesystem::path(filePath).lexically_normal().native(), nullptr};
        }

        return LoadFile(filePath, traverseMode, context);
    }

    ImageLoadStatus ImageOpenController::ClassifyLoadResult(ResultCode resultCode, const IMCodec::ImageSharedPtr& image,
                                                            std::uint32_t maxSupportedDimension)
    {
        if (resultCode == ResultCode::RC_FileNotSupported)
            return ImageLoadStatus::UnsupportedFormat;

        if (resultCode != ResultCode::RC_Success)
            return ImageLoadStatus::UnknownError;

        if (image == nullptr)
            return ImageLoadStatus::UnknownError;

        if (image->GetWidth() > maxSupportedDimension || image->GetHeight() > maxSupportedDimension)
            return ImageLoadStatus::TooLarge;

        return ImageLoadStatus::Loaded;
    }
}  // namespace OIV

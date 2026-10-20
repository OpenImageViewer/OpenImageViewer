#include <OIVAppCore/ImageOpenController.h>

#include <LLUtils/StringDefs.h>
#include <LLUtils/Logging/Logger.h>

#include <filesystem>
#include <utility>

namespace
{
    LLUtils::LogCategory ImageOpenLog()
    {
        static const auto category = LLUtils::Logger::RegisterCategory("ImageOpen");
        return category;
    }
}  // namespace

namespace OIV
{
    bool ImageLoadResult::DecodeSucceeded() const
    {
        return status == ImageLoadStatus::Loaded;
    }

    OIVImageFileLoader::OIVImageFileLoader(IMCodec::ImageLoader& imageLoader) : fImageLoader(imageLoader) {}

    ImageFileLoadResult OIVImageFileLoader::LoadFile(const LLUtils::native_string_type& normalizedFilePath,
                                                     IMCodec::PluginTraverseMode traverseMode,
                                                     const ImageLoadContext& context)
    {
        IMCodec::Parameters params;
        params.SetCustom(LLUTILS_TEXT("canvasWidth"), context.canvasWidth);
        params.SetCustom(LLUTILS_TEXT("canvasHeight"), context.canvasHeight);

        IMCodec::ItemMetaDataSharedPtr metadata;
        auto pixels                = DecodeFileImage(fImageLoader, normalizedFilePath, metadata, traverseMode,
                                                     IMCodec::ImageLoadFlags::None, params);
        ImageFileLoadResult result = std::unexpected(ImageLoadError::UnsupportedFormat);
        if (pixels)
            result = ViewerImage{std::move(pixels), std::move(metadata), ImageSource::File, normalizedFilePath};
        return result;
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
        auto id = context.operationId.value ? context.operationId : LLUtils::OperationScope::Current();
        if (!id.value)
            id = LLUtils::OperationId::Create();
        const LLUtils::OperationScope operation(id);
        LL_LOG(ImageOpenLog(), LLUtils::LogLevel::Info, "Opening {}", LLUtils::StringUtility::ToAString(filePath));
        const LLUtils::native_string_type normalizedPath = std::filesystem::path(filePath).lexically_normal().native();
        if (fBrowseSessionController != nullptr)
            fBrowseSessionController->BeginDirectOpen(normalizedPath);

        auto fileLoadResult = fImageFileLoader->LoadFile(normalizedPath, traverseMode, context);

        return ImageLoadResult{ClassifyLoadResult(fileLoadResult), normalizedPath,
                               fileLoadResult ? std::move(*fileLoadResult) : ViewerImage{}, id};
    }

    ImageLoadResult ImageOpenController::LoadFileOrFolder(const LLUtils::native_string_type& filePath,
                                                          IMCodec::PluginTraverseMode traverseMode,
                                                          const ImageLoadContext& context)
    {
        auto id = context.operationId.value ? context.operationId : LLUtils::OperationScope::Current();
        if (!id.value)
            id = LLUtils::OperationId::Create();
        const LLUtils::OperationScope operation(id);
        if (std::filesystem::is_directory(filePath))
        {
            const bool folderLoadQueued = fBrowseSessionController != nullptr &&
                                          fBrowseSessionController->RequestFolderLoadResidency(filePath);

            return ImageLoadResult{folderLoadQueued ? ImageLoadStatus::FolderLoadQueued
                                                    : ImageLoadStatus::NoSupportedFiles,
                                   std::filesystem::path(filePath).lexically_normal().native(),
                                   {},
                                   id};
        }

        return LoadFile(filePath, traverseMode, context);
    }

    ImageLoadStatus ImageOpenController::ClassifyLoadResult(const ImageFileLoadResult& result)
    {
        ImageLoadStatus status = ImageLoadStatus::UnknownError;
        if (!result)
            status = result.error() == ImageLoadError::UnsupportedFormat ? ImageLoadStatus::UnsupportedFormat
                                                                         : ImageLoadStatus::UnknownError;
        else if (result->pixels)
            status = ImageLoadStatus::Loaded;
        return status;
    }
}  // namespace OIV

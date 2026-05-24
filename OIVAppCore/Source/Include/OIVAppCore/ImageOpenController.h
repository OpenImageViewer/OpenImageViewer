#pragma once

#include <OIVAppCore/BrowseSessionController.h>

#include <OIVImage/OIVFileImage.h>
#include <ImageLoader.h>
#include <Defs.h>

#include <cstdint>
#include <memory>
#include <string>

namespace OIV
{
    enum class ImageLoadStatus
    {
        Loaded,
        UnsupportedFormat,
        TooLarge,
        UnknownError,
        FolderLoadQueued,
        NoSupportedFiles
    };

    struct ImageLoadContext
    {
        int canvasWidth  = 0;
        int canvasHeight = 0;
    };

    struct ImageFileLoadResult
    {
        ResultCode resultCode = ResultCode::RC_UknownError;
        std::shared_ptr<OIVFileImage> image;
    };

    struct ImageLoadResult
    {
        ImageLoadStatus status = ImageLoadStatus::UnknownError;
        ResultCode resultCode  = ResultCode::RC_UknownError;
        std::wstring normalizedPath;
        std::shared_ptr<OIVFileImage> image;

        bool DecodeSucceeded() const;
    };

    class IImageFileLoader
    {
      public:

        virtual ~IImageFileLoader()                                           = default;
        virtual ImageFileLoadResult LoadFile(const std::wstring& normalizedFilePath,
                                             IMCodec::PluginTraverseMode traverseMode,
                                             const ImageLoadContext& context) = 0;
    };

    class OIVImageFileLoader : public IImageFileLoader
    {
      public:

        explicit OIVImageFileLoader(IMCodec::ImageLoader& imageLoader);
        ImageFileLoadResult LoadFile(const std::wstring& normalizedFilePath, IMCodec::PluginTraverseMode traverseMode,
                                     const ImageLoadContext& context) override;

      private:

        IMCodec::ImageLoader& fImageLoader;
    };

    class ImageOpenController
    {
      public:

        static constexpr std::uint32_t MaxSupportedDimension = 16384;

        explicit ImageOpenController(std::unique_ptr<IImageFileLoader> imageFileLoader,
                                     BrowseSessionController* fileSessionController = nullptr);

        void SetBrowseSessionController(BrowseSessionController* fileSessionController);
        ImageLoadResult LoadFile(const std::wstring& filePath, IMCodec::PluginTraverseMode traverseMode,
                                 const ImageLoadContext& context);
        ImageLoadResult LoadFileOrFolder(const std::wstring& filePath, IMCodec::PluginTraverseMode traverseMode,
                                         const ImageLoadContext& context);

        static ImageLoadStatus ClassifyLoadResult(ResultCode resultCode, const IMCodec::ImageSharedPtr& image,
                                                  std::uint32_t maxSupportedDimension = MaxSupportedDimension);

      private:

        BrowseSessionController* fBrowseSessionController = nullptr;
        std::unique_ptr<IImageFileLoader> fImageFileLoader;
    };
}  // namespace OIV

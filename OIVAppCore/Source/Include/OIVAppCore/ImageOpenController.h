#pragma once

#include <LLUtils/StringDefs.h>
#include <LLUtils/Logging/OperationContext.h>

#include <OIVAppCore/BrowseSessionController.h>

#include <OIVAppCore/ViewerImage.h>
#include <ImageProcessing/FileImage.h>
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

    // CPU decode context shared with the existing codec parameter boundary.
    struct ImageLoadContext
    {
        // Requested canvas width in pixels, passed to existing codec parameters.
        int canvasWidth = 0;
        // Requested canvas height in pixels, passed to existing codec parameters.
        int canvasHeight = 0;
        // Logging operation identity carried through decode and completion.
        LLUtils::OperationId operationId;
    };

    // CPU file-load result with the existing API failure code.
    struct ImageFileLoadResult
    {
        // Existing API result code describing decode or opening failure.
        ResultCode resultCode = ResultCode::RC_UknownError;
        // CPU document returned by loading, with empty pixels when decoding fails.
        ViewerImage image;
    };

    // Opening result carried to viewer composition without registering pixels.
    struct ImageLoadResult
    {
        // Viewer classification of the opening result.
        ImageLoadStatus status = ImageLoadStatus::UnknownError;
        // Existing API result code describing decode or opening failure.
        ResultCode resultCode = ResultCode::RC_UknownError;
        // Normalized path for file/folder navigation and error formatting.
        LLUtils::native_string_type normalizedPath;
        // CPU document returned by loading, with empty pixels when decoding fails.
        ViewerImage image;

        // Logging operation identity carried through decode and completion.
        LLUtils::OperationId operationId;

        bool DecodeSucceeded() const;
    };

    // CPU file-loading boundary that returns document values and API result codes.
    class IImageFileLoader
    {
      public:

        virtual ~IImageFileLoader()                                           = default;
        virtual ImageFileLoadResult LoadFile(const LLUtils::native_string_type& normalizedFilePath,
                                             IMCodec::PluginTraverseMode traverseMode,
                                             const ImageLoadContext& context) = 0;
    };

    // Detached decoder adapter preserving orientation, metadata and codec parameters.
    class OIVImageFileLoader : public IImageFileLoader
    {
      public:

        explicit OIVImageFileLoader(IMCodec::ImageLoader& imageLoader);
        ImageFileLoadResult LoadFile(const LLUtils::native_string_type& normalizedFilePath,
                                     IMCodec::PluginTraverseMode traverseMode,
                                     const ImageLoadContext& context) override;

      private:

        // Borrowed codec loader that outlives the file loader.
        IMCodec::ImageLoader& fImageLoader;
    };

    // Opening policy that normalizes paths and coordinates data-only browsing handoffs.
    class ImageOpenController
    {
      public:

        static constexpr std::uint32_t MaxSupportedDimension = 16384;

        explicit ImageOpenController(std::unique_ptr<IImageFileLoader> imageFileLoader,
                                     BrowseSessionController* fileSessionController = nullptr);

        void SetBrowseSessionController(BrowseSessionController* fileSessionController);
        ImageLoadResult LoadFile(const LLUtils::native_string_type& filePath, IMCodec::PluginTraverseMode traverseMode,
                                 const ImageLoadContext& context);
        ImageLoadResult LoadFileOrFolder(const LLUtils::native_string_type& filePath,
                                         IMCodec::PluginTraverseMode traverseMode, const ImageLoadContext& context);

        static ImageLoadStatus ClassifyLoadResult(ResultCode resultCode, const IMCodec::ImageSharedPtr& image,
                                                  std::uint32_t maxSupportedDimension = MaxSupportedDimension);

      private:

        // Optional borrowed browsing controller used for file/folder navigation.
        BrowseSessionController* fBrowseSessionController = nullptr;
        // Owned CPU file-loading boundary used by direct opening.
        std::unique_ptr<IImageFileLoader> fImageFileLoader;
    };
}  // namespace OIV

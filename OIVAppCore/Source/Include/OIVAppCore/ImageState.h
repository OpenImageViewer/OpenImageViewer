#pragma once
#include <OIVAppCore/ViewerImage.h>
#include <ImageProcessing/Resampler.h>
#include <ImageUtil/AxisAlignedTransform.h>
#include <array>
#include <optional>
#include <OIVAppCore/ScreenSpaceGeometry.h>

namespace OIV
{
    constexpr double ResampleScaleThreshold = 0.8;

    // Cached usable pixel results, ordered by their processing dependencies.
    enum class ImageProcessingStage
    {
        // Selected frame or source after automatic file orientation.
        Source,
        // Source after user rotation and flipping, shared if unchanged.
        GeometricTransform,
        // Geometry converted and normalized for rendering without changing dimensions.
        RenderFormat,
        // Full-image fallback within the texture limit, shared with RenderFormat when it fits.
        Downscaled,
        // Optional zoom downsample displayed at one client pixel per texel.
        Resampled,
        // Internal bound for stage storage and the clean-stage sentinel.
        Count
    };

    // Lifecycle of statistics for the selected source generation.
    enum class ColorCountState
    {
        // No worker has requested this generation.
        NotRequested,
        // The existing single worker is counting this generation.
        Pending,
        // A successful count is available.
        Ready,
        // Counting failed and should not restart for this generation.
        Failed
    };

    // Data-only document and processing owner. Explicit Refresh computes cached
    // results, while observation never allocates pixels or registers a presentation.
    class ImageState
    {
      public:

        const ViewerImage* GetOpenedImage() const { return fOpenedImage ? &*fOpenedImage : nullptr; }
        const IMCodec::ImageSharedPtr& GetRetainedImage(ImageProcessingStage stage) const;
        const IMCodec::ImageSharedPtr& GetSelectedSource() const;
        IMCodec::ImageSharedPtr GetVisibleImage() const;
        LLUtils::PointF64 GetVisibleSize() const;
        bool IsDownscaled() const;
        const IMCodec::ImageSharedPtr& GetCachedScreenSpaceImage() const { return fScreenSpaceCache.image; }
        // Capture selection and client-space placement together so a frame evaluates cache reuse only once.
        struct Presentation
        {
            IMCodec::ImageSharedPtr image;
            LLUtils::PointF64 scale = LLUtils::PointF64::One;
            LLUtils::PointF64 offset{};
            bool canShowPixelGrid = false;
        };
        Presentation GetPresentation() const;
        LLUtils::PointI32 GetViewportSize() const { return fViewportSize; }
        void SetViewportSize(LLUtils::PointI32 size) { fViewportSize = size; }
        // Validated settings affect the next view evaluation or generation. Usable caches survive changes.
        void SetScreenSpaceWorkFactor(double factor) { fScreenSpaceWorkFactor = factor; }
        void SetScreenSpaceZoomTolerance(double tolerance) { fScreenSpaceZoomTolerance = tolerance; }
        void SetScreenSpacePixelBufferTargetBytes(uint64_t bytes) { fScreenSpacePixelBufferTargetBytes = bytes; }
        void SetScreenSpaceTexelThresholdPixels(double pixels) { fScreenSpaceTexelThresholdPixels = pixels; }
        // Generate detail only after interaction settles. Observation never allocates pixels.
        void RefreshScreenSpace();
        bool GetUseRainbowNormalization() const { return fUseRainbowNormalization; }
        IMUtil::AxisAlignedRotation GetAxisAlignedRotation() const { return fTransform.rotation; }
        IMUtil::AxisAlignedFlip GetAxisAlignedFlip() const { return fTransform.flip; }
        LLUtils::PointF64 GetScale() const { return fScale; }
        LLUtils::PointF64 GetOffset() const { return fOffset; }
        bool GetResample() const { return fResample; }
        uint64_t GetSourceGeneration() const { return fSourceGeneration; }
        ColorCountState GetColorCountState() const { return fColorCountState; }
        int64_t GetNumUniqueColors() const;
        void SetColorCountPending() { fColorCountState = ColorCountState::Pending; }
        void CompleteColorCount(int64_t count);
        void SetOpenedImage(ViewerImage image);
        void SetSelectedSource(IMCodec::ImageSharedPtr image);
        void ClearAll();
        void SetScale(LLUtils::PointF64 scale);
        void SetOffset(LLUtils::PointF64 offset) { fOffset = offset; }
        void SetUseRainbowNormalization(bool value);
        void Transform(IMUtil::AxisAlignedRotation rotation, IMUtil::AxisAlignedFlip flip);
        void ResetUserState();
        void SetResample(bool enabled);
        void Refresh();
        void Refresh(ImageProcessingStage requiredStage);

      private:

        void SetDirtyStage(ImageProcessingStage stage);
        IMCodec::ImageSharedPtr ProcessStage(ImageProcessingStage stage, IMCodec::ImageSharedPtr input);
        void AdvanceSourceGeneration();
        ScreenSpaceGeometry::View GetScreenSpaceView() const;
        LLUtils::PointF64 GetRenderScale(const IMCodec::ImageSharedPtr& image) const;
        struct ScreenSpaceCache
        {
            IMCodec::ImageSharedPtr image;
            LLUtils::RectI32 sourceRect{};
        };
        // View-dependent pixels are separate from content stages and remain available across pan and zoom.
        ScreenSpaceCache fScreenSpaceCache;
        LLUtils::PointI32 fViewportSize{};
        double fScreenSpaceWorkFactor    = 16.0;
        double fScreenSpaceZoomTolerance = 0.25;
        // Soft target for one output pixel buffer. Required viewport coverage may exceed it.
        uint64_t fScreenSpacePixelBufferTargetBytes = ScreenSpaceGeometry::DefaultPixelBufferTargetBytes;
        double fScreenSpaceTexelThresholdPixels     = ScreenSpaceGeometry::DefaultTexelThresholdPixels;
        // User geometric changes applied after the selected source's file orientation.
        IMUtil::AxisAlignedTransform fTransform{};
        // First invalid stage, Count when all requested results are current.
        ImageProcessingStage fDirtyStage = ImageProcessingStage::GeometricTransform;
        // Shared replacement-only stage pixels, without registration or GPU ownership.
        std::array<IMCodec::ImageSharedPtr, static_cast<size_t>(ImageProcessingStage::Count)> fImages;
        // Original opened document, separate from selected frames and processing results.
        std::optional<ViewerImage> fOpenedImage;
        // Existing grayscale/rainbow normalization choice.
        bool fUseRainbowNormalization = false;
        // Whether refresh should attempt the existing zoom downsampling policy.
        bool fResample = false;
        // Current zoom in client pixels per geometric texel.
        LLUtils::PointF64 fScale = LLUtils::PointF64::One;
        // Desired client placement, rounded when evaluating presentation geometry.
        LLUtils::PointF64 fOffset = LLUtils::PointF64::Zero;
        // Persistent CPU resampler reuses its allocation and threading configuration.
        Resampler fResampler;
        // Identity advanced by every selected-source replacement, including same-path reload.
        uint64_t fSourceGeneration = 0;
        // Statistics state belongs to fSourceGeneration, not pixel pointer identity.
        ColorCountState fColorCountState = ColorCountState::NotRequested;
        // Successful count, valid only when fColorCountState is Ready.
        int64_t fColorCount = 0;
    };
}  // namespace OIV

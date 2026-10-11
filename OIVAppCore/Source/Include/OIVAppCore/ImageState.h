#pragma once
#include <OIVAppCore/ViewerImage.h>
#include <ImageProcessing/Resampler.h>
#include <ImageUtil/AxisAlignedTransform.h>
#include <array>
#include <optional>

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
        // Desired client placement, rounded by viewer presentation policy.
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

#include <OIVAppCore/ImageState.h>
#include <ImageUtil/ImageUtil.h>
#include <LLUtils/Exception.h>
#include <algorithm>
#include <cassert>

namespace OIV
{
    const IMCodec::ImageSharedPtr& ImageState::GetRetainedImage(ImageProcessingStage stage) const
    {
        assert(stage >= ImageProcessingStage::Source && stage < ImageProcessingStage::Count);
        return fImages[static_cast<size_t>(stage)];
    }
    const IMCodec::ImageSharedPtr& ImageState::GetSelectedSource() const
    {
        return GetRetainedImage(ImageProcessingStage::Source);
    }
    IMCodec::ImageSharedPtr ImageState::GetVisibleImage() const
    {
        const auto& resampled = GetRetainedImage(ImageProcessingStage::Resampled);
        return resampled ? resampled : GetRetainedImage(ImageProcessingStage::RenderFormat);
    }
    LLUtils::PointF64 ImageState::GetVisibleSize() const
    {
        auto image = GetRetainedImage(ImageProcessingStage::Resampled);
        if (!image)
            image = GetRetainedImage(ImageProcessingStage::GeometricTransform);
        if (!image)
            image = GetSelectedSource();
        const auto dimensions = image ? static_cast<LLUtils::PointF64>(image->GetDimensions())
                                      : LLUtils::PointF64::Zero;
        return GetRetainedImage(ImageProcessingStage::Resampled) ? dimensions : dimensions * fScale;
    }
    void ImageState::AdvanceSourceGeneration()
    {
        ++fSourceGeneration;
        fColorCountState = ColorCountState::NotRequested;
        fColorCount      = 0;
    }
    void ImageState::SetOpenedImage(ViewerImage image)
    {
        fOpenedImage       = std::move(image);
        const auto& opened = fOpenedImage->pixels;
        SetSelectedSource(opened && opened->GetItemType() == IMCodec::ImageItemType::Container ? opened->GetSubImage(0)
                                                                                               : opened);
    }
    void ImageState::SetSelectedSource(IMCodec::ImageSharedPtr image)
    {
        fImages.fill(nullptr);
        fImages[static_cast<size_t>(ImageProcessingStage::Source)] = std::move(image);
        fDirtyStage                                                = ImageProcessingStage::GeometricTransform;
        AdvanceSourceGeneration();
    }
    void ImageState::ClearAll()
    {
        fImages.fill(nullptr);
        fOpenedImage.reset();
        fDirtyStage = ImageProcessingStage::GeometricTransform;
        AdvanceSourceGeneration();
    }
    void ImageState::SetDirtyStage(ImageProcessingStage stage)
    {
        fDirtyStage = std::min(fDirtyStage, stage);
        // Observation exposes only valid cached results. Invalidated outputs release
        // their data at the model boundary, without waiting for a later frame.
        std::fill(fImages.begin() + static_cast<size_t>(stage), fImages.end(), nullptr);
    }
    void ImageState::SetUseRainbowNormalization(bool value)
    {
        if (fUseRainbowNormalization != value)
        {
            fUseRainbowNormalization = value;
            SetDirtyStage(ImageProcessingStage::RenderFormat);
        }
    }
    void ImageState::SetScale(LLUtils::PointF64 scale)
    {
        if (fScale != scale)
        {
            fScale = scale;
            if (fResample)
                SetDirtyStage(ImageProcessingStage::Resampled);
        }
    }
    void ImageState::SetResample(bool enabled)
    {
        if (fResample != enabled)
        {
            fResample = enabled;
            if (enabled)
                SetDirtyStage(ImageProcessingStage::Resampled);
            else
                fImages[static_cast<size_t>(ImageProcessingStage::Resampled)].reset();
        }
    }
    void ImageState::ResetUserState()
    {
        fTransform               = {};
        fUseRainbowNormalization = false;
        SetDirtyStage(ImageProcessingStage::GeometricTransform);
    }
    int64_t ImageState::GetNumUniqueColors() const
    {
        return fColorCountState == ColorCountState::Ready    ? fColorCount
               : fColorCountState == ColorCountState::Failed ? UniqueColorsFailed
                                                             : UniqueColorsUninitialized;
    }
    void ImageState::CompleteColorCount(int64_t count)
    {
        fColorCountState = count < 0 ? ColorCountState::Failed : ColorCountState::Ready;
        fColorCount      = count < 0 ? 0 : count;
    }
    void ImageState::Refresh()
    {
        Refresh(fResample ? ImageProcessingStage::Resampled : ImageProcessingStage::RenderFormat);
    }
    void ImageState::Refresh(ImageProcessingStage requiredStage)
    {
        assert(requiredStage >= ImageProcessingStage::Source && requiredStage < ImageProcessingStage::Count);
        requiredStage = std::min(requiredStage,
                                 fResample ? ImageProcessingStage::Resampled : ImageProcessingStage::RenderFormat);
        if (GetSelectedSource())
        {
            // Cache prerequisites in order and publish only successfully processed replacements.
            while (fDirtyStage <= requiredStage)
            {
                const auto index = static_cast<size_t>(fDirtyStage);
                fImages[index]   = ProcessStage(fDirtyStage, fImages[index - 1]);
                fDirtyStage      = static_cast<ImageProcessingStage>(index + 1);
            }
        }
    }
    IMCodec::ImageSharedPtr ImageState::ProcessStage(ImageProcessingStage stage, IMCodec::ImageSharedPtr input)
    {
        switch (stage)
        {
            case ImageProcessingStage::GeometricTransform:
                return fTransform.rotation != IMUtil::AxisAlignedRotation::None ||
                               fTransform.flip != IMUtil::AxisAlignedFlip::None
                           ? IMUtil::ImageUtil::Transform(fTransform, input)
                           : input;
            case ImageProcessingStage::RenderFormat:
            {
                auto result = input->GetTexelFormat() == IMCodec::TexelFormat::I_R8_G8_B8_A8
                                  ? input
                                  : IMUtil::ImageUtil::ConvertImageWithNormalization(
                                        input, IMCodec::TexelFormat::I_R8_G8_B8_A8, fUseRainbowNormalization);
                if (!result)
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "Unable to convert image");
                return result;
            }
            case ImageProcessingStage::Resampled:
                return fScale.x > ResampleScaleThreshold || fScale.y > ResampleScaleThreshold
                           ? nullptr
                           : fResampler.Resample(
                                 input, static_cast<LLUtils::PointI32>(
                                            (static_cast<LLUtils::PointF64>(input->GetDimensions()) * fScale).Round()));
            default:
                LL_EXCEPTION_UNEXPECTED_VALUE;
        }
    }
    void ImageState::Transform(IMUtil::AxisAlignedRotation relativeRotation, IMUtil::AxisAlignedFlip flip)
    {
        // Accumulate user transforms without changing Source pixels. Equivalent
        // axis-aligned combinations retain the existing visual transformation policy.
        IMUtil::AxisAlignedTransform newTransform{};

        newTransform.flip = static_cast<IMUtil::AxisAlignedFlip>(flip) ^ fTransform.flip;

        const bool isSingleAxisFlip = newTransform.flip == IMUtil::AxisAlignedFlip::Horizontal ||
                                      newTransform.flip == IMUtil::AxisAlignedFlip::Vertical;

        const int rotationDirection = isSingleAxisFlip ? -1 : 1;

        newTransform.rotation = static_cast<IMUtil::AxisAlignedRotation>(
            (4 + static_cast<int>(relativeRotation * rotationDirection) + static_cast<int>(fTransform.rotation)) % 4);

        if (newTransform.flip != fTransform.flip || newTransform.rotation != fTransform.rotation)
        {
            // Use axis aligned transformations identities for better visual perception

            if (newTransform.rotation == IMUtil::AxisAlignedRotation::None &&
                newTransform.flip == (IMUtil::AxisAlignedFlip::Horizontal | IMUtil::AxisAlignedFlip::Vertical))
                newTransform = {IMUtil::AxisAlignedRotation::Rotate180, IMUtil::AxisAlignedFlip::None};

            if (newTransform.rotation == IMUtil::AxisAlignedRotation::Rotate180 &&
                newTransform.flip == IMUtil::AxisAlignedFlip::Vertical)
                newTransform = {IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Horizontal};

            if (newTransform.rotation == IMUtil::AxisAlignedRotation::Rotate90CW &&
                newTransform.flip == IMUtil::AxisAlignedFlip::Horizontal)
                newTransform = {IMUtil::AxisAlignedRotation::Rotate90CCW, IMUtil::AxisAlignedFlip::Vertical};

            if (newTransform.rotation == IMUtil::AxisAlignedRotation::Rotate90CCW &&
                newTransform.flip == IMUtil::AxisAlignedFlip::Horizontal)
                newTransform = {IMUtil::AxisAlignedRotation::Rotate90CW, IMUtil::AxisAlignedFlip::Vertical};

            fTransform = newTransform;

            SetDirtyStage(ImageProcessingStage::GeometricTransform);
        }
    }

}  // namespace OIV

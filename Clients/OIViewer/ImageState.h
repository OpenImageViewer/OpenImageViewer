#pragma once
#include <array>
#include "OIVImage/OIVBaseImage.h"
#include "OIVImage/OIVFileImage.h"
#include "../../oiv/Source/ImageUtil.h"

namespace OIV
{
    //TODO: adjust resampling conditions
    constexpr double ResampleScaleThreshold = 0.8;
    enum class ImageChainStage
    {
          SourceImage = 0
        , Begin = SourceImage
        , Deformed
        , Rasterized
        , Resampled
        , Count
    };

    class ImageChain
    {
    public:
        OIVBaseImageSharedPtr& Get(ImageChainStage stage);
        const OIVBaseImageSharedPtr& Get(ImageChainStage stage) const;
        void Reset();

    private:
        std::array<OIVBaseImageSharedPtr, static_cast<size_t>(ImageChainStage::Count)> Chain;
    };


    class ImageState
    {
    public:// const methods:

        OIVBaseImageSharedPtr GetOpenedImage() const { return fOpenedImage; }
        bool GetUseRainbowNormalization() const { return fUseRainbowNormalization; }
        IMUtil::OIV_AxisAlignedRotation GetAxisAlignedRotation() const { return fTransform.rotation; }
        IMUtil::OIV_AxisAlignedFlip GetAxisAlignedFlip() const { return fTransform.flip; }
        
        LLUtils::PointF64       GetScale() const { return fScale; }
        LLUtils::PointF64       GetOffset() const { return fOffset; }

        bool GetResample() const;

        LLUtils::PointF64 GetVisibleSize();
        OIVBaseImageSharedPtr GetVisibleImage() const;

    public:// mutating methods:
        void SetImageChainRoot(OIVBaseImageSharedPtr image);
        OIVBaseImageSharedPtr& GetImage(ImageChainStage imageStage);
        void SetScale(LLUtils::PointF64 scale);
        void SetOffset(LLUtils::PointF64 offset);
        void SetUseRainbowNormalization(bool val);
        void SetOpenedImage(const OIVBaseImageSharedPtr& image);
        void ClearAll();
        void Transform(OIV_AxisAlignedRotation relative_rotation, OIV_AxisAlignedFlip flip);
        void ResetUserState();
        void SetResample(bool resample);
        void Refresh();

    private: //methods 

        void SetDirtyStage(ImageChainStage dirtyStage);
        OIVBaseImageSharedPtr ProcessStage(ImageChainStage stage, OIVBaseImageSharedPtr image);
        void Refresh(ImageChainStage requiredImageStage);
        void UpdateImageParameters(OIVBaseImageSharedPtr visibleImage, bool visible);

        bool IsActuallyResampled() const;
        ImageChain& GetWorkingImageChain();
        const ImageChain& GetWorkingImageChain() const;
    private: // member fields

        IMUtil::OIV_AxisAlignedTransform fTransform{ IMUtil::OIV_AxisAlignedRotation::None, IMUtil::OIV_AxisAlignedFlip::None };
        ImageChainStage fDirtyStage = ImageChainStage::Begin;
        ImageChain fCurrentImageChain;
        OIVBaseImageSharedPtr fOpenedImage;
        bool fUseRainbowNormalization = false;
        ImageChainStage fFinalProcessingStage = ImageChainStage::Rasterized;
        LLUtils::PointF64 fScale = LLUtils::PointF64::One;
        LLUtils::PointF64 fOffset = LLUtils::PointF64::Zero;
    };
}

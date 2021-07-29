#pragma once
#include <array>
#include "OIVImage/OIVBaseImage.h"
#include "OIVImage/OIVFileImage.h"

namespace OIV
{
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
        OIV_AxisAlignedRotation GetAxisAlignedRotation() const { return fAxisAlignedRotation; }
        OIV_AxisAlignedFlip     GetAxisAlignedFlip() const { return fAxisAlignedFlip; }
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
        OIV_AxisAlignedRotation fAxisAlignedRotation = OIV_AxisAlignedRotation::AAT_None;
        OIV_AxisAlignedFlip     fAxisAlignedFlip = OIV_AxisAlignedFlip::AAF_None;
        ImageChainStage fDirtyStage = ImageChainStage::Begin;
        ImageChain fCurrentImageChain;
        OIVBaseImageSharedPtr fOpenedImage;
        bool fUseRainbowNormalization = false;
        ImageChainStage fFinalProcessingStage = ImageChainStage::Rasterized;
        OIV_Filter_type fFilterType= OIV_Filter_type::FT_None;
        LLUtils::PointF64 fScale = LLUtils::PointF64::One;
        LLUtils::PointF64 fOffset = LLUtils::PointF64::Zero;
    };
}

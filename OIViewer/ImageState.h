#pragma once
#include <array>
#include "OIVImage/OIVBaseImage.h"
#include "OIVImage/OIVFileImage.h"

namespace OIV
{
    enum class ImageChainStage
    {
          NewImage = -1
        , SourceImage = 0
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

        const ImageChain& GetWorkingImageChain() const;
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
        ImageChain& GetWorkingImageChain();

        OIVBaseImageSharedPtr ProcessStage(ImageChainStage stage, OIVBaseImageSharedPtr image);
        void Refresh();
        void ResetPreviousImageChain();
        void SetScale(LLUtils::PointF64 scale);
        void SetOffset(LLUtils::PointF64 offset);
        void SetUseRainbowNormalization(bool val);
        void SetOpenedImage(const OIVBaseImageSharedPtr& image);
        void ClearAll();
        void Transform(OIV_AxisAlignedRotation relative_rotation, OIV_AxisAlignedFlip flip);
        void SetDirtyStage(ImageChainStage dirtyStage);
        void ResetUserState();
        void SetResample(bool resample);


    private: //methods 
        void ResetDirtyStage();
        bool IsActuallyResampled() const;
    private: // member fields
        OIV_AxisAlignedRotation fAxisAlignedRotation = OIV_AxisAlignedRotation::AAT_None;
        OIV_AxisAlignedFlip     fAxisAlignedFlip = OIV_AxisAlignedFlip::AAF_None;
        ImageChainStage fDirtyStage = ImageChainStage::Count;
        ImageChain fCurrentImageChain;
        ImageChain fPreviousImageChain;
        OIVBaseImageSharedPtr fOpenedImage;
        bool fPreviousImageChainDirty = false;
        bool fUseRainbowNormalization = false;
        ImageChainStage fFinalProcessingStage = ImageChainStage::Rasterized;
        OIV_Filter_type fFilterType= OIV_Filter_type::FT_None;
        LLUtils::PointF64 fScale = LLUtils::PointF64::One;
        LLUtils::PointF64 fOffset = LLUtils::PointF64::Zero;
    };
}

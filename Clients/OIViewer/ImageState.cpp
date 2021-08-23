#include "ImageState.h"
#include "OIVCommands.h"
#include "OIVImage/OIVHandleImage.h"
#include "Helpers/OIVImageHelper.h"

namespace OIV
{
    //Image chain 
    OIVBaseImageSharedPtr& ImageChain::Get(ImageChainStage stage)
    {
        return Chain[static_cast<size_t>(stage)];
    }

    const OIVBaseImageSharedPtr& ImageChain::Get(ImageChainStage stage) const
    {
        return const_cast<ImageChain*>(this)->Get(stage);
    }

    void ImageChain::Reset()
    {
        for (auto& e : Chain)
            e.reset();
    }


    //Image state
    
    void ImageState::Refresh()
    {
        Refresh(fFinalProcessingStage);
    }


    // Reprocess image according to dirty stage. 
    void ImageState::Refresh(ImageChainStage requiredImageStage)
    {
        if (fDirtyStage <= requiredImageStage && fOpenedImage != nullptr)
        {
            ImageChainStage currentStage = fDirtyStage;
            ImageChainStage previousStage = static_cast<ImageChainStage>(std::max((int)currentStage - 1, 0));

            if (previousStage == currentStage)
            {
                //previousStage = static_cast<ImageChainStage>((int)previousStage + 1);
                currentStage = static_cast<ImageChainStage>((int)previousStage + 1);
            }

            ImageChainStage maxImageStage = std::min(requiredImageStage, fFinalProcessingStage);

            while (currentStage <= maxImageStage)
            {
                ImageChainStage previousStage = static_cast<ImageChainStage>(std::max((int)currentStage - 1, 0));
                ImageChainStage nextStage = static_cast<ImageChainStage>(static_cast<int>(currentStage) + 1);
                fCurrentImageChain.Get(currentStage) = ProcessStage(currentStage, fCurrentImageChain.Get(previousStage));
                currentStage = nextStage;
            }
            fDirtyStage = static_cast<ImageChainStage>( std::min<int>(static_cast<int>(maxImageStage) + static_cast<int>(1), static_cast<int>(ImageChainStage::Count)));

        }
    }

    void ImageState::SetUseRainbowNormalization(bool val)
    {
        if (fUseRainbowNormalization != val)
        {
            fUseRainbowNormalization = val;
            SetDirtyStage(ImageChainStage::Rasterized);
        }
    }

    void ImageState::SetOpenedImage(const OIVBaseImageSharedPtr& image)
    {
        fOpenedImage = image;
        SetImageChainRoot(fOpenedImage);
        SetDirtyStage(ImageChainStage::Begin);
    }

    void ImageState::ClearAll()
    {
        fCurrentImageChain.Reset();
        fOpenedImage.reset();
    }

    void ImageState::Transform(OIV_AxisAlignedRotation relativeRotation, OIV_AxisAlignedFlip flip)
    {

        const bool isHorizontalFlip = (int)(fAxisAlignedFlip & static_cast<int>(OIV_AxisAlignedFlip::AAF_Horizontal)) != 0;
        const bool isVerticalFlip = (int)(fAxisAlignedFlip & static_cast<int>(OIV_AxisAlignedFlip::AAF_Vertical)) != 0;
        const bool isFlip = isVerticalFlip || isHorizontalFlip;

        // the two options to manage axes aligned transofrmation are either
        //1. modify the original image so transformation would cumulative - not the case here.
        //2. preserve the original image and add compute the desired transformation - the case here.
        // for simplicity the code accepts only 90 degrees rotations. 

        if (relativeRotation != OIV_AxisAlignedRotation::AAT_None)
        {
            if (relativeRotation != AAT_Rotate90CW && relativeRotation != AAT_Rotate90CCW)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::LogicError, "Rotating image is currently limited to 90 degrees in CW/CCW");

            fAxisAlignedRotation = static_cast<OIV_AxisAlignedRotation>((static_cast<int>(relativeRotation + fAxisAlignedRotation) % 4));
            //If switching axes by rotating 90 degrees and flip is applied, then flip the flip axes.
            if (isFlip == true)
                fAxisAlignedFlip = static_cast<OIV_AxisAlignedFlip>(static_cast<int>(fAxisAlignedFlip) ^ static_cast<int>(3));
        }

        fAxisAlignedFlip = static_cast<OIV_AxisAlignedFlip>(static_cast<int>(fAxisAlignedFlip) ^ static_cast<int>(flip));

        SetDirtyStage(ImageChainStage::Deformed);

    }


    void ImageState::SetDirtyStage(ImageChainStage dirtyStage)
    {
        if (dirtyStage < fDirtyStage)
            fDirtyStage = dirtyStage;
    }

    void ImageState::ResetUserState()
    {
        fAxisAlignedRotation = OIV_AxisAlignedRotation::AAT_None;
        fAxisAlignedFlip = OIV_AxisAlignedFlip::AAF_None;
        fUseRainbowNormalization = false;
        SetDirtyStage(ImageChainStage::Deformed);
    }

    void ImageState::SetResample(bool resample)
    {
            ImageChainStage targetProcessStage = resample ? ImageChainStage::Resampled : ImageChainStage::Rasterized;

            if (fFinalProcessingStage != targetProcessStage)
            {
                fFinalProcessingStage = targetProcessStage;

                if (fFinalProcessingStage == ImageChainStage::Rasterized)
                {
                    auto rasterized = fCurrentImageChain.Get(ImageChainStage::Rasterized);
                    if (rasterized != nullptr)
                        UpdateImageParameters(rasterized, true);
                    fCurrentImageChain.Get(ImageChainStage::Resampled).reset();
                }
                else if (fFinalProcessingStage == ImageChainStage::Resampled)
                {
                    SetDirtyStage(ImageChainStage::Resampled);
                }
            }
        }
       

    bool ImageState::IsActuallyResampled() const
    {
        return GetWorkingImageChain().Get(ImageChainStage::Resampled) != nullptr;
    }

    void ImageState::SetScale(LLUtils::PointF64 scale)  
    {
        if (fScale != scale)
        {
            fScale = scale;
            auto visibleImage = GetVisibleImage();
            if (visibleImage != nullptr)
            {
                visibleImage->GetImageProperties().scale = IsActuallyResampled() == true ? LLUtils::PointF64::One : fScale;
                visibleImage->Update();
            }

            if (GetResample() == true)
                SetDirtyStage(ImageChainStage::Resampled);
        }
    }

    void ImageState::SetOffset(LLUtils::PointF64 offset)
    {
        if (fOffset != offset)
        {
            fOffset = offset;
            auto visibleImage = GetVisibleImage();
            visibleImage->GetImageProperties().position = fOffset.Round();
            visibleImage->Update();
        }
    }



    void ImageState::UpdateImageParameters(OIVBaseImageSharedPtr visibleImage, bool visible)
    {
        visibleImage->GetImageProperties().position = GetOffset();
        visibleImage->GetImageProperties().opacity = 1.0;
        visibleImage->GetImageProperties().visible = visible;
        visibleImage->Update();
    }

    OIVBaseImageSharedPtr ImageState::GetVisibleImage() const
    {
        return IsActuallyResampled() == true ? GetWorkingImageChain().Get(ImageChainStage::Resampled) : GetWorkingImageChain().Get(ImageChainStage::Rasterized);
    }

    bool ImageState::GetResample() const 
    {
        switch (fFinalProcessingStage)
        {
        case ImageChainStage::Rasterized:
            return false;
        case ImageChainStage::Resampled:
            return true;
        default:
            LL_EXCEPTION_UNEXPECTED_VALUE;
        }
    }

    LLUtils::PointF64 ImageState::GetVisibleSize()
    {
        Refresh(fFinalProcessingStage);
        auto visiblImage = GetVisibleImage();
        using namespace LLUtils;
        PointF64 visibleImageSize(visiblImage->GetDescriptor().Width, visiblImage->GetDescriptor().Height);

        //If resampled, scale is already embedded in the image size, else multiplty by scale.
        return IsActuallyResampled() ? visibleImageSize : visibleImageSize * GetScale();
    }

    void ImageState::SetImageChainRoot(OIVBaseImageSharedPtr image)
    {
        auto& souceImageSlot = fCurrentImageChain.Get(ImageChainStage::SourceImage);

        //If current source image is still open and yet to be released from memory like when subimages are available, 
        // hide the current active source image.
        if (souceImageSlot != nullptr)
        {
            souceImageSlot->GetImageProperties().visible = false;
            souceImageSlot->Update();

        }
        //Assign the new source image        
        souceImageSlot = image;
        SetDirtyStage(ImageChainStage::SourceImage);
    }

    OIVBaseImageSharedPtr& ImageState::GetImage(ImageChainStage imageStage)
    {
        Refresh(imageStage);
        return fCurrentImageChain.Get(imageStage);
    }

    ImageChain& ImageState::GetWorkingImageChain()
    {
        return fCurrentImageChain;
    }

    const ImageChain& ImageState::GetWorkingImageChain() const
    {
        return fCurrentImageChain;
    }


    OIVBaseImageSharedPtr ImageState::ProcessStage(ImageChainStage stage, OIVBaseImageSharedPtr inputImage)
    {
        switch (stage)
        {
            break;
        case ImageChainStage::SourceImage:
            //Nothing to do.
            return inputImage;
            break;
        case ImageChainStage::Deformed:
            if (fAxisAlignedRotation != OIV_AxisAlignedRotation::AAT_None || fAxisAlignedFlip != OIV_AxisAlignedFlip::AAF_None)
            {
                ImageHandle transformedHandle = ImageHandleNull;
                
                OIVCommands::TransformImage(inputImage->GetDescriptor().ImageHandle
                    , fAxisAlignedRotation, fAxisAlignedFlip, transformedHandle);
                
                inputImage->GetImageProperties().visible = false;
                inputImage->Update();

                auto deformed = std::make_shared<OIVHandleImage>(transformedHandle);
                return deformed;
            }
            else
            {
                return inputImage;
            }

            break;
        case ImageChainStage::Rasterized:
        {

            inputImage->GetImageProperties().visible = false;
            inputImage->Update();

            auto rasterized = OIVImageHelper::GetRendererCompatibleImage(inputImage, fUseRainbowNormalization);
            rasterized->GetImageProperties().scale = fScale;
            UpdateImageParameters(rasterized, true);
            return rasterized;
        }
           break;


        case ImageChainStage::Resampled:
        {
            //TODO: adjust resampling conditions
            if (GetScale().x > 0.8 || GetScale().y > 0.8)
            {
                return nullptr;
            }
            else
            {
                auto rasterized = fCurrentImageChain.Get(ImageChainStage::Rasterized);
                LLUtils::PointF64 originalImageSize = { static_cast<double>(rasterized->GetDescriptor().Width) , static_cast<double>(rasterized->GetDescriptor().Height) };
                auto resampled = OIVImageHelper::ResampleImage(rasterized, static_cast<LLUtils::PointI32>((originalImageSize * GetScale()).Round()));
                //Resampled image is pixel perfect in relation to the client window, so no scale.

                resampled->GetImageProperties().scale = LLUtils::PointF64::One;

                inputImage->GetImageProperties().visible = false;
                inputImage->Update();
                
                UpdateImageParameters(resampled, true);
                return resampled;
            }
        }
            break;
        default:
            LL_EXCEPTION_UNEXPECTED_VALUE;
        }
    }
}

  
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
        //return Chain[static_cast<size_t>(stage)];
    }

    void ImageChain::Reset()
    {
        for (auto& e : Chain)
            e.reset();
    }


    //Image state


    // Reprocess image according to dirty stage. 
    void ImageState::Refresh()
    {
        if (fDirtyStage <= ImageChainStage::Resampled && fOpenedImage != nullptr)
        {
            //For faster refresh don't delete the old image before displaying the new one. 
            fPreviousImageChain = fCurrentImageChain;
            fPreviousImageChainDirty = true;

            // hide previous image chain.
            if (fPreviousImageChain.Get(ImageChainStage::Resampled) != nullptr)
            {
                fPreviousImageChain.Get(ImageChainStage::Resampled)->GetImageProperties().opacity = 0.0;
                fPreviousImageChain.Get(ImageChainStage::Resampled)->Update();
            }


            //If a new image has been opened set it as the first
            if (fDirtyStage == ImageChainStage::NewImage)
            {
                SetImageChainRoot(fOpenedImage);
                fDirtyStage = ImageChainStage::SourceImage;
            }

            ImageChainStage currentStage = fDirtyStage;
            ImageChainStage previousStage = static_cast<ImageChainStage>(std::max((int)currentStage - 1, 0));

            if (previousStage == currentStage)
            {
                //previousStage = static_cast<ImageChainStage>((int)previousStage + 1);
                currentStage = static_cast<ImageChainStage>((int)previousStage + 1);
            }


            while (currentStage <= ImageChainStage::Resampled)
            {
                ImageChainStage previousStage = static_cast<ImageChainStage>(std::max((int)currentStage - 1, 0));
                ImageChainStage nextStage = static_cast<ImageChainStage>(static_cast<int>(currentStage) + 1);
                fCurrentImageChain.Get(currentStage) = ProcessStage(currentStage, fCurrentImageChain.Get(previousStage));
                currentStage = nextStage;
            }

            fCurrentImageChain.Get(ImageChainStage::Resampled)->GetImageProperties().opacity = 1.0;
            fCurrentImageChain.Get(ImageChainStage::Resampled)->Update();

            ResetDirtyStage();
        }
    }

    void ImageState::ResetPreviousImageChain()
    {
        if (fPreviousImageChainDirty == true)
        {
            fPreviousImageChain.Reset();
            fPreviousImageChainDirty = false;
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
        fDirtyStage = ImageChainStage::NewImage;
    }

    void ImageState::ClearAll()
    {
        fPreviousImageChain.Reset();
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

    void ImageState::ResetDirtyStage()
    {
        fDirtyStage = ImageChainStage::Count;
    }

    void ImageState::SetImageChainRoot(OIVBaseImageSharedPtr image)
    {
        fPreviousImageChain.Get(ImageChainStage::SourceImage) = fCurrentImageChain.Get(ImageChainStage::SourceImage);
        fCurrentImageChain.Get(ImageChainStage::SourceImage) = image;
        SetDirtyStage(ImageChainStage::SourceImage);
    }

    ImageChain& ImageState::GetWorkingImageChain()
    {
        return fCurrentImageChain;
    }

    const ImageChain& ImageState::GetWorkingImageChain() const
    {
        return fCurrentImageChain;
    }


    OIVBaseImageSharedPtr ImageState::ProcessStage(ImageChainStage stage, OIVBaseImageSharedPtr image)
    {
        switch (stage)
        {
        case ImageChainStage::NewImage:
            return image;
            break;
        case ImageChainStage::SourceImage:
            //Nothing to do.
            return image;
            break;
        case ImageChainStage::Deformed:
            if (fAxisAlignedRotation != OIV_AxisAlignedRotation::AAT_None || fAxisAlignedFlip != OIV_AxisAlignedFlip::AAF_None)
            {

                ImageHandle transformedHandle = ImageHandleNull;
                OIVCommands::TransformImage(fCurrentImageChain.Get(ImageChainStage::SourceImage)->GetDescriptor().ImageHandle
                    , fAxisAlignedRotation, fAxisAlignedFlip, transformedHandle);

                return std::make_shared<OIVHandleImage>(transformedHandle);
            }
            else
            {
                return image;
            }

            break;
        case ImageChainStage::Rasterized:
            return OIVImageHelper::GetRendererCompatibleImage(fCurrentImageChain.Get(ImageChainStage::Deformed), fUseRainbowNormalization);
            break;
        case ImageChainStage::Resampled:
            //TODO: implement re-samlping, currently return input iamge
            return image;
            break;
        default:
            LL_EXCEPTION_UNEXPECTED_VALUE;
        }
    }
}

  
#include "ImageState.h"
#include "OIVCommands.h"
#include "Helpers/OIVImageHelper.h"
#include <ImageUtil/ImageUtil.h>

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
        if (fOpenedImage->GetImage()->GetItemType() != IMCodec::ImageItemType::Container)
            SetImageChainRoot(fOpenedImage);
        else
            SetImageChainRoot(std::make_shared<OIVBaseImage>(ImageSource::GeneratedByLib, fOpenedImage->GetImage()->GetSubImage(0)));
    }

    void ImageState::ClearAll()
    {
        fCurrentImageChain.Reset();
        fOpenedImage.reset();
    }

    void ImageState::Transform(IMUtil::AxisAlignedRotation relativeRotation, IMUtil::AxisAlignedFlip flip)
    {

        // the two options to manage axes aligned transofrmation are either
        //1. modify the original image so transformation would cumulative - not the case here.
        //2. preserve the original image and add compute the desired transformation - the case here.
        // for simplicity the code accepts rotation only in 90 degrees rotations increments.


     
        IMUtil::AxisAlignedTransform newTransform{};


        newTransform.flip = static_cast<IMUtil::AxisAlignedFlip>( flip) ^ fTransform.flip;

        const bool isSingleAxisFlip = newTransform.flip == IMUtil::AxisAlignedFlip::Horizontal || newTransform.flip == IMUtil::AxisAlignedFlip::Vertical;

        const int rotationDirection = isSingleAxisFlip ? -1 : 1;

        newTransform.rotation = static_cast<IMUtil::AxisAlignedRotation>((4 + static_cast<int>(relativeRotation * rotationDirection) + static_cast<int>(fTransform.rotation)) % 4);

        if (newTransform.flip != fTransform.flip || newTransform.rotation != fTransform.rotation)
        {

            // Use axis aligned transformations identities for better visual perception

            if (newTransform.rotation == IMUtil::AxisAlignedRotation::None
                && newTransform.flip == (IMUtil::AxisAlignedFlip::Horizontal | IMUtil::AxisAlignedFlip::Vertical))
                newTransform = { IMUtil::AxisAlignedRotation::Rotate180, IMUtil::AxisAlignedFlip::None };


            if (newTransform.rotation == IMUtil::AxisAlignedRotation::Rotate180
                && newTransform.flip == IMUtil::AxisAlignedFlip::Vertical)
                newTransform = { IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Horizontal };

            if (newTransform.rotation == IMUtil::AxisAlignedRotation::Rotate90CW
                && newTransform.flip == IMUtil::AxisAlignedFlip::Horizontal)
                newTransform = { IMUtil::AxisAlignedRotation::Rotate90CCW, IMUtil::AxisAlignedFlip::Vertical};

            if (newTransform.rotation == IMUtil::AxisAlignedRotation::Rotate90CCW
                && newTransform.flip == IMUtil::AxisAlignedFlip::Horizontal)
                newTransform = { IMUtil::AxisAlignedRotation::Rotate90CW, IMUtil::AxisAlignedFlip::Vertical };

            fTransform = newTransform;


            SetDirtyStage(ImageChainStage::Deformed);
        }

    }


    void ImageState::SetDirtyStage(ImageChainStage dirtyStage)
    {
        if (dirtyStage < fDirtyStage)
            fDirtyStage = dirtyStage;
    }

    void ImageState::ResetUserState()
    {
        fTransform = { IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::None };
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
                visibleImage->SetScale(IsActuallyResampled() == true ? LLUtils::PointF64::One : fScale);
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
            visibleImage->SetPosition(fOffset.Round());
        }
    }

    void ImageState::UpdateImageParameters(OIVBaseImageSharedPtr visibleImage, bool visible)
    {
        visibleImage->SetVisible(visible);
        visibleImage->SetPosition(GetOffset());
        visibleImage->SetOpacity(1.0);
        visibleImage->SetImageRenderMode(OIV_Image_Render_mode::IRM_MainImage);
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
        PointF64 visibleImageSize = static_cast<PointF64>(visiblImage->GetImage()->GetDimensions());

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
            souceImageSlot->SetVisible(false);
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
            if (fTransform.rotation != IMUtil::AxisAlignedRotation::None || fTransform.flip != IMUtil::AxisAlignedFlip::None)
            {
                auto deformed = IMUtil::ImageUtil::Transform(fTransform, inputImage->GetImage());

                inputImage->SetVisible(false);

                auto deformedOIVImage = std::make_shared<OIVBaseImage>(ImageSource::GeneratedByLib, deformed);
                return deformedOIVImage;
            }
            else
            {
                return inputImage;
            }

            break;
        case ImageChainStage::Rasterized:
        {

            inputImage->SetVisible(false);

            auto rasterized = OIVImageHelper::GetRendererCompatibleImage(inputImage, fUseRainbowNormalization);
            rasterized->SetScale(fScale);
            UpdateImageParameters(rasterized, true);
            return rasterized;
        }
           break;


        case ImageChainStage::Resampled:
        {
            if (GetScale().x > ResampleScaleThreshold || GetScale().y > ResampleScaleThreshold)
            {
                return nullptr;
            }
            else
            {
                auto rasterized = fCurrentImageChain.Get(ImageChainStage::Rasterized);
                LLUtils::PointF64 originalImageSize = static_cast<LLUtils::PointF64>(rasterized->GetImage()->GetDimensions());
                auto resampled = OIVImageHelper::ResampleImage(rasterized, static_cast<LLUtils::PointI32>((originalImageSize * GetScale()).Round()));
                //Resampled image is pixel perfect in relation to the client window, so no scale.

                resampled->SetScale(LLUtils::PointF64::One);
                inputImage->SetVisible(false);

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

  
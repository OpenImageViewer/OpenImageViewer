#include <ImageProcessing/FileImage.h>
#include <ImageUtil/ImageUtil.h>

namespace OIV
{
    IMUtil::AxisAlignedTransform ResolveExifRotation(unsigned short exifRotation)
    {
        //  1 = Horizontal(normal)
        //	2 = Mirror horizontal
        //	3 = Rotate 180
        //	4 = Mirror vertical
        //	5 = Mirror horizontal and rotate 270 CW
        //	6 = Rotate 90 CW
        //	7 = Mirror horizontal and rotate 90 CW
        //	8 = Rotate 270 CW

        IMUtil::AxisAlignedTransform transform{};
        switch (exifRotation)
        {
            case 1:
                //  1 = Horizontal(normal)
                break;
            case 2:
                //	2 = Mirror horizontal
                transform.flip = IMUtil::AxisAlignedFlip::Horizontal;
                break;
            case 3:
                //	3 = Rotate 180
                transform.rotation = IMUtil::AxisAlignedRotation::Rotate180;
                break;
            case 4:
                //	4 = Mirror vertical
                transform.flip = IMUtil::AxisAlignedFlip::Vertical;
                break;
            case 5:
                //	5 = Mirror horizontal and rotate 270 CW
                // In OIV as opoosed to EXIF, rotation is done first, then flip, so flip vertically instead of
                // horizontally
                transform.rotation = IMUtil::AxisAlignedRotation::Rotate90CCW;
                transform.flip     = IMUtil::AxisAlignedFlip::Vertical;
                break;
            case 6:
                //	6 = Rotate 90 CW
                transform.rotation = IMUtil::AxisAlignedRotation::Rotate90CW;
                break;
            case 7:
                //	7 = Mirror horizontal and rotate 90 CW.
                // In OIV as opoosed to EXIF, rotation is done first, then flip, so flip vertically instead of
                // horizontally.
                transform.rotation = IMUtil::AxisAlignedRotation::Rotate90CW;
                transform.flip     = IMUtil::AxisAlignedFlip::Vertical;
                break;
            case 8:
                //	8 = Rotate 270 CW
                transform.rotation = IMUtil::AxisAlignedRotation::Rotate90CCW;
                break;
            default:
                break;
        }
        return transform;
    }

    IMCodec::ImageSharedPtr ApplyExifRotation(IMCodec::ImageSharedPtr image, int exitOrientation)
    {
        // IMUtil::AxisAlignedTransform transform =
        // static_cast<IMUtil::AxisAlignedRotation>(ResolveExifRotation(exitOrientation)); transform.flip =
        // IMUtil::AxisAlignedFlip::None;
        return IMUtil::ImageUtil::Transform(ResolveExifRotation(exitOrientation), image);
    }

    IMCodec::ImageSharedPtr DecodeFileImage(IMCodec::ImageLoader& loader, const LLUtils::native_string_type& fileName,
                                            IMCodec::ItemMetaDataSharedPtr& metaData,
                                            IMCodec::PluginTraverseMode traverseMode,
                                            IMCodec::ImageLoadFlags imageLoadFlags, const IMCodec::Parameters& params)
    {
        metaData.reset();
        IMCodec::ImageSharedPtr image;
        if (loader.Decode(fileName, imageLoadFlags, params, traverseMode, image) == IMCodec::ImageResult::Success &&
            image)
        {
            // Keep decoder color provenance while adding EXIF and applying orientation privately.
            metaData = image->GetImageItem()->metaData;
            IMCodec::ItemMetaDataSharedPtr exifMetaData;
            if (loader.LoadMetaData(fileName, exifMetaData) == IMCodec::ImageResult::Success && exifMetaData)
            {
                if (metaData)
                    metaData->exifData = std::move(exifMetaData->exifData);
                else
                    metaData = std::move(exifMetaData);
                image->GetImageItem()->metaData = metaData;
                const auto orientation          = metaData->exifData.orientation;
                if (orientation > 1)
                    image = ApplyExifRotation(image, orientation);
                for (uint16_t i = 0; i < image->GetNumSubImages(); ++i)
                    image->SetSubImage(i, ApplyExifRotation(image->GetSubImage(i), orientation));
            }
        }
        else
            image.reset();
        return image;
    }

}  // namespace OIV

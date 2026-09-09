#include <OIVImage/OIVFileImage.h>
#include <LLUtils/FileMapping.h>
#include <LLUtils/StringUtility.h>
#include <Defs.h>
#include <ImageUtil/ImageUtil.h>
#include <utility>

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

    const LLUtils::native_string_type& OIVFileImage::GetFileName() const
    {
        return fFileName;
    }

    OIVFileImage::OIVFileImage(const LLUtils::native_string_type& fileName)
        : OIVBaseImage(ImageSource::File), fFileName(fileName)
    {
    }
    OIVFileImage::OIVFileImage(const LLUtils::native_string_type& fileName, IMCodec::ImageSharedPtr image)
        : OIVBaseImage(ImageSource::File, std::move(image)), fFileName(fileName)
    {
    }

    ResultCode OIVFileImage::Load(IMCodec::ImageLoader* imageCodec, IMCodec::PluginTraverseMode loaderFlags)
    {
        return Load(imageCodec, loaderFlags, IMCodec::ImageLoadFlags::None, {});
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
            // Apply file orientation while the data is private to the loader. OIV wrappers
            // created later receive the same oriented main image, subimages, and metadata.
            if (loader.LoadMetaData(fileName, metaData) == IMCodec::ImageResult::Success)
            {
                const auto orientation = metaData->exifData.orientation;
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

    ResultCode OIVFileImage::Load(IMCodec::ImageLoader* imageCodec, IMCodec::PluginTraverseMode loaderFlags,
                                  IMCodec::ImageLoadFlags imageLoadFlags, const IMCodec::Parameters& params)
    {
        IMCodec::ItemMetaDataSharedPtr metaData;
        auto image        = DecodeFileImage(*imageCodec, fFileName, metaData, loaderFlags, imageLoadFlags, params);
        ResultCode result = RC_FileNotSupported;
        if (image)
        {
            SetMetaData(std::move(metaData));
            SetUnderlyingImage(std::move(image));
            result = RC_Success;
        }
        return result;
    }
}  // namespace OIV

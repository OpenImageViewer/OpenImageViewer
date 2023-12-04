#include <OIVImage/OIVFileImage.h>
#include <LLUtils/FileMapping.h>
#include <LLUtils/StringUtility.h>
#include <defs.h>
#include <ImageUtil/ImageUtil.h>

namespace OIV
{
	IMUtil::AxisAlignedTransform  ResolveExifRotation(unsigned short exifRotation)
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
			// In OIV as opoosed to EXIF, rotation is done first, then flip, so flip vertically instead of horizontally
			transform.rotation = IMUtil::AxisAlignedRotation::Rotate90CCW;
			transform.flip = IMUtil::AxisAlignedFlip::Vertical;
			break;
		case 6:
			//	6 = Rotate 90 CW
			transform.rotation = IMUtil::AxisAlignedRotation::Rotate90CW;
			break;
		case 7:
			//	7 = Mirror horizontal and rotate 90 CW.
			// In OIV as opoosed to EXIF, rotation is done first, then flip, so flip vertically instead of horizontally.
			transform.rotation = IMUtil::AxisAlignedRotation::Rotate90CW;
			transform.flip = IMUtil::AxisAlignedFlip::Vertical;
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
		//IMUtil::AxisAlignedTransform transform = static_cast<IMUtil::AxisAlignedRotation>(ResolveExifRotation(exitOrientation));
		//transform.flip = IMUtil::AxisAlignedFlip::None;
		return IMUtil::ImageUtil::Transform(ResolveExifRotation(exitOrientation), image);
	}


    const LLUtils::native_string_type& OIVFileImage::GetFileName() const { return fFileName; }
    
	OIVFileImage::OIVFileImage(const LLUtils::native_string_type& fileName) : OIVBaseImage(ImageSource::File), fFileName(fileName) {}
    
	ResultCode OIVFileImage::Load(IMCodec::ImageLoader* imageCodec, IMCodec::PluginTraverseMode loaderFlags)
	{
		return Load(imageCodec, loaderFlags, IMCodec::ImageLoadFlags::None, {});
	}
    ResultCode OIVFileImage::Load(IMCodec::ImageLoader* imageCodec, IMCodec::PluginTraverseMode loaderFlags, IMCodec::ImageLoadFlags imageLoadFlags, const IMCodec::Parameters& params)
    {
		
		ResultCode result = RC_FileNotSupported;
		using namespace IMCodec;
		ImageSharedPtr image;
		ImageResult loadResult = imageCodec->Decode(fFileName, imageLoadFlags, params,loaderFlags, image);

		if (loadResult == ImageResult::Success)
		{
			if (image != nullptr)
			{
				ItemMetaDataSharedPtr metaData;
				if (imageCodec->LoadMetaData(fFileName, metaData) == ImageResult::Success)
				{
					auto exifOrientation = metaData->exifData.orientation;
					if (exifOrientation > 1)
					{
						// I see no use of using the original image, discard source image and use the image with exif rotation applied. 
						// If needed, responsibility for exif rotation can be transferred to the user by returning MetaData.exifOrientation.
						image = ApplyExifRotation(image, exifOrientation);
					}

					for (uint16_t i = 0; i < image->GetNumSubImages(); i++)
						image->SetSubImage(i, ApplyExifRotation(image->GetSubImage(i), exifOrientation));
				}

				SetMetaData(metaData);
				SetUnderlyingImage(image);
				result = RC_Success;
			}
		}
		return result;
    }
 }
#include <OIVImage/OIVFileImage.h>
#include <LLUtils/FileMapping.h>
#include <LLUtils/StringUtility.h>
#include <defs.h>
#include "../ImageUtil.h"

namespace OIV
{

	OIV_AxisAlignedRotation ResolveExifRotation(unsigned short exifRotation)
	{
		OIV_AxisAlignedRotation rotation;
		switch (exifRotation)
		{
		case 3:
			rotation = AAT_Rotate180;
			break;
		case 6:
			rotation = AAT_Rotate90CW;
			break;
		case 8:
			rotation = AAT_Rotate90CCW;
			break;
		default:
			rotation = AAT_None;
		}
		return rotation;
	}

	IMCodec::ImageSharedPtr ApplyExifRotation(IMCodec::ImageSharedPtr image)
	{
		IMUtil::OIV_AxisAlignedTransform transform;
		transform.rotation = static_cast<IMUtil::OIV_AxisAlignedRotation>(ResolveExifRotation(image->GetMetaData().exifData.orientation));
		transform.flip = IMUtil::OIV_AxisAlignedFlip::None;
		return IMUtil::ImageUtil::Transform(transform, image);
	}


    const std::wstring & OIVFileImage::GetFileName() const { return fFileName; }
    
	OIVFileImage::OIVFileImage(const std::wstring& fileName) : OIVBaseImage(ImageSource::File), fFileName(fileName) {}
    
	ResultCode OIVFileImage::Load(IMCodec::ImageLoaderFlags loaderFlags)
	{
		return Load(loaderFlags, IMCodec::ImageLoadFlags::None);
	}
    ResultCode OIVFileImage::Load(IMCodec::ImageLoaderFlags loaderFlags, IMCodec::ImageLoadFlags imageLoadFlags)
    {
        using namespace LLUtils;
        FileMapping fileMapping(fFileName);
        void* buffer = fileMapping.GetBuffer();
        std::size_t size = fileMapping.GetSize();
        std::string extension = LLUtils::StringUtility::ConvertString<std::string>(StringUtility::GetFileExtension(fFileName));


		if (buffer == nullptr || size == 0)
			return RC_InvalidParameters;

		ResultCode result = RC_FileNotSupported;
		using namespace IMCodec;
		ImageSharedPtr image;
		ImageResult loadResult = fImageLoader.Load(static_cast<const std::byte*>(buffer), size, extension.c_str(), imageLoadFlags, loaderFlags, image);

		if (loadResult == ImageResult::Success)
		{
			if (image != nullptr)
			{

				if (image->GetMetaData().exifData.orientation != 0)
				{
					// I see no use of using the original image, discard source image and use the image with exif rotation applied. 
					// If needed, responsibility for exif rotation can be transferred to the user by returning MetaData.exifOrientation.
					image = ApplyExifRotation(image);

				}

				for (size_t i = 0; i < image->GetNumSubImages(); i++)
					image->SetSubImage(i, ApplyExifRotation(image->GetSubImage(i)));

				SetUnderlyingImage(image);
				result = RC_Success;
			}
		}
		return result;
    }
 }
#include <OIVImage/OIVRawImage.h>
#include "../ImageUtil.h"
#include <defs.h>

namespace OIV
{
    ResultCode OIVRawImage::Load(const RawBufferParams& loadParams, const IMUtil::OIV_AxisAlignedTransform& transform)
    {
        using namespace IMCodec;
        ImageItemSharedPtr imageItem = std::make_shared<ImageItem>();
        ImageDescriptor& props = imageItem->descriptor;
        
        imageItem->itemType = ImageItemType::Image;
        props.height = loadParams.height;
        props.width = loadParams.width;
        props.texelFormatStorage = loadParams.texelFormat;
        props.texelFormatDecompressed = loadParams.texelFormat;
        props.rowPitchInBytes = loadParams.rowPitch;
        const size_t bufferSize = props.rowPitchInBytes * props.height;
        imageItem->data.Allocate(bufferSize);
        imageItem->data.Write(loadParams.buffer, 0, bufferSize);

        ImageSharedPtr image = std::make_shared<Image>(imageItem, ImageItemType::Unknown);
        image = IMUtil::ImageUtil::Transform(transform, image);
        SetUnderlyingImage(image);
        
        return RC_Success;


    }
}
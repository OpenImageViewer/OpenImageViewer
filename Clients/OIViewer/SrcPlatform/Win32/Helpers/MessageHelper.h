#include <OIVImage/OIVBaseImage.h>

namespace IMCodec
{
	class ImageCodec;
}

namespace OIV
{
	class MessageHelper
	{
	public:
		static LLUtils::native_string_type CreateImageInfoMessage(const OIVBaseImageSharedPtr& oivImage, const OIVBaseImageSharedPtr& rasterized,  IMCodec::ImageCodec& imageCodec);
		static LLUtils::native_string_type CreateKeyBindingsMessage();
		static LLUtils::native_string_type GetFileTime(const LLUtils::native_string_type& filePath);
	};
}

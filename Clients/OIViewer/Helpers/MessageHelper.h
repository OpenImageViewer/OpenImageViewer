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
		static std::wstring CreateImageInfoMessage(const OIVBaseImageSharedPtr& oivImage, const OIVBaseImageSharedPtr& rasterized,  IMCodec::ImageCodec& imageCodec);
		static std::wstring CreateKeyBindingsMessage();
		static std::wstring ParseImageSource(const OIVBaseImageSharedPtr& image);
		static std::wstring GetFileTime(const std::wstring& filePath);
	};
}
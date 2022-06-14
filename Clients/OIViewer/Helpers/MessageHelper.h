#include <OIVImage/OIVBaseImage.h>
namespace OIV
{
	class MessageHelper
	{
	public:
		static std::wstring CreateImageInfoMessage(const OIVBaseImageSharedPtr& oivImage, const IMCodec::ImageSharedPtr& rasterized);
		static std::wstring CreateKeyBindingsMessage();
		static std::wstring ParseImageSource(const OIVBaseImageSharedPtr& image);
		static std::wstring GetFileTime(const std::wstring& filePath);
	};
}
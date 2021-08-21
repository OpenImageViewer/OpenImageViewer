#include "../OIVImage/OIVBaseImage.h"
namespace OIV
{
	class MessageHelper
	{
	public:
		static std::wstring CreateImageInfoMessage(const OIVBaseImageSharedPtr& image);
		static std::wstring CreateKeyBindingsMessage();
		static std::wstring ParseImageSource(const OIVBaseImageSharedPtr& image);
	};
}
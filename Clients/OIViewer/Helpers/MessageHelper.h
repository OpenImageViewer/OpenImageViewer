#include "../OIVImage/OIVBaseImage.h"
namespace OIV
{
	class MessageHelper
	{
	public:
		static std::string CreateImageInfoMessage(const OIVBaseImageSharedPtr& image);
		static std::string CreateKeyBindingsMessage();
		static std::string ParseImageSource(const OIVBaseImageSharedPtr& image);
	};
}
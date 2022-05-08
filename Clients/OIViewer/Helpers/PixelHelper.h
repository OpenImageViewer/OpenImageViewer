#include <cstdint>
#include <OIVImage/OIVBaseImage.h>
namespace OIV
{
	class PixelHelper
	{
    public:
        static int64_t CountUniqueValues(const IMCodec::ImageSharedPtr& image);
		
		template <typename underlying_type>
		static int64_t GetUniqueColors(const IMCodec::ImageSharedPtr& image, IMCodec::ChannelWidth bpp);
	};
}
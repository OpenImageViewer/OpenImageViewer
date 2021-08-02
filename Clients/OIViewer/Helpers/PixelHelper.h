#include <cstdint>
#include "../OIVImage/OIVBaseImage.h"
namespace OIV
{
	class PixelHelper
	{
    public:
        static int64_t CountUniqueValues(const OIVBaseImageSharedPtr& image);
		
		template <typename underlying_type>
		static int64_t GetUniqueColors(const OIV_CMD_GetPixels_Response& pixelData, uint8_t bpp);
	};
}
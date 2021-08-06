#include "PixelHelper.h"
#include "../OIVCommands.h"
#include <unordered_set>
#include <xxh3.h>

namespace OIV
{
#pragma pack(push,1)
    /// <summary>
    /// ValueComparer is an arbirtrary size comparer.
    /// </summary>
    template <size_t size>
    struct ValueComparer
    {
        std::byte x[size];
        bool operator==(const ValueComparer&) const = default;
    };
#pragma pack(pop)
}

namespace std 
{
    /// <summary>
    /// Tempalte specialization for hashing ValueComparer
    /// </summary>
    template <size_t size> struct hash<OIV::ValueComparer<size>>
    {
        size_t operator()(const OIV::ValueComparer<size>& x) const
        {
            return XXH3_64bits(&x, sizeof(x));
        }
    };
}

namespace OIV
{
    namespace
    {
        /// <summary>
        /// Indirect value comparer, stores the address of the value and its size, a total of 72 bits.
        /// Rarely used to save memory when there is texel size which is more than 72 bits.
        /// </summary>
        class ValueIndirectComparer
        {

        public:
            struct Hasher
            {
                size_t operator()(const ValueIndirectComparer& obj) const
                {
                    return obj.hash();
                }
            };

            ValueIndirectComparer(const void* address, uint8_t size) :
                fAddress(address), fSize(size)
            {

            }

            size_t hash() const
            {
                return XXH3_64bits(fAddress, fSize);
            }

            bool operator == (const ValueIndirectComparer& obj) const
            {
                return memcmp(fAddress, obj.fAddress, fSize) == 0;
            }

        private:
            const void* fAddress;
            uint8_t fSize;
        };
    }

    template <typename underlying_type>
    int64_t PixelHelper::GetUniqueColors(const OIV_CMD_GetPixels_Response& pixelData, uint8_t bpp)
    {
        std::unordered_set<underlying_type> valueSet(pixelData.height * pixelData.width);
        
        OIV_Util_GetBPPFromTexelFormat(pixelData.texelFormat, &bpp);
        const uint8_t* baseAddress = reinterpret_cast<const uint8_t*>(pixelData.pixelBuffer);
        for (size_t y = 0; y < pixelData.height; y++)
        {
            size_t lineOffset = pixelData.rowPitch * y;
            for (size_t x = 0; x < pixelData.width; x++)
            {
                const uint8_t* currentValue = baseAddress + lineOffset + (x * bpp / CHAR_BIT);
                valueSet.insert(*reinterpret_cast<const underlying_type*>(currentValue));
            }
        }

        return valueSet.size();
    }


    int64_t PixelHelper::CountUniqueValues(const OIVBaseImageSharedPtr& image)
    {
        int64_t numUniqueValues = -1;

        OIV_CMD_GetPixels_Request pixelsRequest;
        OIV_CMD_GetPixels_Response pixelsResponse;


        pixelsRequest.handle = image->GetDescriptor().ImageHandle;
        ResultCode result = OIVCommands::ExecuteCommand(CommandExecute::OIV_CMD_GetPixels, &pixelsRequest, &pixelsResponse);

        if (result == ResultCode::RC_Success)
        {
            uint8_t bpp;
            OIV_Util_GetBPPFromTexelFormat(pixelsResponse.texelFormat, &bpp);
            if (bpp % 8 == 0) // if bpp is multiples of 8
            {
                if (OIVCommands::ExecuteCommand(OIV_CMD_GetPixels, &pixelsRequest, &pixelsResponse) == RC_Success)
                {
                    switch (bpp)
                    {
                    case 8:
                        numUniqueValues = GetUniqueColors<ValueComparer<8 / CHAR_BIT>>(pixelsResponse, bpp);
                        break;
                    case 16:
                        numUniqueValues = GetUniqueColors<ValueComparer<16 / CHAR_BIT>>(pixelsResponse, bpp);
                        break;
                    case 24:
                        numUniqueValues = GetUniqueColors<ValueComparer<24 / CHAR_BIT>>(pixelsResponse, bpp);
                        break;
                    case 32:
                        numUniqueValues = GetUniqueColors<ValueComparer<32 / CHAR_BIT>>(pixelsResponse, bpp);
                        break;
                    case 48:
                        numUniqueValues = GetUniqueColors<ValueComparer<48 / CHAR_BIT>>(pixelsResponse, bpp);
                        break;
                    case 64:
                        numUniqueValues = GetUniqueColors<ValueComparer<64 / CHAR_BIT>>(pixelsResponse, bpp);
                        break;
                    case 72:
                        numUniqueValues = GetUniqueColors<ValueComparer<72 / CHAR_BIT>>(pixelsResponse, bpp);
                        break;
                    default:
                    {
                        //Slower way to count unique values by using additional indirection to access arbitrary memory size
                        using SetValues = std::unordered_set<ValueIndirectComparer, ValueIndirectComparer::Hasher>;
                        SetValues setValues(pixelsResponse.height * pixelsResponse.width);

                        const uint8_t* baseAddress = reinterpret_cast<const uint8_t*>(pixelsResponse.pixelBuffer);

                        for (size_t y = 0; y < pixelsResponse.height; y++)
                        {
                            size_t lineOffset = pixelsResponse.rowPitch * y;
                            for (size_t x = 0; x < pixelsResponse.width; x++)
                            {
                                const uint8_t* currentValue = baseAddress + lineOffset + (x * bpp / CHAR_BIT);
                                setValues.insert(ValueIndirectComparer(currentValue, bpp / CHAR_BIT));
                            }
                        }
                        numUniqueValues = setValues.size();
                    }
                    }
                }
                else
                {
                    LL_EXCEPTION_NOT_IMPLEMENT("unsupported bit width, currently only 8 bit and higher image are supported");
                }
            }
        }
        return numUniqueValues;
    }
}
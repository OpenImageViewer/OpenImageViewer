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
    int64_t PixelHelper::GetUniqueColors(const IMCodec::ImageSharedPtr& image, IMCodec::ChannelWidth bpp)
    {
        std::unordered_set<underlying_type> valueSet(image->GetTotalPixels());

        const uint8_t* baseAddress = reinterpret_cast<const uint8_t*>(image->GetBuffer());
        for (size_t y = 0; y < image->GetHeight(); y++)
        {
            size_t lineOffset = image->GetRowPitchInBytes() * y;
            for (size_t x = 0; x < image->GetWidth(); x++)
            {
                const uint8_t* currentValue = baseAddress + lineOffset + (x * bpp / CHAR_BIT);
                valueSet.insert(*reinterpret_cast<const underlying_type*>(currentValue));
            }
        }

        return valueSet.size();
    }


    int64_t PixelHelper::CountUniqueValues(const IMCodec::ImageSharedPtr& image)
    {
        int64_t numUniqueValues = -1;

        const IMCodec::ChannelWidth bpp = image->GetBitsPerTexel();

        if (bpp % 8 == 0) // if bpp is multiples of 8
        {
            switch (bpp)
            {
            case 8:
                numUniqueValues = GetUniqueColors<ValueComparer<8 / CHAR_BIT>>(image, bpp);
                break;
            case 16:
                numUniqueValues = GetUniqueColors<ValueComparer<16 / CHAR_BIT>>(image, bpp);
                break;
            case 24:
                numUniqueValues = GetUniqueColors<ValueComparer<24 / CHAR_BIT>>(image, bpp);
                break;
            case 32:
                numUniqueValues = GetUniqueColors<ValueComparer<32 / CHAR_BIT>>(image, bpp);
                break;
            case 48:
                numUniqueValues = GetUniqueColors<ValueComparer<48 / CHAR_BIT>>(image, bpp);
                break;
            case 64:
                numUniqueValues = GetUniqueColors<ValueComparer<64 / CHAR_BIT>>(image, bpp);
                break;
            case 72:
                numUniqueValues = GetUniqueColors<ValueComparer<72 / CHAR_BIT>>(image, bpp);
                break;
            default:
            {
                //Slower way to count unique values by using additional indirection to access arbitrary memory size
                using SetValues = std::unordered_set<ValueIndirectComparer, ValueIndirectComparer::Hasher>;
                SetValues setValues(image->GetTotalPixels());

                const uint8_t* baseAddress = reinterpret_cast<const uint8_t*>(image->GetBuffer());

                for (size_t y = 0; y < image->GetHeight(); y++)
                {
                    size_t lineOffset = image->GetRowPitchInBytes() * y;
                    for (size_t x = 0; x < image->GetWidth(); x++)
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
            LL_ERROR(LLUtils::Exception::ErrorCode::NotImplemented, "unsupported bit width, currently only 8 bit and higher image are supported");
        }

        return numUniqueValues;
    }
}
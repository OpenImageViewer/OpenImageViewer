#pragma once

#include <type_traits>
#include <map>
#include <functional>
#include <LLUtils/Platform.h>
#include <TexelFormat.h>

namespace OIV
{
#pragma pack(push,1)
    struct uint24_t
    {
        uint8_t val[3];
    };

    struct uint48_t
    {
        uint8_t val[6];
    };

#pragma pack(pop)

    template <size_t var_width>
    struct VariableWidth
    {

    };

    template <>
    struct VariableWidth<8>
    {
        using var = uint8_t;

    };

    template <>
    struct VariableWidth<16>
    {
        using var = uint16_t;

    };


    template <>
    struct VariableWidth<24>
    {
        using var = uint24_t;

    };


    template <>
    struct VariableWidth<32>
    {
        using var = uint32_t;

    };

    template <>
    struct VariableWidth<48>
    {
        using var = uint48_t;

    };

    template <>
    struct VariableWidth<64>
    {
        using var = uint64_t;

    };

    template <uint8_t dest_index, typename channel_dest_type, typename channel_source_type, uint8_t source_index_head, uint8_t... source_index_tail>
    constexpr LLUTILS_FORCE_INLINE void _Swizzle(std::byte* i_dest, const std::byte* i_src)
    {
        //if diff is positive destination is wider than source

        constexpr ptrdiff_t diff = sizeof(channel_dest_type) - sizeof(channel_source_type);

        reinterpret_cast<channel_dest_type*>(i_dest)[dest_index] =
            ((static_cast<channel_dest_type>((reinterpret_cast<const channel_source_type*>(i_src)[source_index_head])
                >> std::max< ptrdiff_t>(0, -diff * CHAR_BIT))) << std::max<ptrdiff_t>(0, diff * CHAR_BIT));

        if constexpr (sizeof...(source_index_tail) == 0)
        {
            reinterpret_cast<channel_dest_type*>(i_dest)[dest_index] =
                ((static_cast<channel_dest_type>((reinterpret_cast<const channel_source_type*>(i_src)[source_index_head])
                    >> std::max<ptrdiff_t>(0, -diff * CHAR_BIT))) << std::max<ptrdiff_t>(0, diff * CHAR_BIT));
        }
        else
        {
            _Swizzle<dest_index + 1, channel_dest_type, channel_source_type, source_index_tail...>(i_dest, i_src);
        }
    }

    template <uint8_t bit_width_dest, uint8_t bit_width_source, uint8_t source_index_head, uint8_t... source_index_tail>
    LLUTILS_FORCE_INLINE  void Swizzle(std::byte* i_dest, const std::byte* i_src)
    {
        using channel_dest_type = typename VariableWidth<bit_width_dest>::var;
        using channel_source_type = typename VariableWidth<bit_width_source>::var;
        _Swizzle<0, channel_dest_type, channel_source_type, source_index_head, source_index_tail...>(i_dest, i_src);
    }

    template <uint8_t bit_width_dest, uint8_t bit_width_source, uint8_t append_index, auto append_value, uint8_t source_index_head, uint8_t... source_index_tail>
    LLUTILS_FORCE_INLINE  void SwizzleAppend(std::byte* i_dest, const std::byte* i_src)
    {
        using channel_dest_type = typename VariableWidth<bit_width_dest>::var;
        using channel_source_type = typename VariableWidth<bit_width_source>::var;

        _Swizzle<0, channel_dest_type, channel_source_type, source_index_head, source_index_tail...>(i_dest, i_src);
        reinterpret_cast<channel_dest_type*>(i_dest)[append_index] = append_value;
    }

    struct ConvertKey
    {

        struct Hash
        {
            constexpr std::size_t operator()(const ConvertKey& convertKey) const
            {
                return 12;
                return convertKey.key();
            }
        };

        constexpr ConvertKey(IMCodec::TexelFormat aSource, IMCodec::TexelFormat aTarget)
        {
            source = aSource;
            target = aTarget;
        }

        constexpr size_t key() const
        {
            using eumType = std::underlying_type<IMCodec::TexelFormat>::type;
            static_assert(sizeof(eumType) <= 2, "Size of enum underlying type should not be more than two bytes");
            return static_cast<eumType>(source) << (sizeof(eumType) * 8) | static_cast<eumType>(target);
        }

        constexpr bool operator<(const ConvertKey& rhs) const
        {
            return key() < rhs.key();
        }
        

        constexpr bool operator==(const ConvertKey& rhs) const
        {
            return source == rhs.source && target == rhs.target;
        }

        IMCodec::TexelFormat source;
        IMCodec::TexelFormat target;
    };

   

    class TexelConvertor
    {
    public:
    
        bool Convert(IMCodec::TexelFormat targetPixelFormat, IMCodec::TexelFormat sourcePixelFormat,
            std::byte* destBuffer, const std::byte* sourceBuffer, size_t numpixels);
       
    private:
        using SwizzleFuncType = std::function<void(std::byte*, const std::byte*)>;
        using MapConvertKeyToFunc = std::map<ConvertKey, SwizzleFuncType>;

        void  ConvertSegment(std::byte* i_dest, const std::byte* i_src, size_t count, uint8_t sourceSize, uint8_t destSize, SwizzleFuncType swizzle)
        {
            for (size_t i = 0; i < count; i++)
            {
                swizzle(i_dest, i_src);
                i_dest += destSize;
                i_src += sourceSize;
            }
        }

        SwizzleFuncType PopulateSwizzleFunc(IMCodec::TexelFormat sourcePixelFormat, IMCodec::TexelFormat targetPixelFormat);

        SwizzleFuncType FindSwizzleFunc(IMCodec::TexelFormat sourcePixelFormat, IMCodec::TexelFormat targetPixelFormat);

        //TODO: change to constexpr
        MapConvertKeyToFunc fConversionMappings = {
            {ConvertKey(IMCodec::TexelFormat::I_R8_G8_B8, IMCodec::TexelFormat::I_B8_G8_R8), SwizzleFuncType(Swizzle< 8, 8, 2, 1, 0>)}
            ,{ConvertKey(IMCodec::TexelFormat::I_R8_G8_B8, IMCodec::TexelFormat::I_R8_G8_B8_A8), SwizzleFuncType(SwizzleAppend< 8, 8, 3,0xFF, 0, 1, 2>)}
            ,{ConvertKey(IMCodec::TexelFormat::I_R8_G8_B8, IMCodec::TexelFormat::I_B8_G8_R8_A8), SwizzleFuncType(SwizzleAppend< 8, 8, 3,0xFF, 2, 1, 0>)}
            ,{ConvertKey(IMCodec::TexelFormat::I_B8_G8_R8, IMCodec::TexelFormat::I_R8_G8_B8), SwizzleFuncType(Swizzle< 8, 8, 2, 1, 0>)}
            ,{ConvertKey(IMCodec::TexelFormat::I_B8_G8_R8, IMCodec::TexelFormat::I_B8_G8_R8_A8), SwizzleFuncType(SwizzleAppend< 8, 8, 3,0xFF, 0, 1, 2>)}
            ,{ConvertKey(IMCodec::TexelFormat::I_B8_G8_R8, IMCodec::TexelFormat::I_R8_G8_B8_A8), SwizzleFuncType(SwizzleAppend< 8, 8, 3,0xFF, 2, 1, 0>)}
            ,{ConvertKey(IMCodec::TexelFormat::I_B16_G16_R16_A16, IMCodec::TexelFormat::I_B8_G8_R8_A8), SwizzleFuncType(Swizzle<8, 16, 0, 1, 2 ,3>)}
            ,{ConvertKey(IMCodec::TexelFormat::I_R16_G16_B16_A16, IMCodec::TexelFormat::I_R8_G8_B8_A8), SwizzleFuncType(Swizzle<8, 16, 0, 1, 2, 3>)}
            ,{ConvertKey(IMCodec::TexelFormat::I_R16_G16_B16_A16, IMCodec::TexelFormat::I_B8_G8_R8_A8), SwizzleFuncType(Swizzle<8, 16, 2, 1, 0,3>)}
            ,{ConvertKey(IMCodec::TexelFormat::I_R16_G16_B16, IMCodec::TexelFormat::I_B8_G8_R8), SwizzleFuncType(Swizzle<8, 16, 2, 1, 0>)}
            ,{ConvertKey(IMCodec::TexelFormat::I_R16_G16_B16, IMCodec::TexelFormat::I_B8_G8_R8_A8), SwizzleFuncType(SwizzleAppend<8, 16,3,0xFF, 2, 1, 0>)}
            ,{ConvertKey(IMCodec::TexelFormat::I_R16_G16_B16, IMCodec::TexelFormat::I_R8_G8_B8_A8), SwizzleFuncType(SwizzleAppend<8, 16,3,0xFF, 0, 1, 2>)}
            ,{ ConvertKey(IMCodec::TexelFormat::I_B8_G8_R8_A8, IMCodec::TexelFormat::I_R8_G8_B8_A8)   ,SwizzleFuncType(Swizzle<8, 8, 2, 1, 0,3>) }
            ,{ ConvertKey(IMCodec::TexelFormat::I_B8_G8_R8_A8, IMCodec::TexelFormat::I_B8_G8_R8)      ,SwizzleFuncType(Swizzle<8, 8, 0, 1, 2>) }
            ,{ ConvertKey(IMCodec::TexelFormat::I_R8_G8_B8_A8, IMCodec::TexelFormat::I_B8_G8_R8_A8)   ,SwizzleFuncType(Swizzle<8, 8, 2, 1, 0, 3>) }
        };
    };
}

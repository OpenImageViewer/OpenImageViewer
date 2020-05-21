#pragma once
#include <cstdint>
struct BlitBox
{
    std::byte* buffer;
    uint32_t rowPitch;
    uint32_t width;
    uint32_t height;
    uint32_t left;
    uint32_t top;
    uint32_t pixelSizeInbytes;

    int32_t GetStartOffset() const
    {
        return (top * rowPitch) + (left * pixelSizeInbytes);

    }

    static void Blit(BlitBox& dst, const BlitBox& src)
    {
        const std::byte* srcPos = src.buffer + src.GetStartOffset();
        std::byte* dstPos = dst.buffer + dst.GetStartOffset();

        //Perform range check on target.
        if (dst.left + src.width > dst.width ||
            dst.top + src.height > dst.height)
        {
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::LogicError, "Target Out of bounds ");
        }


        const uint32_t bytesPerLine = src.pixelSizeInbytes * src.width;

        for (uint32_t y = src.top; y < src.height; y++)
        {
            for (uint32_t x = 0; x < bytesPerLine; x += 4)
            {
                using namespace LLUtils;
                Color srcColor(*reinterpret_cast<const uint32_t*>(srcPos + x));
                Color dstColor(*reinterpret_cast<const uint32_t*>(dstPos + x));
                *reinterpret_cast<uint32_t*>(dstPos + x) = dstColor.Blend(srcColor).colorValue;
            }
            dstPos += dst.rowPitch;
            srcPos += src.rowPitch;
        }
    }
};
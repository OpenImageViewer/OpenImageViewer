#pragma once
#include <algorithm>
namespace LLUtils
{
    struct Color
    {
        union
        {
            struct
            {
                uint8_t R, G, B, A;
            };
            uint32_t colorValue;
        };

        Color() {}
        Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        {
            R = r;
            B = b;
            G = g;
            A = a;
        }
        Color(uint32_t color)
        {
            colorValue = color;
        }

        Color Blend(const Color& source)
        {
            Color blended;
            uint8_t invSourceAlpha = 0xFF - source.A;
            blended.R = (source.A * source.R + invSourceAlpha * R) / 0xFF;
            blended.G = (source.A * source.G + invSourceAlpha * G) / 0xFF;
            blended.B = (source.A * source.B + invSourceAlpha * B) / 0xFF;
            blended.A = std::max(A, source.A);
            return blended;
        }
    };
}
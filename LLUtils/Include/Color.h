#pragma once
#include <algorithm>
#include <string>
#include <math.h>

inline constexpr uint8_t operator "" _u8(unsigned long long int arg) noexcept
{
    return static_cast<uint8_t>(arg);
}

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
        template <class T>
        const T* GetNormalizedColorValue() const
        {
            static_assert(std::is_floating_point<T>(), "Only floating point support normilization");
            static thread_local T normalizedColor[4];
            normalizedColor[0] = R / static_cast<T>(255.0);
            normalizedColor[1] = G / static_cast<T>(255.0);
            normalizedColor[2] = B / static_cast<T>(255.0);
            normalizedColor[3] = A / static_cast<T>(255.0);
            return normalizedColor;
        }
        Color() = default;
		
        Color (double r, double g, double b, double a = 1.0)
        {
            R = static_cast<uint8_t>(std::round(r * 255.0));
            G = static_cast<uint8_t>(std::round(g * 255.0));
			B = static_cast<uint8_t>(std::round(b * 255.0));
            A = static_cast<uint8_t>(std::round(a * 255.0));
        }
        Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        {
            R = r;
            G = g;
            B = b;
            A = a;
        }
        Color(uint32_t color)
        {
            colorValue = color;
        }

		bool operator ==(const Color rhs) const
		{
			return this->colorValue == rhs.colorValue;
		}

		bool operator !=(const Color rhs) const
		{
			return this->colorValue != rhs.colorValue;
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
        static Color FromString(const std::string& str)
        {
            using namespace std;;
            Color c = static_cast<uint32_t>(0xFF) << 24;
            char strByteColor[3];
            strByteColor[2] = 0;
            if (str.length() > 0)
            {
                if (str[0] == '#')
                {
                    string hexValues = str.substr(1);
                    if (hexValues.length() > 8)
                        hexValues.erase(hexValues.length());

                    size_t length = hexValues.length();
                    int i = 0;
                    while (i < length)
                    {
                        if (length - i >= 2)
                        {
                            strByteColor[0] = hexValues[i];
                            strByteColor[1] = hexValues[i + 1];
                            uint8_t val = static_cast<uint8_t>(std::strtoul(strByteColor, nullptr, 16));
                            *(reinterpret_cast<uint8_t*>(&c.colorValue) + i / 2) = val;
                            i += 2;
                        }
                        else if (length - i == 1)
                        {
                            strByteColor[0] = '0';
                            strByteColor[1] = hexValues[i];
                            uint8_t val = static_cast<uint8_t>(std::strtoul(strByteColor, nullptr, 16));
                            *(reinterpret_cast<uint8_t*>(&c.colorValue) + i / 2) = val;
                            i += 2;
                        }

                    }

                }

            }
            return c;
        }
    };
}
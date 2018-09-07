#pragma once
#pragma pack(push,1)
class Float24
{
public:
    struct
    {
        operator int() const { return upperByte << 16 | lowerbytes; }
        uint16_t lowerbytes;
        uint8_t upperByte;
    } data;

    operator float() const { return fromFP24(static_cast<const int>(data)); }

    float fromFP24(int data) const {
        int chr = (data & 0x7f0000) >> 16;
        int temp = (data & 0x800000) << 8;

        if (chr == 0) {
            int mant = data & 0xffff;
            if (mant != 0) {
                int exp = -62;
                while ((mant & 0x8000) == 0) {
                    mant <<= 1;
                    exp -= 1;
                }
                mant <<= 1;
                exp -= 1;
                temp |= (mant & 0xffff) << 7 | ((exp + 127) << 23);
            }
        }
        else {
            temp |= (data & 0xffff) << 7 | ((chr - 63 + 127) << 23);
        }
        float *f = (float *)&temp;
        return *f;
    }
};
#pragma pack(pop)

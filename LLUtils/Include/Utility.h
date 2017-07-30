#pragma once
#include <list>
#include "StringUtility.h"
#include <filesystem>
#include <immintrin.h> // AVX2

namespace LLUtils
{

    class Utility
    {
    public:
        template <class T>
        static T Align(T num ,T alignement)
        {
            static_assert(std::is_integral<T>(),"Alignment works only with integrals");
            if (alignement < 1)
                throw std::logic_error("alignement must be a positive value");
            return (num + (alignement - 1)) / alignement * alignement;
        }
        

        template <typename T>
        static int8_t Sign(T val)
        {
            return (T(0) < val) - (val < T(0));
        }

        static bool EnsureDirectory(const string_type& path)
        {
            if (std::experimental::filesystem::exists(path) == false)
                return std::experimental::filesystem::create_directory(path);

            return std::experimental::filesystem::is_directory(path);
        }

        static int CopyMemSSE4(void* piDst, void* piSrc, unsigned long SizeInBytes)
        {
            const int RegisterSizeinBytes = 256 / 8;
            const int totalRegisters = 4;
            const int offset = RegisterSizeinBytes * totalRegisters;

            size_t bytesCopyBulk = SizeInBytes / offset * offset;
            size_t bytesResidue = SizeInBytes - bytesCopyBulk;
            __m256i data[totalRegisters];


            uint8_t* src = (uint8_t*)piSrc;
            uint8_t* dst = (uint8_t*)piDst;

            while (bytesCopyBulk > 0)
            {
                /*  for (int i = 0; i < totalRegisters; i++)
                  {
                      data[i] = _mm256_loadu_si256((__m256i const *) (src + i * RegisterSizeinBytes));
                  }

                  for (int i = 0; i < totalRegisters; i++)
                  {
                      _mm256_storeu_si256((__m256i *)(dst + i * RegisterSizeinBytes), data[i]);
                  }*/

                data[0] = _mm256_loadu_si256((__m256i const *)src + 0);
                data[1] = _mm256_loadu_si256((__m256i const *)(src + 32));
                data[2] = _mm256_loadu_si256((__m256i const *)(src + 64));
                data[3] = _mm256_loadu_si256((__m256i const *)(src + 96));
                /*data[4] = _mm256_loadu_si256((__m256i const *)src + 128 );
                data[5] = _mm256_loadu_si256((__m256i const *)(src + 160));
                data[6] = _mm256_loadu_si256((__m256i const *)(src + 192));
                data[7] = _mm256_loadu_si256((__m256i const *)(src + 224));*/


                _mm256_storeu_si256((__m256i *)(dst + 0), data[0]);
                _mm256_storeu_si256((__m256i *)(dst + 32), data[1]);
                _mm256_storeu_si256((__m256i *)(dst + 64), data[2]);
                _mm256_storeu_si256((__m256i *)(dst + 96), data[3]);
                /*_mm256_storeu_si256((__m256i *)(dst + 128), data[4]);
                _mm256_storeu_si256((__m256i *)(dst + 160), data[5]);
                _mm256_storeu_si256((__m256i *)(dst + 192), data[6]);
                _mm256_storeu_si256((__m256i *)(dst + 224), data[7]);*/

                bytesCopyBulk -= 128;
                src += 128;
                dst += 128;
            }

            memcpy(dst, src, bytesResidue);

            return 0;
        }

    };
}
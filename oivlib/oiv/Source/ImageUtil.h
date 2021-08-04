#pragma once

#include <unordered_map>
#include <map>
#include <Image.h>
#include "PixelUtil.h"
#include "TexelConvertor.h"
#include <LLUtils/Rect.h>
#include <LLUtils/Color.h>
#include <ExoticNumbers/half.hpp>
#include <LLUtils/Exception.h>
#include <ExoticNumbers/Float24.h>
#include <algorithm>
#include <execution>
#include <span>


namespace IMUtil
{



    constexpr uint32_t RGBA(uint8_t R, uint8_t G, uint8_t B, uint8_t A)
    {
        return (static_cast<uint32_t>(A) << 24) | (static_cast<uint32_t>(B) << 16) | (static_cast<uint32_t>(G) << 8) | static_cast<uint32_t>(R);
    }

    constexpr uint32_t RGBA_GRAYSCALE(uint8_t Gray)
    {
        return RGBA(Gray, Gray, Gray, 255);
    }

    constexpr uint32_t RGBA_To_GRAYSCALE_LUMA(uint8_t R, uint8_t G, uint8_t B, uint8_t A)
    {
        return RGBA(static_cast<uint32_t>(R * 0.2126), static_cast<uint32_t>(G * 0.7152), static_cast<uint32_t>(B * 0.0722), A);
    }


    class ImageUtil
    {
    private:

        static inline OIV::TexelConvertor texelConvertor;
        enum class NormalizeMode
        {
              Default = 0
            , GrayScale = 1
            , RainBow = 2
        };

        struct ConvertKey
        {

            struct Hash
            {
                std::size_t operator()(const ConvertKey& key) const
                {
                    using eumType = std::underlying_type<IMCodec::TexelFormat>::type;
                    static_assert(sizeof(eumType) <= 2, "Size of enum underlying type should not be more than two bytes");
                    return static_cast<eumType>(key.source) << (sizeof(eumType) * 8) | static_cast<eumType>(key.target);
                }
            };

            ConvertKey(IMCodec::TexelFormat aSource, IMCodec::TexelFormat aTarget)
            {
                source = aSource;
                target = aTarget;
            }

            bool operator==(const ConvertKey& rhs) const
            {
                return source == rhs.source && target == rhs.target;
            }

            IMCodec::TexelFormat source;
            IMCodec::TexelFormat target;
        };

        typedef std::unordered_map<ConvertKey, PixelConvertFunc, ConvertKey::Hash> MapConvertKeyToFunc;
        static MapConvertKeyToFunc sConvertionFunction;

    public:

        static IMCodec::ImageSharedPtr Transform(OIV_AxisAlignedTransform transform, IMCodec::ImageSharedPtr image)
        {
            using namespace std;
            using namespace IMCodec;

            if (image->GetIsByteAligned() == false)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::LogicError, "OIV::Image::Transom works only with byte aligned image formats.");

            if (transform.rotation != OIV_AxisAlignedRotation::None || transform.flip != OIV_AxisAlignedFlip::None)
            {
                ImageDescriptor transformedProperties;
                transformedProperties.fProperties = image->GetDescriptor().fProperties;
                transformedProperties.fData.Allocate(image->GetTotalSizeOfImageTexels());
                std::byte* dest = transformedProperties.fData.data();

                const size_t MegaBytesPerThread = 6;
                static const uint8_t MaxGlobalThrads = 32;
                static uint8_t maxThreads = static_cast<uint8_t>(
                    min(static_cast<unsigned int>(MaxGlobalThrads), max(1u, thread::hardware_concurrency() - 1)));
                static thread threads[MaxGlobalThrads];


                const size_t bytesPerThread = MegaBytesPerThread * 1024 * 1024;
                const uint8_t totalThreads = (std::min)(maxThreads, static_cast<uint8_t>(image->GetTotalSizeOfImageTexels() / bytesPerThread));
                PixelUtil::TransformTexelsInfo descTemplate;
                descTemplate.resolvedTransformData = PixelUtil::ReolvedAxisAlignedTransformData(transform);
                descTemplate.dstBuffer = dest;
                descTemplate.srcBuffer = image->GetBuffer();
                descTemplate.width = image->GetWidth();
                descTemplate.height = image->GetHeight();
                descTemplate.bytesPerTexel = image->GetBytesPerTexel();
                descTemplate.srcRowPitch = image->GetRowPitchInBytes();
                descTemplate.startCol = 0;
                descTemplate.endCol = image->GetWidth();
                descTemplate.startRow = 0;
                descTemplate.endRow = image->GetHeight();

                if (totalThreads > 0)
                {
                    size_t rowsPerThread = image->GetHeight() / totalThreads;
                    for (uint8_t threadNum = 0; threadNum < totalThreads; threadNum++)
                    {

                        threads[threadNum] = std::thread
                        (
                            [&descTemplate, rowsPerThread, threadNum]()
                            {
                                PixelUtil::TransformTexelsInfo desc = descTemplate;
                                desc.startRow = rowsPerThread * threadNum;
                                desc.endRow = rowsPerThread * (threadNum + 1);
                                PixelUtil::TransformTexels(desc);
                            }
                        );
                    }

                    PixelUtil::TransformTexelsInfo desc = descTemplate;
                    desc.startRow = rowsPerThread * totalThreads;
                    desc.endRow = image->GetHeight();

                    PixelUtil::TransformTexels(desc);

                    for (uint8_t i = 0; i < totalThreads; i++)
                        threads[i].join();
                }
                else
                {
                    // single (main) thread implementation
                    PixelUtil::TransformTexels(descTemplate);
                }



                if (transform.rotation == OIV_AxisAlignedRotation::Rotate90CW || transform.rotation == OIV_AxisAlignedRotation::Rotate90CCW)
                    swap(transformedProperties.fProperties.Height, transformedProperties.fProperties.Width);

                transformedProperties.fProperties.RowPitchInBytes = transformedProperties.fProperties.Width * image->GetBytesPerTexel();

                return ImageSharedPtr(new Image(transformedProperties));
            }
            else
                return image;

        }

        static IMCodec::ImageSharedPtr ConvertImageWithNormalization(IMCodec::ImageSharedPtr image, IMCodec::TexelFormat targetTexelFormat, bool isRainbow)
        {

            const NormalizeMode normalizeMode = isRainbow ? NormalizeMode::RainBow : NormalizeMode::GrayScale;

            switch (image->GetImageType())
            {
            case IMCodec::TexelFormat::F_X16:
                image = Normalize<half_float::half>(image, normalizeMode);
                break;

            case IMCodec::TexelFormat::F_X24:
                image = Normalize<Float24>(image, normalizeMode);
                break;

            case IMCodec::TexelFormat::F_X32:
                image = ImageUtil::Normalize<float>(image, normalizeMode);
                break;

            case IMCodec::TexelFormat::I_X8:
                image = ImageUtil::Normalize<uint8_t>(image, normalizeMode);
                break;

            case IMCodec::TexelFormat::S_X16:
                image = ImageUtil::Normalize<int16_t>(image, normalizeMode);
                break;

            default:
                image = ImageUtil::Convert(image, targetTexelFormat);
            }

            return image;

        }


        static IMCodec::ImageSharedPtr NormalizePitch(IMCodec::ImageSharedPtr image)
        {
            using namespace IMCodec;

            if (image->GetIsByteAligned() == false)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::LogicError, "can not normalize a non byte aligned pixel format.");

            ImageDescriptor normalizedImageProperties;

            if (image->GetIsRowPitchNormalized() == true)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::LogicError, "image already normalized.");


            const std::byte* oldBuffer = image->GetBuffer();
            normalizedImageProperties.fProperties = image->GetDescriptor().fProperties;
            normalizedImageProperties.fData.Allocate(image->GetTotalSizeOfImageTexels());
            uint32_t targetRowPitch = image->GetBytesPerRowOfPixels();
            std::byte* newBuffer = normalizedImageProperties.fData.data();

            for (std::size_t y = 0; y < image->GetHeight(); y++)
                for (std::size_t x = 0; x < targetRowPitch; x++)
                {
                    std::size_t srcIndex = y * image->GetRowPitchInBytes() + x;
                    std::size_t dstIndex = y * targetRowPitch + x;

                    newBuffer[dstIndex] = oldBuffer[srcIndex];
                }

            normalizedImageProperties.fProperties.RowPitchInBytes = targetRowPitch;


            return ImageSharedPtr(new Image(normalizedImageProperties));
        }

        static IMCodec::ImageSharedPtr Convert(IMCodec::ImageSharedPtr sourceImage, IMCodec::TexelFormat targetPixelFormat)
        {
            using namespace IMCodec;

            ImageSharedPtr convertedImage;

            if (sourceImage->GetImageType() != targetPixelFormat)
            {
                ImageDescriptor imagedescriptor;
                imagedescriptor.fProperties = sourceImage->GetDescriptor().fProperties;

                uint8_t targetPixelSize = GetTexelFormatSize(targetPixelFormat);
                imagedescriptor.fData.Allocate(sourceImage->GetTotalPixels() * targetPixelSize);

                std::byte* dest = imagedescriptor.fData.data();
                //TODO: convert without normalization.
                ImageSharedPtr normalizedImage = sourceImage->GetIsRowPitchNormalized() == true ? sourceImage : NormalizePitch(sourceImage);
                
                imagedescriptor.fProperties.TexelFormatDecompressed = targetPixelFormat;
                imagedescriptor.fProperties.RowPitchInBytes = normalizedImage->GetRowPitchInTexels() * targetPixelSize / CHAR_BIT;


                bool succcess = false;

                if ((succcess = 
                    texelConvertor.Convert(targetPixelFormat
                        , sourceImage->GetDescriptor().fProperties.TexelFormatDecompressed
                        , dest
                        , normalizedImage->GetBuffer()
                        , normalizedImage->GetTotalPixels())) == false)
                {

                   auto converter = sConvertionFunction.find(ConvertKey(sourceImage->GetImageType(), targetPixelFormat));
                   
                   if (converter != sConvertionFunction.end())
                   {

                           PixelUtil::Convert(converter->second
                               , &dest
                               , normalizedImage->GetBuffer()
                               , targetPixelSize
                               , normalizedImage->GetTotalPixels());
                           
                           succcess = true;
                   }
                }

                if (succcess == true)
                    convertedImage = std::make_shared<Image>(imagedescriptor);
               
            }
            else
            { // No need to convert, return source image.
                convertedImage = sourceImage;

            }
            return convertedImage;
        }

        static IMCodec::ImageSharedPtr GetSubImage(IMCodec::ImageSharedPtr sourceImage, LLUtils::RectI32 subimage)
        {
            LLUtils::RectI32 image = { { 0,0 } ,{ static_cast<int32_t> (sourceImage->GetWidth())
                , static_cast<int32_t> (sourceImage->GetHeight()) } };
            if (subimage.IsNonNegative() && subimage.IsInside(image))
            {
                using namespace IMCodec;
                const std::byte* sourceBuffer = sourceImage->GetBuffer();


                const uint32_t destBufferSize = subimage.GetWidth() * subimage.GetHeight() * sourceImage->GetBytesPerTexel();
                ImageDescriptor props;
                props.fProperties = sourceImage->GetDescriptor().fProperties;
                props.fData.Allocate(destBufferSize);
                std::byte* destBuffer = props.fData.data();

                for (int32_t y = 0; y < subimage.GetHeight(); y++)
                {
                    for (int32_t x = 0; x < subimage.GetWidth(); x++)
                    {
                        decltype(subimage)::Point_Type topLeft = subimage.GetCorner(LLUtils::Corner::TopLeft);

                        const uint32_t idxDest = y * subimage.GetWidth() + x;
                        const uint32_t idxSource = (y + topLeft.y) * sourceImage->GetWidth() + (x + topLeft.x);
                        PixelUtil::CopyTexel(destBuffer, idxDest, sourceBuffer, idxSource, sourceImage->GetBytesPerTexel());
                    }
                }


                props.fProperties.Height = subimage.GetHeight();
                props.fProperties.Width = subimage.GetWidth();

                props.fProperties.RowPitchInBytes = subimage.GetWidth() * sourceImage->GetBytesPerTexel();

                ImageSharedPtr subImagePtr = ImageSharedPtr(new Image(props));

                return subImagePtr;
            }
            return IMCodec::ImageSharedPtr();
        }




        template <typename _FwdItDst, typename _FwdItSrc >
        static void NormalizeAnyToBGRA(_FwdItDst _DstFirst, _FwdItDst _DstLast, _FwdItSrc _SrcFirst, _FwdItSrc _SrcLast, NormalizeMode mode)
        {
            auto minMax = std::minmax_element(std::execution::parallel_unsequenced_policy(), _SrcFirst, _SrcLast);
            const auto range = *minMax.second - *minMax.first;

            while (_SrcFirst != _SrcLast)
            {
                const auto& sourceSample = *_SrcFirst;
                auto& destSample = _DstFirst->value;

                switch (mode)
                {
                case NormalizeMode::Default:
                case NormalizeMode::GrayScale:
                {
                    uint8_t grayValue = std::min(static_cast<uint8_t>(static_cast<double>(sourceSample - *minMax.first) / range * 255.0 + 0.5), static_cast<uint8_t>(255));
                    destSample = RGBA_GRAYSCALE(grayValue);
                }
                break;

                case NormalizeMode::RainBow:
                {

                    //Red (0) is the minimum , Magenta is the maximum (300)
                    const uint16_t hue = static_cast<uint16_t>(static_cast<double>(sourceSample - *minMax.first) / range * 300.0);
                    destSample = LLUtils::Color::FromHSL(hue, 0.5, 0.5).colorValue;
                }
                break;
                }

                _SrcFirst++;
                _DstFirst++;
            }
            
        }

        template <class SourceSampleType>
        static IMCodec::ImageSharedPtr Normalize(IMCodec::ImageSharedPtr sourceImage, NormalizeMode normalizeMode)
        {
            const SourceSampleType* sampleData = reinterpret_cast<const SourceSampleType*> (sourceImage->GetBuffer());
            const uint32_t totalPixels = sourceImage->GetTotalPixels();

            IMCodec::ImageDescriptor props;
            props.fProperties = sourceImage->GetDescriptor().fProperties;
            props.fProperties.TexelFormatDecompressed = IMCodec::TexelFormat::I_B8_G8_R8_A8;
            props.fProperties.RowPitchInBytes = IMCodec::GetTexelFormatSize(props.fProperties.TexelFormatDecompressed) / CHAR_BIT * props.fProperties.Width;
            props.fData.Allocate(props.fProperties.RowPitchInBytes * props.fProperties.Height);

            std::span sourceData(reinterpret_cast<const SourceSampleType*>(sampleData), totalPixels);
            std::span bgraData(reinterpret_cast<PixelUtil::BitTexel32Ex*>(props.fData.data()), totalPixels);

            NormalizeAnyToBGRA(std::begin(bgraData), std::end(bgraData), std::begin(sourceData), std::end(sourceData), normalizeMode);

            return IMCodec::ImageSharedPtr(new IMCodec::Image(props));
        }
    };
}

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
#include "PixelUtil.h"

namespace IMUtil
{

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


            if (transform.rotation != OIV_AxisAlignedRotation::None || transform.flip != OIV_AxisAlignedFlip::None)
            {
                if (image->GetIsByteAligned() == false)
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::LogicError, "OIV::Image::Transom works only with byte aligned image formats.");

                ImageItemSharedPtr imageItem = std::make_shared<ImageItem>();
                imageItem->descriptor = image->GetDescriptor();
                imageItem->data.Allocate(image->GetTotalSizeOfImageTexels());
                imageItem->itemType = image->GetItemType();
                std::byte* dest = imageItem->data.data();

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
                    swap(imageItem->descriptor.height, imageItem->descriptor.width);

                imageItem->descriptor.rowPitchInBytes = imageItem->descriptor.width * image->GetBytesPerTexel();

                return std::make_shared<Image>(imageItem, image->GetSubImageGroupType());
            }
            else
                return image;

        }

        static IMCodec::ImageSharedPtr ConvertImageWithNormalization(IMCodec::ImageSharedPtr image, IMCodec::TexelFormat targetTexelFormat, bool isRainbow)
        {

            const NormalizeMode normalizeMode = isRainbow ? NormalizeMode::RainBow : NormalizeMode::GrayScale;

            switch (image->GetTexelFormat())
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

            case IMCodec::TexelFormat::I_A8:
            case IMCodec::TexelFormat::I_X8:
                image = ImageUtil::Normalize<uint8_t>(image, normalizeMode);
                break;

            case IMCodec::TexelFormat::S_X16:
                image = ImageUtil::Normalize<int16_t>(image, normalizeMode);
                break;
            /*case IMCodec::TexelFormat::F_R32_G32_B32:
                image = ImageUtil::Normalize<int16_t>(image, normalizeMode);
                break;*/

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


            ImageItemSharedPtr normalizedImageItem = std::make_shared<ImageItem>();
            ImageDescriptor& normalizedImageProperties = normalizedImageItem->descriptor;
            //imageItem->itemType = image->getty

            if (image->GetIsRowPitchNormalized() == true)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::LogicError, "image already normalized.");


            const std::byte* oldBuffer = image->GetBuffer();
            normalizedImageProperties = image->GetDescriptor();
            normalizedImageItem->data.Allocate(image->GetTotalSizeOfImageTexels());
            uint32_t targetRowPitch = image->GetBytesPerRowOfPixels();
            std::byte* newBuffer = normalizedImageItem->data.data();

            for (std::size_t y = 0; y < image->GetHeight(); y++)
                for (std::size_t x = 0; x < targetRowPitch; x++)
                {
                    std::size_t srcIndex = y * image->GetRowPitchInBytes() + x;
                    std::size_t dstIndex = y * targetRowPitch + x;

                    newBuffer[dstIndex] = oldBuffer[srcIndex];
                }

            normalizedImageProperties.rowPitchInBytes = targetRowPitch;


            return std::make_shared<Image>(normalizedImageItem, image->GetSubImageGroupType());
        }

        static IMCodec::ImageSharedPtr Convert(IMCodec::ImageSharedPtr sourceImage, IMCodec::TexelFormat targetPixelFormat)
        {
            using namespace IMCodec;

            ImageSharedPtr convertedImage;

            if (sourceImage->GetTexelFormat() != targetPixelFormat)
            {
                ImageItemSharedPtr imageItem = std::make_shared<ImageItem>();
                imageItem->descriptor = sourceImage->GetDescriptor();

                uint8_t targetPixelSize = GetTexelFormatSize(targetPixelFormat);
                imageItem->data.Allocate(sourceImage->GetTotalPixels() * targetPixelSize);

                std::byte* dest = imageItem->data.data();
                //TODO: convert without normalization.
                ImageSharedPtr normalizedImage = sourceImage->GetIsRowPitchNormalized() == true ? sourceImage : NormalizePitch(sourceImage);

                imageItem->descriptor.texelFormatDecompressed = targetPixelFormat;
                imageItem->descriptor.rowPitchInBytes = normalizedImage->GetRowPitchInTexels() * targetPixelSize / CHAR_BIT;


                bool succcess = false;
                // Try convert using the meta programmed swizzler
                if ((succcess =
                    texelConvertor.Convert(targetPixelFormat
                        , sourceImage->GetDescriptor().texelFormatDecompressed
                        , dest
                        , normalizedImage->GetBuffer()
                        , normalizedImage->GetTotalPixels())) == false)
                {

                    // if couldn't convert using the above method, try a 'standard' conversion method
                    auto converter = sConvertionFunction.find(ConvertKey(sourceImage->GetTexelFormat(), targetPixelFormat));

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
                    convertedImage = std::make_shared<Image>(imageItem, sourceImage->GetSubImageGroupType());

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
                ImageItemSharedPtr imageItem = std::make_shared<ImageItem>();

                const uint32_t destBufferSize = subimage.GetWidth() * subimage.GetHeight() * sourceImage->GetBytesPerTexel();
                imageItem->descriptor = sourceImage->GetDescriptor();
                imageItem->data.Allocate(destBufferSize);
                std::byte* destBuffer = imageItem->data.data();

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

                imageItem->descriptor.height = subimage.GetHeight();
                imageItem->descriptor.width = subimage.GetWidth();

                imageItem->descriptor.rowPitchInBytes = subimage.GetWidth() * sourceImage->GetBytesPerTexel();

                ImageSharedPtr subImagePtr = std::make_shared<Image>(imageItem, sourceImage->GetSubImageGroupType());

                return subImagePtr;
            }
            return nullptr;
        }

        template <typename _FwdItDst, typename _FwdItSrc, typename value_type = typename _FwdItDst::value_type, 
            typename Range = std::pair<value_type, value_type> >
        static void NormalizeAnyToRGBA(_FwdItDst _DstFirst, _FwdItDst _DstLast, _FwdItSrc _SrcFirst, _FwdItSrc _SrcLast, NormalizeMode mode, const Range& minMax)
        {
            //destination is assumed to be 8 bit RGBA.
            const auto range = minMax.second - minMax.first;
            static_assert(sizeof(value_type) == sizeof(LLUtils::Color), "Currently normalization support output for 32 bit color only");

            while (_SrcFirst != _SrcLast)
            {
                const auto& sourceSample = *_SrcFirst;
                LLUtils::Color& destSample = reinterpret_cast<LLUtils::Color&>(_DstFirst->value);

                switch (mode)
                {
                case NormalizeMode::Default:
                case NormalizeMode::GrayScale:
                {
                    uint8_t grayValue = std::min(static_cast<uint8_t>(static_cast<double>(sourceSample - minMax.first) / range * 255.0 + 0.5), static_cast<uint8_t>(255));
                    destSample = LLUtils::Color(grayValue, grayValue, grayValue);
                }
                break;

                case NormalizeMode::RainBow:
                {
                    //Red (0) is the minimum , Magenta is the maximum (300)
                    const uint16_t hue = static_cast<uint16_t>(static_cast<double>(sourceSample - minMax.first) / range * 300.0);
                    destSample = LLUtils::Color::FromHSL(hue, 0.5, 0.5);
                }
                break;
                }

                _SrcFirst++;
                _DstFirst++;
            }
        }

        /// <summary>
        /// Find min and maximum value in typed data structure, traversing line by line
        /// </summary>
        /// <typeparam name="SourceSampleType"></typeparam>
        /// <param name="sampleData"></param>
        /// <param name="width"></param>
        /// <param name="rowPitch"></param>
        /// <param name="height"></param>
        /// <returns></returns>
        template <typename SourceSampleType>
        static std::pair<SourceSampleType, SourceSampleType> FindMinMax(const SourceSampleType* sampleData, uint32_t width, uint32_t rowPitch, uint32_t height)
        {
            size_t sourceOffset = 0;
            SourceSampleType min = std::numeric_limits< SourceSampleType>::max();
            SourceSampleType max = std::numeric_limits< SourceSampleType>::min();
            for (uint32_t y = 0; y < height; y++)
            {
                auto sourcePtr = reinterpret_cast<const uint8_t*>(sampleData) + sourceOffset;
                for (uint32_t x = 0; x < width; x++)
                {
                    const SourceSampleType sample = reinterpret_cast<const SourceSampleType*>(sourcePtr)[x];
                    if (sample < min)
                        min = sample;

                    if (sample > max)
                        max = sample;
                }

                sourceOffset += rowPitch;
            }

            return { min,max };
        }


        /// <summary>
        /// Normalize data from single channel to RGBA
        /// </summary>
        /// <typeparam name="SourceSampleType"></typeparam>
        /// <param name="sourceImage"></param>
        /// <param name="normalizeMode"></param>
        /// <returns></returns>
        template <class SourceSampleType>
        static IMCodec::ImageSharedPtr Normalize(IMCodec::ImageSharedPtr sourceImage, NormalizeMode normalizeMode)
        {
            const SourceSampleType* sampleData = reinterpret_cast<const SourceSampleType*> (sourceImage->GetBuffer());
            const uint32_t totalPixels = sourceImage->GetTotalPixels();
            using namespace IMCodec;
            ImageItemSharedPtr imageItem = std::make_shared<ImageItem>();

            imageItem->descriptor = sourceImage->GetDescriptor();
            imageItem->descriptor.texelFormatDecompressed = IMCodec::TexelFormat::I_R8_G8_B8_A8;
            imageItem->descriptor.rowPitchInBytes = IMCodec::GetTexelFormatSize(imageItem->descriptor.texelFormatDecompressed) / CHAR_BIT * imageItem->descriptor.width;
            imageItem->data.Allocate(imageItem->descriptor.rowPitchInBytes * imageItem->descriptor.height);

            if (sourceImage->GetIsRowPitchNormalized() == true)
            {
                std::span sourceData(reinterpret_cast<const SourceSampleType*>(sampleData), totalPixels);
                std::span bgraData(reinterpret_cast<PixelUtil::BitTexel32Ex*>(imageItem->data.data()), totalPixels);
                //Find minMax is a single pass since image data is consecutive 
                auto minMax = std::minmax_element(std::execution::parallel_unsequenced_policy(), std::begin(sourceData), std::end(sourceData));
                NormalizeAnyToRGBA(std::begin(bgraData), std::end(bgraData), std::begin(sourceData), std::end(sourceData), normalizeMode, std::make_pair(*minMax.first, *minMax.second) );
            }
            else
            {
                //find min max row by row by calling  FindMinMax
                auto minMax = FindMinMax(sampleData, sourceImage->GetWidth(), sourceImage->GetRowPitchInBytes(), sourceImage->GetHeight());
                size_t sourceOffset = 0;
                size_t destOffset = 0;
                for (uint32_t y = 0; y < sourceImage->GetHeight(); y++)
                {
                    auto sourcePtr = reinterpret_cast<const uint8_t*>(sampleData) + sourceOffset;
                    auto destPtr = reinterpret_cast<uint8_t*>(imageItem->data.data()) + destOffset;
                    std::span sourceData(reinterpret_cast<const SourceSampleType*>(sourcePtr), sourceImage->GetWidth());
                    std::span bgraData(reinterpret_cast<PixelUtil::BitTexel32Ex*>(destPtr), sourceImage->GetWidth());

                    NormalizeAnyToRGBA(std::begin(bgraData), std::end(bgraData), std::begin(sourceData), std::end(sourceData), normalizeMode, minMax);

                    sourceOffset += sourceImage->GetRowPitchInBytes();
                    destOffset += imageItem->descriptor.rowPitchInBytes;
                }
            }

            return std::make_shared<Image>(imageItem, sourceImage->GetSubImageGroupType());
        }

        static IMCodec::ImageSharedPtr CropImage(IMCodec::ImageSharedPtr inputImage, const LLUtils::RectI32& rect)
        {
            LLUtils::RectI32 imageRect = { { 0,0 } , static_cast<LLUtils::PointI32>(inputImage->GetDimensions()) };
            LLUtils::RectI32 cuttedRect = rect.Intersection(imageRect);
            IMCodec::ImageSharedPtr subImage = GetSubImage(inputImage, cuttedRect);
            return subImage;
        }



        static IMCodec::ImageSharedPtr FillColor(IMCodec::ImageSharedPtr inputImage, const LLUtils::RectI32& rect, LLUtils::Color color)
        {
            if (inputImage->GetTexelFormat() == IMCodec::TexelFormat::I_R8_G8_B8_A8)
            {
                LLUtils::RectI32 image = { { 0,0 } ,{ static_cast<int32_t> (inputImage->GetWidth())  , static_cast<int32_t> (inputImage->GetHeight()) } };

                auto topLeft = rect.GetCorner(LLUtils::Corner::TopLeft);

                if (rect.IsNonNegative() && rect.IsInside(image))
                {
                    using namespace IMCodec;
                    ImageItemSharedPtr imageItem = std::make_shared<ImageItem>();

                    //Copy the image item
                    *imageItem = *inputImage->GetImageItem();

                    std::byte* imageBuffer = imageItem->data.data();

                    for (int32_t y = 0; y < rect.GetHeight(); y++)
                    {
                        for (int32_t x = 0; x < rect.GetWidth(); x++)
                        {
                            auto curPos = imageBuffer + (topLeft.y +  y) * imageItem->descriptor.rowPitchInBytes + (x +  topLeft.x) * GetTexelFormatSize(imageItem->descriptor.texelFormatDecompressed) / CHAR_BIT;
                            *reinterpret_cast<LLUtils::Color*>(curPos) = color;
                        }
                    }

                    ImageSharedPtr cuttedImagePtr = std::make_shared<Image>(imageItem, inputImage->GetSubImageGroupType());

                    return cuttedImagePtr;
                }
            }
            return nullptr;
        }
        

        static bool HasAlphaChannelAndInUse(IMCodec::ImageSharedPtr inputImage)
        {
            bool alphaChannelInUse{};
            bool alphaChannelFound{};
            IMCodec::ChannelWidth alphaChannelWidth{};
            for (auto c = 0; c < inputImage->GetTexelInfo().numChannles; c++)
                if (inputImage->GetTexelInfo().channles[c].semantic == IMCodec::ChannelSemantic::Opacity)
                {
                    alphaChannelFound = true;
                    alphaChannelWidth = inputImage->GetTexelInfo().channles[c].width;
                }


            if (alphaChannelFound == true)
            {
                if (alphaChannelWidth % 8 != 0 || inputImage->GetTexelFormat() != IMCodec::TexelFormat::I_R8_G8_B8_A8)
                {
                    //Currently, support only RGBA input
                    //If alpha channel exist and its width is is not multiple of 8, for simplicity assume that alpha is in use.
                    alphaChannelInUse = true;
                }
                else
                {
                    

                    for (uint32_t y = 0; y < inputImage->GetHeight(); y++)
                        for (uint32_t x = 0; x < inputImage->GetWidth(); x++)
                        {
                            if (((const PixelUtil::BitTexel32*)inputImage->GetBufferAt(x, y))->W != 255)
                            {
                                alphaChannelInUse = true;
                                break;
                            }
                        }
                }
            }

            return alphaChannelInUse;
        }
    };
}

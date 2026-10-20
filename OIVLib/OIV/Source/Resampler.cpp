#include <ImageProcessing/Resampler.h>
#include <System.h>

#include <algorithm>
#include <cstring>
#include <LLUtils/Exception.h>
#include <array>
#include <bit>
#include <span>

namespace OIV
{
    namespace
    {
        struct ResamplerParams
        {
            uint32_t* targetBuffer;
            uint32_t targetWidth;
            uint32_t targetHeight;
            const std::byte* sourceBuffer;
            uint32_t sourceWidth;
            uint32_t sourceHeight;
            size_t sourceRowPitchInBytes;
        };

        struct SampleSpan
        {
            uint32_t first;
            uint32_t end;
            // Edge positions within a source texel, in units of the target axis length.
            uint32_t firstOffset;
            uint32_t lastEnd;

            uint32_t Weight(uint32_t sample, uint32_t unit) const
            {
                const uint32_t left  = sample == first ? firstOffset : 0;
                const uint32_t right = sample + 1 == end ? lastEnd : unit;
                return right - left;
            }
        };

        std::vector<SampleSpan> BuildSampleSpans(uint32_t sourceSize, uint32_t targetSize)
        {
            std::vector<SampleSpan> spans(targetSize);
            for (uint32_t index = 0; index < targetSize; ++index)
            {
                const uint64_t first = static_cast<uint64_t>(index) * sourceSize;
                const uint64_t last  = first + sourceSize - 1;
                spans[index]         = {
                    .first       = static_cast<uint32_t>(first / targetSize),
                    .end         = static_cast<uint32_t>(last / targetSize + 1),
                    .firstOffset = static_cast<uint32_t>(first % targetSize),
                    .lastEnd     = static_cast<uint32_t>(last % targetSize + 1),
                };
            }
            return spans;
        }

        // Keep the direct (fused) 2D weighted average, with boundaries prepared once for all row workers.
        // Single-worker benchmarks also tried full-image and row-streamed two-pass filters, plus column sums
        // with a running integral. Those variants needed intermediate buffers and did not consistently win,
        // including common half-size reductions. Keep exact integer weights and only final normalization.
        // A single output row cannot amortize horizontal boundary preparation across rows.
        void ResampleRows(const ResamplerParams& params, std::span<const SampleSpan> columns,
                          std::span<const SampleSpan> rows, size_t firstRow, size_t lastRow)
        {
            // Scaling coordinates by target dimensions makes fractional overlaps exact integers.
            // Every output footprint has area sourceWidth * sourceHeight in these units.
            const uint32_t unitX         = params.targetWidth;
            const uint32_t unitY         = params.targetHeight;
            const uint64_t footprintArea = static_cast<uint64_t>(params.sourceWidth) * params.sourceHeight;
            for (size_t y = firstRow; y < lastRow; ++y)
            {
                const auto& vertical = rows[y];
                for (size_t x = 0; x < params.targetWidth; ++x)
                {
                    const auto& horizontal = columns[x];
                    // 64-bit sums cover 256K x 256K sources even when reduced to a single output pixel.
                    std::array<uint64_t, 4> sum{};
                    for (uint32_t sourceY = vertical.first; sourceY < vertical.end; ++sourceY)
                    {
                        const uint64_t weightY = vertical.Weight(sourceY, unitY);
                        const auto* row        = params.sourceBuffer +
                                                 static_cast<size_t>(sourceY) * params.sourceRowPitchInBytes;
                        for (uint32_t sourceX = horizontal.first; sourceX < horizontal.end; ++sourceX)
                        {
                            const uint64_t weight = horizontal.Weight(sourceX, unitX) * weightY;
                            // Byte reads also support crops whose original row pitch is not four-byte aligned.
                            const auto* pixel = reinterpret_cast<const uint8_t*>(row + static_cast<size_t>(sourceX) *
                                                                                           sizeof(uint32_t));
                            for (size_t channel = 0; channel < sum.size(); ++channel)
                                sum[channel] += pixel[channel] * weight;
                        }
                    }
                    std::array<uint8_t, 4> pixel{};
                    for (size_t channel = 0; channel < sum.size(); ++channel)
                        pixel[channel] = static_cast<uint8_t>(sum[channel] / footprintArea);
                    params.targetBuffer[y * params.targetWidth + x] = std::bit_cast<uint32_t>(pixel);
                }
            }
        }
    }  // namespace

    IMCodec::ImageSharedPtr Resampler::Resample(IMCodec::ImageSharedPtr sourceImage, LLUtils::PointI32 targetSize,
                                                std::optional<LLUtils::RectI32> sourceRect)
    {
        const LLUtils::RectI32 imageRect{{0, 0}, static_cast<LLUtils::PointI32>(sourceImage->GetDimensions())};
        const auto region = sourceRect.value_or(imageRect);
        if (region.IsEmpty() || !region.IsInside(imageRect) || targetSize.x <= 0 || targetSize.y <= 0)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters,
                         "Invalid resampling dimensions or source region");

        const uint32_t width  = targetSize.x;
        const uint32_t height = targetSize.y;

        using namespace IMCodec;
        // Create target downscaled image.
        ImageItemSharedPtr imageItem = std::make_shared<ImageItem>();
        ImageDescriptor& desc        = imageItem->descriptor;
        desc.height                  = height;
        desc.width                   = width;
        desc.rowPitchInBytes         = width * sourceImage->GetBytesPerTexel();
        desc.texelFormatDecompressed = sourceImage->GetTexelFormat();
        desc.texelFormatStorage      = sourceImage->GetOriginalTexelFormat();
        imageItem->data.Allocate(static_cast<size_t>(desc.rowPitchInBytes) * height);

        ImageSharedPtr resampled = std::make_shared<IMCodec::Image>(imageItem, ImageItemType::Unknown);
        const auto origin        = region.GetCorner(LLUtils::Corner::TopLeft);
        const size_t sourcePitch = sourceImage->GetRowPitchInBytes();
        const auto* sourceBuffer = sourceImage->GetBuffer() + static_cast<size_t>(origin.y) * sourcePitch +
                                   static_cast<size_t>(origin.x) * sourceImage->GetBytesPerTexel();
        if (width == region.GetWidth() && height == region.GetHeight())
        {
            for (uint32_t y = 0; y < height; ++y)
                std::memcpy(imageItem->data.data() + static_cast<size_t>(y) * desc.rowPitchInBytes,
                            sourceBuffer + static_cast<size_t>(y) * sourcePitch, desc.rowPitchInBytes);
        }
        else
        {
            const ResamplerParams params{
                .targetBuffer          = reinterpret_cast<uint32_t*>(imageItem->data.data()),
                .targetWidth           = width,
                .targetHeight          = height,
                .sourceBuffer          = sourceBuffer,
                .sourceWidth           = static_cast<uint32_t>(region.GetWidth()),
                .sourceHeight          = static_cast<uint32_t>(region.GetHeight()),
                .sourceRowPitchInBytes = sourcePitch,
            };
            const auto columns = BuildSampleSpans(params.sourceWidth, width);
            const auto rows    = BuildSampleSpans(params.sourceHeight, height);
            if (fThreads.empty())
                fThreads.resize(System::GetIdealNumThreadsForMemoryOperations());
            // Each worker receives at least one output row. Thin images need no empty worker launches.
            const size_t threadCount = std::min(fThreads.size(), static_cast<size_t>(params.targetHeight));
            if (threadCount == 1)
                ResampleRows(params, columns, rows, 0, params.targetHeight);
            else
            {
                for (size_t index = 0; index < threadCount; ++index)
                {
                    const size_t firstRow = static_cast<size_t>(params.targetHeight) * index / threadCount;
                    const size_t lastRow  = static_cast<size_t>(params.targetHeight) * (index + 1) / threadCount;
                    fThreads[index]       = std::thread([&params, &columns, &rows, firstRow, lastRow]
                                                        { ResampleRows(params, columns, rows, firstRow, lastRow); });
                }
                for (size_t index = 0; index < threadCount; ++index)
                    fThreads[index].join();
            }
        }

        return resampled;
    }
}  // namespace OIV

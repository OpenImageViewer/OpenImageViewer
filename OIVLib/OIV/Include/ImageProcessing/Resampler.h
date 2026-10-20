#pragma once
#include <Image.h>
#include <LLUtils/Point.h>
#include <LLUtils/Rect.h>
#include <optional>
#include <thread>
#include <vector>

namespace OIV
{
    // Reusable CPU producer. Calls on one instance are synchronous and must not overlap.
    class Resampler
    {
      public:

        // Area averaging of nonempty RGBA8 buffers up to 256K texels per axis.
        // The optional crop uses source coordinates and retains the original byte row pitch.
        IMCodec::ImageSharedPtr Resample(IMCodec::ImageSharedPtr sourceImage, LLUtils::PointI32 targetSize,
                                         std::optional<LLUtils::RectI32> sourceRect = std::nullopt);

      private:

        std::vector<std::thread> fThreads;
    };
}  // namespace OIV

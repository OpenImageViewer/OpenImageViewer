#pragma once

#include <cstdint>

namespace OIV
{
    enum class ColorCountCompletionAction
    {
        ApplyToCurrentImage,
        CountVisibleImage,
        Ignore
    };

    class ColorCountPolicy
    {
      public:

        static ColorCountCompletionAction DecideCompletion(uint64_t countedGeneration, uint64_t currentGeneration,
                                                           bool imageInfoVisible);
        static int64_t NormalizeCountResult(int64_t countedValue, int64_t failureSentinel, int64_t failedValue);
    };
}  // namespace OIV

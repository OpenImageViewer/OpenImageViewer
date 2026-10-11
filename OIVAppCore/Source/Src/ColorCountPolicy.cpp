#include <OIVAppCore/ColorCountPolicy.h>

namespace OIV
{
    ColorCountCompletionAction ColorCountPolicy::DecideCompletion(uint64_t countedGeneration,
                                                                  uint64_t currentGeneration, bool imageInfoVisible)
    {
        if (countedGeneration == currentGeneration)
            return ColorCountCompletionAction::ApplyToCurrentImage;

        return imageInfoVisible ? ColorCountCompletionAction::CountVisibleImage : ColorCountCompletionAction::Ignore;
    }

    int64_t ColorCountPolicy::NormalizeCountResult(int64_t countedValue, int64_t failureSentinel, int64_t failedValue)
    {
        return countedValue != failureSentinel ? countedValue : failedValue;
    }
}  // namespace OIV

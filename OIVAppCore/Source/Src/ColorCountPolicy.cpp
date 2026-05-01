#include <OIVAppCore/ColorCountPolicy.h>

namespace OIV
{
    ColorCountCompletionAction ColorCountPolicy::DecideCompletion(const void* countedImage,
                                                                  const void* currentImage,
                                                                  bool imageInfoVisible)
    {
        if (countedImage == currentImage)
            return ColorCountCompletionAction::ApplyToCurrentImage;

        return imageInfoVisible ? ColorCountCompletionAction::CountVisibleImage : ColorCountCompletionAction::Ignore;
    }

    int64_t ColorCountPolicy::NormalizeCountResult(int64_t countedValue, int64_t failureSentinel, int64_t failedValue)
    {
        return countedValue != failureSentinel ? countedValue : failedValue;
    }
}

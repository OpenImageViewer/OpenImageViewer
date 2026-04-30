#include <oivappcore/SlideshowPolicy.h>

namespace OIV
{
    void SlideshowPolicy::SetEnabled(bool enabled)
    {
        fEnabled = enabled;
    }

    bool SlideshowPolicy::IsEnabled() const
    {
        return fEnabled;
    }

    void SlideshowPolicy::SetIntervalMs(std::uint32_t intervalMs)
    {
        fIntervalMs = intervalMs;
    }

    std::uint32_t SlideshowPolicy::GetIntervalMs() const
    {
        return fIntervalMs;
    }

    std::uint32_t SlideshowPolicy::GetTimerIntervalMs() const
    {
        return fEnabled ? fIntervalMs : 0;
    }

    bool SlideshowPolicy::ShouldWrap(std::ptrdiff_t currentIndex, std::size_t fileCount) const
    {
        return fileCount > 0 && currentIndex == static_cast<std::ptrdiff_t>(fileCount - 1);
    }
}  // namespace OIV

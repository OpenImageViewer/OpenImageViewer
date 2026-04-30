#pragma once

#include <cstddef>
#include <cstdint>

namespace OIV
{
    class SlideshowPolicy
    {
      public:
        void SetEnabled(bool enabled);
        bool IsEnabled() const;
        void SetIntervalMs(std::uint32_t intervalMs);
        std::uint32_t GetIntervalMs() const;
        std::uint32_t GetTimerIntervalMs() const;
        bool ShouldWrap(std::ptrdiff_t currentIndex, std::size_t fileCount) const;

      private:
        bool fEnabled = false;
        std::uint32_t fIntervalMs = 3000;
    };
}  // namespace OIV

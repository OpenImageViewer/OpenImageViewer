#pragma once

#include <LLUtils/Event.h>

#include <LWS/Platform.hpp>

namespace OIV
{
    // Transitional shim until LWS reports monitor changes for a window directly.
    class MonitorProvider
    {
      public:

        using ChangeEvent = LLUtils::Event<void(const LWS::MonitorDesc&)>;
        ChangeEvent Changed;
        void UpdateFromWindow(LWS::Window& window);

      private:

        LWS::MonitorDesc fMonitorDesc{};
    };
}  // namespace OIV

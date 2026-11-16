#pragma once
#include <string>
#include <LWS/Bitmap.hpp>
#include <LWS/Window.hpp>
#include <LWSUI/UIHost.hpp>
#include <LWSUI/Event.hpp>

namespace OIV
{
    class AboutForm
    {
      public:

        explicit AboutForm(LWS::PlatformContext& platform) : fWindow(platform) {}
        LWS::Result Show(LWS::Window& owner, LWS::BitmapSharedPtr icon, std::string buildTime);

      private:

        LWS::Window fWindow;
        std::unique_ptr<LWSUI::UIHost> fHost;
        LWS::EventConnection fEvents;
        LWSUI::ConnectionSet fConnections;
    };
}  // namespace OIV

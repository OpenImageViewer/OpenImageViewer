#pragma once
#include <LWS/Window.hpp>
#include <LWSUI/UIHost.hpp>
#include <functional>

namespace LWSUI
{
    class Button;
}

namespace OIV
{
    // Presentation only: the application handles the choice after this owned modal window is hidden.
    class FileAssociationPrompt
    {
      public:

        enum class Choice
        {
            AssociateAll,
            Selective,
            AskNextTime,
            SuppressForCopy,
            SuppressForUser
        };
        explicit FileAssociationPrompt(LWS::PlatformContext& platform) : fWindow(platform) {}
        LWS::Result Show(LWS::Window& owner, std::function<void(Choice)> respond);
        // Programmatic dismissal (shutdown or replacement) does not submit a response.
        void Hide();

      private:

        void Finish(Choice choice);
        LWS::Window fWindow;
        std::unique_ptr<LWSUI::UIHost> fHost;
        LWS::EventConnection fEvents;
        LWSUI::ConnectionSet fConnections;
        LWSUI::Button* fDefault{};
        std::function<void(Choice)> fRespond;
    };
}  // namespace OIV

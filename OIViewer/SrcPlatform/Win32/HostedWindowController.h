#pragma once
#include <LWS/Window.hpp>
namespace OIV
{
    class MainWindow;
}
namespace OIV::Win32
{
    // A child cannot own desktop fullscreen. Borrow the composite and move it into an owned shell without
    // replacing its canvas or renderer. The MainWindow outlives this controller and destroys it on its UI thread.
    class HostedWindowController final
    {
      public:

        explicit HostedWindowController(MainWindow& window) : owner(window) {}
        LWS::Result SetWindowMode(LWS::WindowMode mode);
        LWS::WindowMode GetWindowMode() const;
        LWS::Result Destroy();

      private:

        MainWindow& owner;
        LWS::Window* dockParent{};
        bool escaped{};
        std::unique_ptr<LWS::Window> fullscreen;
        LWS::EventConnection fullscreenConnection;
    };
}  // namespace OIV::Win32

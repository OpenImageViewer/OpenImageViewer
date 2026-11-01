#pragma once
#include <Windows.h>
#include <cstdint>
namespace OIV
{
    class Viewer;
}
namespace OIV::Win32
{
    // Translate hosted focus and capture without changing the host process's raw input registration.
    class HostedInputAdapter final
    {
      public:

        explicit HostedInputAdapter(Viewer& owner) : viewer(owner) {}
        void HandleInput(HWND window, uint32_t message, uintptr_t parameter, uint32_t& pressedButtons) noexcept;

      private:

        Viewer& viewer;
    };
}  // namespace OIV::Win32

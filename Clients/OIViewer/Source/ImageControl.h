#pragma once

#include "ImageList.h"

#include <LWS/Window.hpp>

#include <cstdint>
#include <memory>

namespace OIV
{
    class ImageControl
    {
      public:

        explicit ImageControl(LWS::PlatformContext& platform);
        ~ImageControl();

        [[nodiscard]] LWS::Window& GetWindow() { return fWindow; }
        [[nodiscard]] const LWS::Window& GetWindow() const { return fWindow; }

        void SetImagePos(int pos);
        ImageList& GetImageList();
        void RefreshScrollInfo();
        std::intptr_t SendMessage(std::uint32_t message, std::uintptr_t wParam, std::intptr_t lParam);

      private:

        struct NativeState;

        bool HandleWindowEvent(const LWS::AnyEvent& eventData);
        void InitializeEvents();
        void InitializePlatformRendering();
        void RequestRepaint();
        void UpdateScrollPosition();

        LWS::Window fWindow;
        ImageList fImageList;
        std::unique_ptr<NativeState> fNativeState;
        ImageList::ChangedEvent::Subscription fImageListSubscription;
        LWS::EventConnection fEventConnection;
        LWS::EventConnection fPlatformConnection;
    };
}  // namespace OIV

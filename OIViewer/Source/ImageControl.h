#pragma once
#include "ImageList.h"
#include <LWSUI/Containers.hpp>
#include <LWSUI/Primitives.hpp>

namespace OIV
{
    // Virtualized sidebar rows share the main window's UI host and input routing.
    class ImageControl final : public LWSUI::Panel
    {
      public:

        ImageControl();
        ImageList& GetImageList();
        void RefreshScrollInfo();

      protected:

        LWSUI::Size OnMeasure(LWSUI::Size available) override { return available; }
        void OnArrange() override;
        void OnRender(LWSUI::Canvas& canvas) override;
        bool OnInput(const LWSUI::Input& input) override;

      private:

        ImageList fImageList;
        LWSUI::ScrollBar* fScrollBar{};
        ImageList::ChangedEvent::Subscription fImageListSubscription;
        LWSUI::ConnectionSet fConnections;
    };
}  // namespace OIV

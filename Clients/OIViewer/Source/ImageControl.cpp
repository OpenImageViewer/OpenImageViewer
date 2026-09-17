#include "ImageControl.h"

#include <LLUtils/Exception.h>
#include <utility>

namespace OIV
{
    void ImageControl::InitializeEvents()
    {
        fImageList.Changed.Add(
            [this](ImageList::ChangeType change)
            {
                switch (change)
                {
                    case ImageList::ChangeType::ItemCount:
                        RefreshScrollInfo();
                        break;
                    case ImageList::ChangeType::ScrollPosition:
                        UpdateScrollPosition();
                        [[fallthrough]];
                    case ImageList::ChangeType::Visual:
                        RequestRepaint();
                        break;
                }
            });
        auto connection = fWindow.Listen(
            [this](const LWS::AnyEvent& eventData)
            { return HandleWindowEvent(eventData) ? LWS::EventResponse::Handled : LWS::EventResponse::Unhandled; });
        if (!connection.has_value())
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to register required window listener");
        fEventConnection = std::move(*connection);
    }

    void ImageControl::SetImagePos(int pos)
    {
        fImageList.SetPos(pos);
    }

    ImageList& ImageControl::GetImageList()
    {
        return fImageList;
    }
}  // namespace OIV

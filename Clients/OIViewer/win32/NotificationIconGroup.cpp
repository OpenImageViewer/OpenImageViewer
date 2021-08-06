#include "NotificationIconGroup.h"
#include <defs.h>
#include <windowsx.h>
namespace OIV::Win32
{
    NotificationIconGroup::IconID NotificationIconGroup::AddIcon(LPWSTR IconName, const std::wstring& tooltip)
    {
        if (fWindow.GetHandle() == nullptr)
        {
            fWindow.Create();
            fWindow.SetVisible(false);
            fWindow.AddEventListener(std::bind(&NotificationIconGroup::OnWindowMessage, this, std::placeholders::_1));
        }
        NOTIFYICONDATA nid{};
        nid.cbSize = sizeof(nid);
        nid.hWnd = fWindow.GetHandle();
        nid.uVersion = NOTIFYICON_VERSION_4;
        nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
        IconID iconId = fIconIdProvider.Acquire();
        nid.uID = static_cast<UINT>(iconId);
        nid.uCallbackMessage = WM_PRIVATE_NOTIFICATION_CALLBACK_MESSAGE_ID;

        LLUtils::StringUtility::StrCpy(nid.szTip, LLUtils::array_length(nid.szTip), tooltip.c_str());
        
        nid.hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IconName));
        
        if (Shell_NotifyIcon(NIM_ADD, &nid) == TRUE && Shell_NotifyIcon(NIM_SETVERSION, &nid) == TRUE)
        {
            fMapIconData.emplace(iconId, NotificationIconData{ iconId });
        }
        else
        {
            fIconIdProvider.Release(iconId);
            LL_EXCEPTION_SYSTEM_ERROR("Cannot add notification icon");
        }

        return iconId;
    }

  /*  void RemoveIcon(NotificationIconGroup::IconID iconID)
    {

    }*/


    NotificationIconGroup::~NotificationIconGroup()
    {
        NOTIFYICONDATA nid{};
        nid.cbSize = sizeof(nid);
        nid.hWnd = fWindow.GetHandle();
        nid.uVersion = NOTIFYICON_VERSION_4;
        nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;


        for (const auto& [ id, iconData] : fMapIconData)
        {
            nid.uID = id;
            if (Shell_NotifyIcon(NIM_DELETE, &nid) == TRUE)
            {
                

            }
        }
    }

    LLUtils::Rect<uint16_t> NotificationIconGroup::GetIconRect(IconID iconid)
    {
        auto iconData = fMapIconData.find(iconid);
        if (iconData != fMapIconData.end())
        {
            NOTIFYICONIDENTIFIER iconIdentifer{ static_cast<DWORD>(sizeof(NOTIFYICONIDENTIFIER)), fWindow.GetHandle(),static_cast<UINT>(iconid), GUID{} };
            RECT rect;

            if (Shell_NotifyIconGetRect(&iconIdentifer, &rect) == S_OK) [[likely]]
            {
                using type = LLUtils::Rect<uint16_t>::Point_Type::point_type;
                return {LLUtils::Rect<uint16_t>::Point_Type {(type)rect.left,(type) rect.top } ,{(type)rect.right, (type)rect.bottom}};
            }
            else [[unlikely]]
            {
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotFound, "Icon id not found");
            }
        };

        return {};
    }

    void NotificationIconGroup::HandleMessage(const WinMessage& message)
    {
        UINT notificationEvent = LOWORD(message.lParam);
        //USHORT iconID = HIWORD(message.lParam);
        short  x = GET_X_LPARAM(message.wParam);
        short y = GET_Y_LPARAM(message.wParam);

        switch (notificationEvent)
        {
        case NIN_SELECT:
            OnNotificationIconEvent.Raise(NotificationIconEventArgs { NotificationIconAction::Select, x, y });
            break;
        case NIN_KEYSELECT:
            break;
        case NIN_POPUPOPEN:
            break;
        case NIN_POPUPCLOSE:
            break;
        case NIN_BALLOONSHOW:
            break;
        case WM_CONTEXTMENU:
            OnNotificationIconEvent.Raise(NotificationIconEventArgs{ NotificationIconAction::ContextMenu, x, y });
        break;
        case WM_MOUSEMOVE:
            break;

        }
    }

    LRESULT NotificationIconGroup::OnWindowMessage(const Win32::Event* evnt)
    {
        const EventWinMessage* winEvent = dynamic_cast<const Win32::EventWinMessage*>(evnt);
        switch (winEvent->message.message)
        {
        case WM_PRIVATE_NOTIFICATION_CALLBACK_MESSAGE_ID:
            HandleMessage(winEvent->message);
            break;
        }
        
        return 0;
    }
}

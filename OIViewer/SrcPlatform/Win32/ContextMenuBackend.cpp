#include "ContextMenu.h"

#include <Windows.h>

#include <LWS/Win32/WindowExtensions.hpp>

#include <map>

namespace OIV::detail
{
    struct ContextMenuBackend::NativeState
    {
        explicit NativeState(HWND windowHandle) : window(windowHandle), menu(CreatePopupMenu()) {}

        ~NativeState()
        {
            if (menu != nullptr)
                DestroyMenu(menu);
        }

        HWND window  = nullptr;
        HMENU menu   = nullptr;
        bool visible = false;
    };

    ContextMenuBackend::ContextMenuBackend(LWS::Window& window)
        : fNativeState(std::make_unique<NativeState>(*LWS::Win32::GetHwnd(window)))
    {
    }

    ContextMenuBackend::~ContextMenuBackend() = default;

    uint32_t ContextMenuBackend::Show(int x, int y, AlignmentHorizontal horizontal, AlignmentVertical vertical)
    {
        UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
        switch (horizontal)
        {
            case AlignmentHorizontal::Left:
                flags |= TPM_LEFTALIGN;
                break;
            case AlignmentHorizontal::Center:
                flags |= TPM_CENTERALIGN;
                break;
            case AlignmentHorizontal::Right:
                flags |= TPM_RIGHTALIGN;
                break;
            case AlignmentHorizontal::None:
                break;
        }

        switch (vertical)
        {
            case AlignmentVertical::Top:
                flags |= TPM_TOPALIGN;
                break;
            case AlignmentVertical::Center:
                flags |= TPM_VCENTERALIGN;
                break;
            case AlignmentVertical::Bottom:
                flags |= TPM_BOTTOMALIGN;
                break;
            case AlignmentVertical::None:
                break;
        }

        fNativeState->visible = true;
        const uint32_t itemId = TrackPopupMenu(fNativeState->menu, flags, x, y, 0, fNativeState->window, nullptr);
        fNativeState->visible = false;
        return itemId;
    }

    void ContextMenuBackend::EnableItem(uint32_t itemId, bool enabled)
    {
        MENUITEMINFO info{};
        info.cbSize = sizeof(info);
        info.fMask  = MIIM_STATE;
        info.fState = enabled ? MFS_ENABLED : MFS_GRAYED;
        SetMenuItemInfo(fNativeState->menu, itemId, FALSE, &info);
    }

    void ContextMenuBackend::AddItem(uint32_t itemId, const LLUtils::native_string_type& name)
    {
        MENUITEMINFO info{};
        info.cbSize     = sizeof(info);
        info.fMask      = MIIM_STRING | MIIM_ID | MIIM_STATE;
        info.fState     = MFS_ENABLED;
        info.dwTypeData = const_cast<LLUtils::native_char_type*>(name.c_str());
        info.cch        = static_cast<UINT>(name.length());
        info.wID        = itemId;
        InsertMenuItem(fNativeState->menu, itemId, FALSE, &info);
    }

    bool ContextMenuBackend::IsVisible() const
    {
        return fNativeState->visible;
    }

    bool ContextMenuBackend::IsSupported() const
    {
        return true;
    }
}  // namespace OIV::detail

#include "ContextMenu.h"

#include <LLUtils/Exception.h>

namespace OIV::detail
{
    struct ContextMenuBackend::NativeState
    {
        bool visible = false;
    };

    ContextMenuBackend::ContextMenuBackend([[maybe_unused]] LWS::Window& window)
        : fNativeState(std::make_unique<NativeState>())
    {
    }

    ContextMenuBackend::~ContextMenuBackend() = default;

    uint32_t ContextMenuBackend::Show([[maybe_unused]] int x, [[maybe_unused]] int y,
                                      [[maybe_unused]] AlignmentHorizontal horizontal,
                                      [[maybe_unused]] AlignmentVertical vertical)
    {
        LL_EXCEPTION_NOT_IMPLEMENT("Native context menus are not implemented on Linux");
    }

    void ContextMenuBackend::EnableItem([[maybe_unused]] uint32_t itemId, [[maybe_unused]] bool enabled) {}

    void ContextMenuBackend::AddItem([[maybe_unused]] uint32_t itemId,
                                     [[maybe_unused]] const LLUtils::native_string_type& name)
    {
    }

    bool ContextMenuBackend::IsVisible() const
    {
        return fNativeState->visible;
    }

    bool ContextMenuBackend::IsSupported() const
    {
        return false;
    }
}  // namespace OIV::detail

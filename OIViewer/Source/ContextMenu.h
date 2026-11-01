#pragma once

#include <LLUtils/StringDefs.h>
#include <LWS/Window.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>

namespace OIV
{
    enum class AlignmentHorizontal
    {
        None,
        Left,
        Center,
        Right
    };

    enum class AlignmentVertical
    {
        None,
        Top,
        Center,
        Bottom
    };

    namespace detail
    {
        class ContextMenuBackend final
        {
          public:

            explicit ContextMenuBackend(LWS::Window& window);
            ~ContextMenuBackend();

            ContextMenuBackend(const ContextMenuBackend&)            = delete;
            ContextMenuBackend& operator=(const ContextMenuBackend&) = delete;

            uint32_t Show(int x, int y, AlignmentHorizontal horizontal, AlignmentVertical vertical);
            void EnableItem(uint32_t itemId, bool enabled);
            void AddItem(uint32_t itemId, const LLUtils::native_string_type& name);
            [[nodiscard]] bool IsVisible() const;
            [[nodiscard]] bool IsSupported() const;

          private:

            struct NativeState;
            std::unique_ptr<NativeState> fNativeState;
        };
    }  // namespace detail

    template <typename T>
    class ContextMenu final
    {
        struct MenuItemData
        {
            uint32_t id{};
            T userData{};
            LLUtils::native_string_type itemDisplayName;
            bool enabled = true;
        };

      public:

        explicit ContextMenu(LWS::Window& window) : fBackend(window) {}

        MenuItemData* Show(int x, int y, AlignmentHorizontal horizontal, AlignmentVertical vertical)
        {
            return GetItemByID(fBackend.Show(x, y, horizontal, vertical));
        }

        void EnableItem(const LLUtils::native_string_type& name, bool enabled)
        {
            const auto it = fMapCommandToData.find(name);
            if (it != fMapCommandToData.end())
            {
                it->second.enabled = enabled;
                fBackend.EnableItem(it->second.id, enabled);
            }
        }

        void AddItem(const LLUtils::native_string_type& name, const T& data)
        {
            MenuItemData menuData;
            menuData.id              = ++fCurrentItemID;
            menuData.itemDisplayName = name;
            menuData.userData        = data;

            const auto [it, inserted] = fMapCommandToData.emplace(name, std::move(menuData));
            if (inserted)
                fBackend.AddItem(it->second.id, it->first);
        }

        [[nodiscard]] bool IsVisible() const { return fBackend.IsVisible(); }
        [[nodiscard]] bool IsSupported() const { return fBackend.IsSupported(); }

      private:

        MenuItemData* GetItemByID(uint32_t itemId)
        {
            const auto it = std::find_if(fMapCommandToData.begin(), fMapCommandToData.end(),
                                         [itemId](const auto& pair) { return pair.second.id == itemId; });
            return it != fMapCommandToData.end() ? &it->second : nullptr;
        }

        std::map<LLUtils::native_string_type, MenuItemData> fMapCommandToData;
        detail::ContextMenuBackend fBackend;
        uint32_t fCurrentItemID = 0;
    };
}  // namespace OIV

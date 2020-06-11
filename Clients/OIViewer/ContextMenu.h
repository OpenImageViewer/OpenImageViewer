#pragma once
#include <Windows.h>
#include <string>
#include <map>

#include "LLUtils/StringDefs.h"

namespace OIV
{

	template <typename T>
	class ContextMenu final
	{
	struct MenuItemData : public T
	{
		int id;
		MENUITEMINFO info;
	};
		
	public:
		ContextMenu(HWND hwnd)
			:fHandle(hwnd)
		{
			fMenu = ::CreatePopupMenu();
		}
		~ContextMenu()
		{
			::DestroyMenu(fMenu);
		}
		T* Show(int x, int y)
		{
			fVisible = true;
			auto itemId = TrackPopupMenu(fMenu, TPM_CENTERALIGN | TPM_VCENTERALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, fHandle, nullptr);
			fVisible = false;
			return GetItemByID(itemId);
			
		}

		void EnableItem(LLUtils::native_string_type name, bool enabled)
		{
			auto it = fMapCommandToData.find(name);
			if (it != std::end(fMapCommandToData))
			{
				int itemID = it->second.id;
				auto& info = it->second.info;

				if (enabled == false)
					info.fState |= MFS_GRAYED;
				else
					info.fState &= ~MFS_GRAYED;

				if (SetMenuItemInfo(fMenu, itemID, FALSE, &info) != 0)
				{
					//error
				}
			
			}
		}
		
	
		
		void AddItem(LLUtils::native_string_type name, const T& data)
		{
			
			MenuItemData menuData;
			static_cast<T&>(menuData) = data;
			
			menuData.id = ++fCurrentItemID;
			
			const auto it = fMapCommandToData.emplace (name, menuData).first;

			MENUITEMINFO& menuItemInfo = it->second.info;
			menuItemInfo = {};

			menuItemInfo.cbSize = sizeof(MENUITEMINFO);
			menuItemInfo.fMask = MIIM_STRING | MIIM_DATA | MIIM_ID | MIIM_STATE;
			menuItemInfo.fState = MFS_ENABLED;
			menuItemInfo.hSubMenu = nullptr;
			menuItemInfo.hbmpChecked = nullptr;
			menuItemInfo.hbmpUnchecked = nullptr;
			menuItemInfo.dwTypeData = const_cast<LLUtils::native_char_type*>(it->first.c_str());
			menuItemInfo.cch = it->first.length();
			menuItemInfo.hbmpItem = nullptr;
			menuItemInfo.wID = fCurrentItemID;

			menuItemInfo.dwItemData = reinterpret_cast<ULONG_PTR>(&(it->second));
			::InsertMenuItem(fMenu, fCurrentItemID, FALSE, &menuItemInfo);
		}

	bool IsVisible() const
		{
			return fVisible;
		}
	private:
		MenuItemData* GetItemByID(uint32_t itemId)
		{
			auto it = std::find_if(std::begin(fMapCommandToData), std::end(fMapCommandToData), [itemId]
			(const decltype(fMapCommandToData)::value_type& pair) -> bool
				{
					return pair.second.id == itemId;
				});

			return it != std::end(fMapCommandToData) ? &it->second : nullptr;
		}
private:
		std::map<std::wstring, MenuItemData> fMapCommandToData;
		HMENU fMenu = nullptr;
		HWND fHandle = nullptr;
		uint16_t fCurrentItemID = 0;
		bool fVisible = false;
	};
}

#pragma once
#include <Windows.h>
#include <string>
#include <map>

#include <LLUtils/StringDefs.h>
#include <LLUtils/BitFlags.h>

namespace OIV
{

	enum class AlignmentHorizontal { None, Left, Center, Right};
	enum class AlignmentVertical { None, Top, Center, Bottom };
	template <typename T>
	class ContextMenu final
	{
	struct MenuItemData
	{
		uint32_t id;
		MENUITEMINFO info;
		T userData;
		LLUtils::native_string_type itemDisplayName;
	};

	struct UserItemData
	{
		T userData;
		LLUtils::native_string_type itemDisplayName;
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

		MenuItemData* Show(int x, int y, AlignmentHorizontal horizontal, AlignmentVertical vertical)
		{
			fVisible = true;

			LLUtils::BitFlags<UINT> flags(TPM_RETURNCMD | TPM_RIGHTBUTTON);
			switch (horizontal)
			{
			case AlignmentHorizontal::Left:
				flags.set(TPM_LEFTALIGN);
					break;
				case AlignmentHorizontal::Center:
					flags.set(TPM_CENTERALIGN);
						break;
				case AlignmentHorizontal::Right:
					flags.set(TPM_RIGHTALIGN);
					break;
				case AlignmentHorizontal::None:
					break;
			}

			switch (vertical)
			{
			case AlignmentVertical::Top:
				flags.set(TPM_TOPALIGN);
					break;
				case AlignmentVertical::Center:
					flags.set(TPM_VCENTERALIGN);
						break;
				case AlignmentVertical::Bottom:
					flags.set(TPM_BOTTOMALIGN);
						break;
				case AlignmentVertical::None:
					break;
			}

			auto itemId = TrackPopupMenu(fMenu, flags, x, y, 0, fHandle, nullptr);
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
			
			menuData.id = ++fCurrentItemID;
			menuData.itemDisplayName = name;
			menuData.userData = data;
			
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
			(const typename decltype(fMapCommandToData)::value_type& pair) -> bool
				{
					return pair.second.id == itemId;
				});

			return it != std::end(fMapCommandToData) ? &it->second : nullptr;
		}
private:
		std::map<std::wstring, MenuItemData> fMapCommandToData;
		HMENU fMenu = nullptr;
		HWND fHandle = nullptr;
		uint32_t fCurrentItemID = 0;
		bool fVisible = false;
	};
}

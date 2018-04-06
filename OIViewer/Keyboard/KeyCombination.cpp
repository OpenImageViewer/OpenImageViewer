#include "KeyCombination.h"
#include <StringUtility.h>
#include "KeyCodeHelper.h"

#ifdef _WIN32
#include <windows.h>
#endif


namespace OIV
{
    ListKeyCombinations  KeyCombination::FromString(const std::string& string)
    {
        using namespace LLUtils;
        std::string upper = StringUtility::ToUpper(string);

        KeyCombination combination;
        using KeyCodeList = std::vector<KeyCode>;
        using KeyCodeListList = std::vector<KeyCodeList>;

        KeyCodeListList duplicateCombinations2Darray;

        ListKeyCombinations bindings;


        ListAString  keyCombination = StringUtility::split(upper, '+');

        for (const std::string& key : keyCombination)
        {
            if (key == "CONTROL")
                duplicateCombinations2Darray.push_back({ KeyCode::LCONTROL, KeyCode::RCONTROL });
            else if (key == "ALT")
                duplicateCombinations2Darray.push_back({ KeyCode::LALT, KeyCode::RALT });

            else if (key == "SHIFT")
                duplicateCombinations2Darray.push_back({ KeyCode::LSHIFT, KeyCode::RSHIFT });

            else if (key == "WINKEY")
                duplicateCombinations2Darray.push_back({ KeyCode::LWIN, KeyCode::RWIN });
            else
                combination.AssignKey(KeyCodeHelper::KeyNameToKeyCode(key));
        }

        if (duplicateCombinations2Darray.empty() == false)
        {
            using namespace std;
            vector<int> sizes;

            for (const KeyCodeList& vec : duplicateCombinations2Darray)
                sizes.push_back(vec.size());

            vector<vector<int>> indices = KeyCodeHelper::ComputeCombinations(sizes);

            for (const vector<int>& currentIndices : indices)
            {
                KeyCombination extraCombination = combination;
                for (int i = 0; i < currentIndices.size(); i++)
                    extraCombination.AssignKey((duplicateCombinations2Darray[i])[currentIndices[i]]);

                bindings.push_back(extraCombination);
            }
        }
        else
        {
            bindings.push_back(combination);
        }

        return bindings;
    }

    std::string KeyCombination::ToString()
    {

        return "";
    }

#ifdef _WIN32

    KeyCombination KeyCombination::FromVirtualKey(uint32_t virtualKey, uint32_t params)
    {
        KeyCombination combination;
        combination.leftAlt = (GetKeyState(VK_LMENU) & static_cast<USHORT>(0x8000)) != 0;
        combination.rightAlt = (GetKeyState(VK_RMENU) & static_cast<USHORT>(0x8000)) != 0;
        combination.leftCtrl = (GetKeyState(VK_LCONTROL) & static_cast<USHORT>(0x8000)) != 0;
        combination.rightCtrl = (GetKeyState(VK_RCONTROL) & static_cast<USHORT>(0x8000)) != 0;
        combination.leftShift = (GetKeyState(VK_LSHIFT) & static_cast<USHORT>(0x8000)) != 0;
        combination.rightShift = (GetKeyState(VK_RSHIFT) & static_cast<USHORT>(0x8000)) != 0;
        combination.leftWinKey = (GetKeyState(VK_LWIN) & static_cast<USHORT>(0x8000)) != 0;
        combination.rightWinKey = (GetKeyState(VK_RWIN) & static_cast<USHORT>(0x8000)) != 0;
        combination.keycode = KeyCodeHelper::KeyCodeFromVK(virtualKey, params);
        return combination;
    }
#endif

    void KeyCombination::AssignKey(KeyCode key)
    {
        switch (key)
        {
        case KeyCode::LALT:
            leftAlt = 1;
            break;
        case KeyCode::RALT:
            rightAlt = 1;
            break;
        case KeyCode::RCONTROL:
            rightCtrl = 1;
            break;
        case KeyCode::LCONTROL:
            leftCtrl = 1;
            break;
        case KeyCode::RSHIFT:
            rightShift = 1;
            break;
        case KeyCode::LSHIFT:
            leftShift = 1;
            break;
        case KeyCode::RWIN:
            rightWinKey = 1;
            break;
        case KeyCode::LWIN:
            leftWinKey = 1;
            break;
        default:
            //Not a modifer - assign key.
            keycode = key;
            break;
        }
    }
}

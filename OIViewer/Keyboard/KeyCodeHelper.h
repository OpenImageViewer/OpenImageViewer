#pragma once
#include <algorithm>
#include <string>
#include "KeyCode.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace OIV
{

    class KeyCodeHelper
    {
    public:

        static std::string KeyCodeToString(OIV::KeyCode keycode)
        {
            auto foundItem = std::find_if(KeyCodeString.begin(), KeyCodeString.end(),
                [&keycode](KeyCodeKeyStringPair const& item)
            {
                return keycode == item.keyCode;
            });
            return foundItem->keyName;
        }

        static KeyCode KeyNameToKeyCode(const std::string& keyName)
        {
            auto foundItem = std::find_if(KeyCodeString.begin(), KeyCodeString.end(),
                [&keyName](KeyCodeKeyStringPair const& item)
            {
                return keyName == item.keyName;
            });

            return foundItem != KeyCodeString.end() ? foundItem->keyCode :KeyCode::UNASSIGNED;
        }

        static std::vector<std::vector<int>> ComputeCombinations(std::vector<int> groupSizes)
        {
            using namespace std;

            vector<vector<int>> result;
            const int totalGroups = groupSizes.size();
            vector<int> accumulativeSize = std::vector<int>(totalGroups);
            int totalElements = 1;
            for (int i = 0; i < groupSizes.size(); i++)
            {
                totalElements *= groupSizes[i];
                accumulativeSize[i] = totalElements;
            }

            for (int e = 0; e < totalElements; e++)
            {
                vector<int> indices = std::vector<int>(totalGroups);

                for (int i = 0; i < indices.size(); i++)
                {
                    int accumulativeFactor = i == 0 ? 1 : accumulativeSize[i - 1];
                    indices[i] = (e / accumulativeFactor) % groupSizes[i];
                }
                result.push_back(indices);

            }
            return result;
        }

        struct KeyEventParams
        {
            unsigned short repeatCount : 16;  // 0 - 15	The repeat count for the current message.The value is the number of times the keystroke is autorepeated as a result of the user holding down the key.If the keystroke is held long enough, multiple messages are sent.However, the repeat count is not cumulative.
            unsigned char scanCode : 8;    //16 - 23	The scan code.The value depends on the OEM.
            bool isExtented : 1;    //24	Indicates whether the key is an extended key, such as the right - hand ALT and CTRL keys that appear on an enhanced 101 - or 102 - key keyboard.The value is 1 if it is an extended key; otherwise, it is 0.
            unsigned char reserved : 4;    //25 - 28	Reserved; do not use.
            unsigned char contextCode : 1;    //29	The context code.The value is always 0 for a WM_KEYDOWN message.
            unsigned char previousKeyState : 1;     //30	The previous key state.The value is 1 if the key is down before the message is sent, or it is zero if the key is up.
            unsigned char transitionstate : 1;    //31	The transition state.The value is always 0 for a WM_KEYDOWN message.
        };

      
        static KeyCode KeyCodeFromVK(uint32_t key, uint32_t params)
        {
            KeyEventParams* keydown = reinterpret_cast<KeyEventParams*>(&params);
            uint16_t scanCode = ((keydown->isExtented == true ? 0xe0 : 0)  << 8) |  MapVirtualKey(key, MAPVK_VK_TO_VSC_EX);
            return   static_cast<KeyCode>(scanCode);
        }
    };
}

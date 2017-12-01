#pragma once
#include <algorithm>
#include <string>
#include "KeyCode.h"

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
    };
}

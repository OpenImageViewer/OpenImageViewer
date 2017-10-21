#pragma once
#include <string>
#include <vector>
#include "KeyCode.h"


namespace OIV
{
    class KeyCombination;
    using ListKeyCombinations = std::vector<KeyCombination> ;
    class KeyCombination
    {
    public:
        struct Hash
        {
            size_t operator()(const KeyCombination& key) const
            {
                return static_cast<size_t>(key.keycode);
            }
        };

        bool operator ==( const KeyCombination& rhs) const
        {
            return keyValue == rhs.keyValue;
        }
        static ListKeyCombinations  FromString(const std::string& string);
        std::string ToString();
#ifdef _WIN32
        static KeyCombination FromVirtualKey(uint32_t key);
#endif
     
      
    private:
        void AssignKey(KeyCode keyCode);

#pragma region memeber fields
        union
        {
            uint16_t keyValue = 0;
            struct
            {
                unsigned keycode : 8;
                unsigned leftCtrl : 1;
                unsigned rightCtrl : 1;
                unsigned leftAlt : 1;
                unsigned rightAlt : 1;
                unsigned leftShift : 1;
                unsigned rightShift : 1;
                unsigned leftWinKey : 1;
                unsigned rightWinKey : 1;
            };
        };

#pragma endregion //memeber fields
    };


}

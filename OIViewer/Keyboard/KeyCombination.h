#pragma once
#include <string>
#include <vector>
#include "KeyCode.h"

#pragma pack(push, 1)
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
                static_assert(sizeof(KeyCombination) <= sizeof(size_t)
                    , "For the benefit of fast hashing the size of key combination must less or equal to the size of size_t");
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
        static KeyCombination FromVirtualKey(uint32_t key,uint32_t params);
#endif
     
      
    private:
        void AssignKey(KeyCode keyCode);

#pragma region memeber fields

        union
        {
            uint32_t keyValue = 0;
            struct
            {
                KeyCode keycode;
                unsigned char leftCtrl : 1;
                unsigned char rightCtrl : 1;
                unsigned char leftAlt : 1;
                unsigned char rightAlt : 1;
                unsigned char leftShift : 1;
                unsigned char rightShift : 1;
                unsigned char leftWinKey : 1;
                unsigned char rightWinKey : 1;
            };
        };

#pragma endregion //memeber fields
    };
}

#pragma pack(pop)
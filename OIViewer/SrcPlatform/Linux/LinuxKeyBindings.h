#pragma once

#include <LWS/KeyCode.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace OIV
{
    struct LinuxKeyModifiers
    {
        bool control{};
        bool shift{};
        bool alt{};
        bool win{};

        bool operator==(const LinuxKeyModifiers&) const = default;
    };

    class LinuxKeyBindings
    {
      public:

        void AddBinding(const std::string& keyCombination, const std::string& commandGroup);
        [[nodiscard]] std::vector<std::string> Resolve(LWS::KeyCode key, const LinuxKeyModifiers& modifiers);

      private:

        struct KeyChord
        {
            LWS::KeyCode key{LWS::KeyCode::Unknown};
            LinuxKeyModifiers modifiers;

            bool operator==(const KeyChord&) const = default;
        };

        struct KeyChordHash
        {
            size_t operator()(const KeyChord& chord) const;
        };

        static KeyChord ParseChord(const std::string& configuredCombination);
        std::unordered_map<KeyChord, std::vector<std::string>, KeyChordHash> fBindings;
    };
}  // namespace OIV

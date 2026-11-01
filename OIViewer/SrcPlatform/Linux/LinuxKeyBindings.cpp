#include "LinuxKeyBindings.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace OIV
{
    namespace
    {
        std::string ToUpper(std::string_view value)
        {
            std::string upper(value);
            std::ranges::transform(upper, upper.begin(),
                                   [](unsigned char character) { return std::toupper(character); });
            return upper;
        }

        std::optional<unsigned int> ParseIndex(std::string_view value, std::string_view prefix, unsigned int maximum)
        {
            if (!value.starts_with(prefix))
                return std::nullopt;

            unsigned int index{};
            const std::string_view number = value.substr(prefix.size());
            const auto [end, error]       = std::from_chars(number.data(), number.data() + number.size(), index);
            if (error != std::errc{} || end != number.data() + number.size() || index > maximum)
                return std::nullopt;
            return index;
        }

        LWS::KeyCode ParseKey(std::string_view name)
        {
            using LWS::KeyCode;
            if (name.size() == 1 && name.front() >= 'A' && name.front() <= 'Z')
                return static_cast<KeyCode>(std::to_underlying(KeyCode::A) + name.front() - 'A');
            if (name.size() == 1 && name.front() >= '0' && name.front() <= '9')
                return static_cast<KeyCode>(std::to_underlying(KeyCode::Digit0) + name.front() - '0');
            if (const auto index = ParseIndex(name, "F", 12); index && *index >= 1)
                return static_cast<KeyCode>(std::to_underlying(KeyCode::F1) + *index - 1);
            if (const auto index = ParseIndex(name, "NUMPAD", 9))
                return static_cast<KeyCode>(std::to_underlying(KeyCode::Numpad0) + *index);

            static const std::unordered_map<std::string_view, KeyCode> namedKeys{
                {"ESCAPE", KeyCode::Escape},
                {"TAB", KeyCode::Tab},
                {"BACK", KeyCode::Backspace},
                {"BACKSPACE", KeyCode::Backspace},
                {"SPACE", KeyCode::Space},
                {"ENTER", KeyCode::Enter},
                {"ENTERMAIN", KeyCode::Enter},
                {"GRAVE", KeyCode::Tilde},
                {"TILDE", KeyCode::Tilde},
                {"COMMA", KeyCode::Comma},
                {"PERIOD", KeyCode::Period},
                {"SLASH", KeyCode::Slash},
                {"SEMICOLON", KeyCode::Semicolon},
                {"APOSTROPHE", KeyCode::Quote},
                {"LBRACKET", KeyCode::LeftBracket},
                {"RBRACKET", KeyCode::RightBracket},
                {"BACKSLASH", KeyCode::Backslash},
                {"MINUS", KeyCode::Minus},
                {"EQUALS", KeyCode::Equals},
                {"ADD", KeyCode::NumpadAdd},
                {"SUBTRACT", KeyCode::NumpadSubtract},
                {"MULTIPLY", KeyCode::NumpadMultiply},
                {"DIVIDE", KeyCode::NumpadDivide},
                {"KEYPADDIVIDE", KeyCode::NumpadDivide},
                {"DECIMAL", KeyCode::NumpadDecimal},
                {"KEYPADENTER", KeyCode::NumpadEnter},
                {"LEFT", KeyCode::Left},
                {"RIGHT", KeyCode::Right},
                {"UP", KeyCode::Up},
                {"DOWN", KeyCode::Down},
                {"HOME", KeyCode::Home},
                {"END", KeyCode::End},
                {"PGUP", KeyCode::PageUp},
                {"PGDOWN", KeyCode::PageDown},
                {"INSERT", KeyCode::Insert},
                {"DELETE", KeyCode::Delete},
                {"GREYLEFT", KeyCode::Left},
                {"GREYRIGHT", KeyCode::Right},
                {"GREYUP", KeyCode::Up},
                {"GREYDOWN", KeyCode::Down},
                {"GREYHOME", KeyCode::Home},
                {"GREYEND", KeyCode::End},
                {"GREYPGUP", KeyCode::PageUp},
                {"GREYPGDN", KeyCode::PageDown},
                {"GREYINSERT", KeyCode::Insert},
                {"GREYDELETE", KeyCode::Delete},
            };
            const auto item = namedKeys.find(name);
            return item != namedKeys.end() ? item->second : KeyCode::Unknown;
        }
    }  // namespace

    size_t LinuxKeyBindings::KeyChordHash::operator()(const KeyChord& chord) const
    {
        size_t hash = std::hash<LWS::KeyCode>{}(chord.key);
        hash        = hash * 31 + chord.modifiers.control;
        hash        = hash * 31 + chord.modifiers.shift;
        hash        = hash * 31 + chord.modifiers.alt;
        return hash * 31 + chord.modifiers.win;
    }

    LinuxKeyBindings::KeyChord LinuxKeyBindings::ParseChord(const std::string& configuredCombination)
    {
        KeyChord chord;
        size_t begin = 0;
        while (begin <= configuredCombination.size())
        {
            const size_t end       = configuredCombination.find('+', begin);
            const std::string part = ToUpper(
                std::string_view(configuredCombination)
                    .substr(begin, end == std::string::npos ? std::string::npos : end - begin));
            if (part == "CONTROL")
                chord.modifiers.control = true;
            else if (part == "SHIFT")
                chord.modifiers.shift = true;
            else if (part == "ALT")
                chord.modifiers.alt = true;
            else if (part == "WINKEY")
                chord.modifiers.win = true;
            else
                chord.key = ParseKey(part);

            if (end == std::string::npos)
                break;
            begin = end + 1;
        }
        if (chord.key == LWS::KeyCode::Unknown)
            throw std::invalid_argument("Unknown Linux key combination: " + configuredCombination);
        return chord;
    }

    void LinuxKeyBindings::AddBinding(const std::string& keyCombination, const std::string& commandGroup)
    {
        fBindings[ParseChord(keyCombination)].push_back(commandGroup);
    }

    std::vector<std::string> LinuxKeyBindings::Resolve(LWS::KeyCode key, const LinuxKeyModifiers& modifiers)
    {
        const auto item = fBindings.find({.key = key, .modifiers = modifiers});
        return item != fBindings.end() ? item->second : std::vector<std::string>{};
    }
}  // namespace OIV

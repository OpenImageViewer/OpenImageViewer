#if defined(OIV_TEST_PLATFORM_LINUX)

    #include <catch2/catch_test_macros.hpp>

    #include <SrcPlatform/Linux/LinuxKeyBindings.h>

    #include <algorithm>

namespace
{
    struct BindingCase
    {
        const char* configuredCombination;
        LWS::KeyCode key;
        OIV::LinuxKeyModifiers modifiers{};
    };
}  // namespace

TEST_CASE("Linux resolves configured key names from LWS events", "[oiviewer][platform][linux][keys]")
{
    const std::vector<BindingCase> cases{
        {"F1", LWS::KeyCode::F1},
        {"1", LWS::KeyCode::Digit1},
        {"Grave", LWS::KeyCode::Tilde},
        {"Shift+Grave", LWS::KeyCode::Tilde, {.shift = true}},
        {"Control+Shift+O", LWS::KeyCode::O, {.control = true, .shift = true}},
        {"Alt+Enter", LWS::KeyCode::Enter, {.alt = true}},
        {"Shift+Escape", LWS::KeyCode::Escape, {.shift = true}},
        {"RBracket", LWS::KeyCode::RightBracket},
        {"Numpad8", LWS::KeyCode::Numpad8},
        {"Add", LWS::KeyCode::NumpadAdd},
        {"KeyPadDivide", LWS::KeyCode::NumpadDivide},
        {"GREYDELETE", LWS::KeyCode::Delete},
        {"GREYRIGHT", LWS::KeyCode::Right},
        {"GREYPGDN", LWS::KeyCode::PageDown},
    };

    OIV::LinuxKeyBindings bindings;
    for (const BindingCase& item : cases)
        bindings.AddBinding(item.configuredCombination, item.configuredCombination);

    for (const BindingCase& item : cases)
    {
        CAPTURE(item.configuredCombination);
        const std::vector<std::string> commands = bindings.Resolve(item.key, item.modifiers);
        REQUIRE(std::ranges::find(commands, item.configuredCombination) != commands.end());
    }
}

TEST_CASE("Linux key bindings require exact modifiers and retain duplicate commands",
          "[oiviewer][platform][linux][keys]")
{
    OIV::LinuxKeyBindings bindings;
    bindings.AddBinding("Control+O", "open-first");
    bindings.AddBinding("Control+O", "open-second");

    REQUIRE(bindings.Resolve(LWS::KeyCode::O, {}).empty());
    REQUIRE(bindings.Resolve(LWS::KeyCode::O, {.control = true, .shift = true}).empty());
    REQUIRE(bindings.Resolve(LWS::KeyCode::Unknown, {.control = true}).empty());
    REQUIRE(bindings.Resolve(LWS::KeyCode::O, {.control = true}) ==
            std::vector<std::string>{"open-first", "open-second"});
}

#endif

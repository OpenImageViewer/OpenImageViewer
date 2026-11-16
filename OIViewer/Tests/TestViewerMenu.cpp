#ifdef OIV_TEST_HAS_SETTINGS_EDITOR
    #include <catch2/catch_test_macros.hpp>
    #include "UI/InterfacePreferences.h"
    #include "UI/ViewerMenu.h"
    #include <nlohmann/json.hpp>
    #include <algorithm>
    #include <tuple>
    #include <chrono>
    #include <fstream>

TEST_CASE("Interface preferences preserve unrelated settings and reject corrupt profiles",
          "[oiviewer][menus][settings]")
{
    const auto folder = std::filesystem::temp_directory_path() /
                        ("oiv-interface-" +
                         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(folder);
    struct Cleanup
    {
        std::filesystem::path path;
        ~Cleanup() { std::filesystem::remove_all(path); }
    } cleanup{folder};
    const auto path = folder / "Settings.json";
    REQUIRE(OIV::LoadInterfacePreferences(path));
    CHECK(*OIV::LoadInterfacePreferences(path) == OIV::InterfacePreferences{});
    REQUIRE(OIV::SaveInterfacePreferences(path, {}));
    {
        std::ofstream file(path);
        file << R"({"unrelated":42,"viewsettings":{"other":true,"interface":{"showcaption":false}}})";
    }
    for (unsigned mask = 0; mask < 16; ++mask)
    {
        const OIV::InterfacePreferences preferences{.borders   = bool(mask & 1),
                                                    .menuBar   = bool(mask & 2),
                                                    .statusBar = bool(mask & 4),
                                                    .caption   = bool(mask & 8)};
        REQUIRE(OIV::SaveInterfacePreferences(path, preferences));
        REQUIRE(OIV::LoadInterfacePreferences(path));
        CHECK(*OIV::LoadInterfacePreferences(path) == preferences);
        std::ifstream file(path);
        const auto json = nlohmann::json::parse(file);
        CHECK(json["unrelated"] == 42);
        CHECK(json["viewsettings"]["other"] == true);
        CHECK(json["viewsettings"]["interface"]["showcaption"] == preferences.caption);
    }
    {
        std::ofstream file(path);
        file << "invalid";
    }
    CHECK_FALSE(OIV::LoadInterfacePreferences(path));
    CHECK_FALSE(OIV::SaveInterfacePreferences(path, {}));
    std::ifstream file(path);
    CHECK(std::string(std::istreambuf_iterator<char>(file), {}) == "invalid");
}

TEST_CASE("Viewer menus use configured commands and shortcut labels", "[oiviewer][menus]")
{
    const OIV::ConfigurationLoader::CommandGroupList commands{{"OpenFile", "Open", "cmd_open", ""},
                                                              {"ShowResizeForm", "Resize", "cmd_resize", ""},
                                                              {"ToggleWindowBorders", "Borders", "cmd_toggle", ""}};
    const OIV::ConfigurationLoader::KeyBindingList bindings{{"Ctrl+J", "OpenFile"},
                                                            {"Ctrl+J", "OpenFile"},
                                                            {"Alt+O", "OpenFile"}};
    std::string executed;
    OIV::ViewerMenuState state;
    auto menu = OIV::BuildViewerMenu(state, commands, bindings, [&](std::string id) { executed = id; });
    REQUIRE(menu.size() == 5);
    CHECK(menu[0].label == "&File");
    CHECK(menu[1].label == "&Edit");
    CHECK(menu[2].label == "&Image");
    CHECK(menu[3].label == "&View");
    CHECK(menu[4].label == "&Help");
    CHECK(menu[0].items[0].shortcut == "Ctrl+J, Alt+O");
    CHECK(menu[0].items[0].enabled);
    menu[0].items[0].action();
    CHECK(executed == "OpenFile");
    CHECK_FALSE(menu[0].items[1].enabled);
    CHECK_FALSE(menu[2].items[0].enabled);
    state.image = state.canResize = true;
    menu                          = OIV::BuildViewerMenu(state, commands, {}, [](std::string) {});
    CHECK(menu[2].items[0].enabled);
    CHECK(menu[0].items[0].shortcut.empty());
    menu = OIV::BuildViewerMenu(state, commands, bindings, [](std::string) {}, [](const auto&) { return false; });
    CHECK_FALSE(menu[0].items[0].enabled);
}
TEST_CASE("Interface menu commands use configured view commands and bindings", "[oiviewer][menus]")
{
    const auto commands = OIV::ConfigurationLoader::LoadCommandGroups(OIV_TEST_CONFIGURATION_DIR);
    const auto bindings = OIV::ConfigurationLoader::LoadKeyBindings(OIV_TEST_CONFIGURATION_DIR);
    OIV::ViewerMenuState state;
    state.subImages    = true;
    const auto menu    = OIV::BuildViewerMenu(state, commands, bindings, [](std::string) {});
    const auto& items  = menu[3].items;
    const auto section = std::ranges::find(items, "&Interface", &LWSUI::MenuItem::label);
    REQUIRE(section != items.end());
    for (const auto& [label, group, shortcut] :
         {std::tuple{"&Borders", "ToggleWindowBorders", "B"}, std::tuple{"&Menu Bar", "ToggleMenuBar", "Shift+F10"},
          std::tuple{"&Caption", "ToggleCaption", "Shift+F11"},
          std::tuple{"Sub-image &Panel", "ToggleSubImagePanel", "Shift+Tab"}})
    {
        const auto item = std::ranges::find(section->submenu, label, &LWSUI::MenuItem::label);
        REQUIRE(item != section->submenu.end());
        CHECK(item->enabled);
        CHECK(item->shortcut == shortcut);
        const auto command = std::ranges::find(commands, group,
                                               &OIV::ConfigurationLoader::CommandGroup::commandGroupID);
        REQUIRE(command != commands.end());
        CHECK(command->commandName == "cmd_view_state");
    }
}
#endif

#ifdef OIV_TEST_HAS_SETTINGS_EDITOR
    #include <LWSSettings/SettingsStorage.hpp>
    #include <LWSSettings/SettingsEditor.hpp>
    #include <LWSSettings/SettingsView.hpp>
    #include <nlohmann/json.hpp>
    #include <catch2/catch_test_macros.hpp>
    #include <catch2/catch_approx.hpp>
    #include <array>
    #include "../Source/UI/FileAssociationPanel.h"
    #include <OIVAppCore/FileLock.h>
    #include <chrono>
    #include <thread>
    #include <stdexcept>
    #include <LWSUI/UIHost.hpp>
    #ifdef _WIN32
        #include <LWS/Win32/Platform.hpp>
    #endif

namespace
{
    using namespace LWSSettings;
    nlohmann::json CustomTemplate()
    {
        return {{"version", 1},
                {"children",
                 {{{"type", "group"},
                   {"id", "filesystem"},
                   {"label", "Filesystem"},
                   {"children",
                    {{{"type", "checkbox"}, {"id", "enabled"}, {"label", "Enabled"}, {"default", false}},
                     {{"type", "custom"},
                      {"id", "associations"},
                      {"label", "File associations"},
                      {"control", "file-associations"}}}}}}}};
    }
    void Apply(SettingsView& view, bool save = false)
    {
        REQUIRE(view.PreviewInput(
            {.kind = LWSUI::InputKind::KeyDown, .key = save ? LWS::KeyCode::S : LWS::KeyCode::Enter, .control = true}));
    }
}  // namespace
TEST_CASE("Custom placeholders have identity but no persistent value", "[oiviewer][settings][custom]")
{
    SettingsDocument document;
    REQUIRE(SettingsStorage::LoadTemplate(document, CustomTemplate()));
    REQUIRE(document.WorkingValues().size() == 1);
    CHECK_FALSE(document.Find("associations"));
    CHECK_FALSE(SettingsStorage::EncodeValues(document.WorkingValues())["values"].contains("associations"));
    REQUIRE(document.SetValue("enabled", true));
    REQUIRE(document.Reset("filesystem"));
    CHECK_FALSE(std::get<bool>(*document.WorkingValue("enabled")));
    auto invalid                                = CustomTemplate();
    invalid["children"][0]["children"][1]["id"] = "enabled";
    CHECK_FALSE(SettingsStorage::LoadTemplate(document, invalid));
    invalid = CustomTemplate();
    invalid["children"][0]["children"][1].erase("control");
    CHECK_FALSE(SettingsStorage::LoadTemplate(document, invalid));
    CHECK(document.WorkingValues().size() == 1);
}
TEST_CASE("Custom completion follows successful settings and gates reentry", "[oiviewer][settings][custom]")
{
    bool failSave   = true;
    unsigned writes = 0, calls = 0, discards = 0;
    DocumentConfig config;
    config.valuesFile  = "unused.json";
    config.writeValues = [&](const auto&, const auto&) -> Status
    {
        ++writes;
        return failSave ? std::unexpected(Error{"test", "write failed"}) : Status{};
    };
    SettingsDocument document(config);
    REQUIRE(SettingsStorage::LoadTemplate(document, CustomTemplate()));
    CustomCompletion pending;
    auto view = std::make_unique<SettingsView>(document, "Test", SettingsTheme{}, false, false,
                                               [&](const CustomDefinition&) -> CustomContent
                                               {
                                                   return {std::make_unique<LWSUI::Label>("Custom panel"),
                                                           [&](auto operation, auto complete)
                                                           {
                                                               if (operation == CustomOperation::Discard)
                                                               {
                                                                   ++discards;
                                                                   complete({});
                                                               }
                                                               else
                                                               {
                                                                   ++calls;
                                                                   pending = std::move(complete);
                                                               }
                                                           }};
                                               });
    REQUIRE(document.SetValue("enabled", true));
    Apply(*view, true);
    CHECK(writes == 1);
    CHECK(calls == 0);
    failSave = false;
    Apply(*view, true);
    CHECK(writes == 2);
    REQUIRE(calls == 1);
    CHECK_FALSE(view->PrepareClose());
    Apply(*view);
    CHECK(calls == 1);
    auto duplicate = pending;
    pending(std::unexpected(Error{"associations", "partial failure"}));
    CHECK(view->PrepareClose());
    CHECK_FALSE(document.IsDirty());
    CHECK(std::get<bool>(*document.WorkingValue("enabled")));
    duplicate({});
    Apply(*view);
    REQUIRE(calls == 2);
    pending({});
    CHECK(view->PrepareClose());
    CHECK(view->DiscardForClose({}));
    CHECK(discards == 1);
    Apply(*view);
    REQUIRE(calls == 3);
    view.reset();
    CHECK_NOTHROW(pending({}));
}
TEST_CASE("Custom operation chains reject callbacks from completed steps", "[oiviewer][settings][custom]")
{
    auto schema = CustomTemplate();
    schema["children"][0]["children"].push_back(
        {{"type", "custom"}, {"id", "second"}, {"label", "Second"}, {"control", "second"}});
    SettingsDocument document;
    REQUIRE(SettingsStorage::LoadTemplate(document, schema));
    std::array<CustomCompletion, 2> complete;
    std::array<unsigned, 2> calls{};
    SettingsView view(document, "Test", {}, false, false,
                      [&](const CustomDefinition& definition) -> CustomContent
                      {
                          const size_t index = definition.id == "second" ? 1 : 0;
                          return {std::make_unique<LWSUI::Label>(definition.label), [&, index](auto, auto callback)
                                  {
                                      ++calls[index];
                                      complete[index] = std::move(callback);
                                  }};
                      });
    Apply(view);
    REQUIRE(calls[0] == 1);
    CHECK(calls[1] == 0);
    complete[0]({});
    REQUIRE(calls[1] == 1);
    complete[0](std::unexpected(Error{"late", "Duplicate must not finish the next step"}));
    CHECK_FALSE(view.PrepareClose());
    complete[1](std::unexpected(Error{"test", "Second step failed"}));
    CHECK(view.PrepareClose());
    Apply(view);
    CHECK(calls[0] == 2);
    complete[0]({});
    REQUIRE(calls[1] == 2);
    complete[1]({});
    CHECK(view.PrepareClose());
}
TEST_CASE("A throwing custom operation stops the chain and permits retry", "[oiviewer][settings][custom]")
{
    auto schema = CustomTemplate();
    schema["children"][0]["children"].push_back(
        {{"type", "custom"}, {"id", "second"}, {"label", "Second"}, {"control", "second"}});
    SettingsDocument document;
    REQUIRE(SettingsStorage::LoadTemplate(document, schema));
    bool fail          = true;
    unsigned completed = 0;
    SettingsView view(document, "Test", {}, false, false,
                      [&](const CustomDefinition& definition) -> CustomContent
                      {
                          return {std::make_unique<LWSUI::Label>(definition.label),
                                  [&, first = definition.id == "associations"](auto, auto complete)
                                  {
                                      if (first && fail)
                                          throw std::runtime_error("System operation failed");
                                      if (!first)
                                          ++completed;
                                      complete({});
                                  }};
                      });
    Apply(view);
    CHECK(completed == 0);
    CHECK(view.PrepareClose());
    fail = false;
    Apply(view);
    CHECK(completed == 1);
    CHECK(view.PrepareClose());
}
TEST_CASE("Custom content participates in metadata filtering only", "[oiviewer][settings][custom]")
{
    SettingsDocument document;
    REQUIRE(SettingsStorage::LoadTemplate(document, CustomTemplate()));
    LWSUI::Control* content{};
    SettingsView view(document, "Test", {}, false, false,
                      [&](const auto&) -> CustomContent
                      {
                          auto label = std::make_unique<LWSUI::Label>("Runtime content");
                          content    = label.get();
                          return {std::move(label), {}};
                      });
    REQUIRE(content);
    view.SetFilter("associations");
    CHECK(content->Parent()->Visible());
    view.SetFilter("unrelated");
    CHECK_FALSE(content->Parent()->Visible());
    view.SetFilter("");
    view.SetValueFilter(SettingValueFilter::Changed);
    CHECK_FALSE(content->Parent()->Visible());
    view.SetValueFilter(SettingValueFilter::All);
    CHECK(content->Parent()->Visible());
}
TEST_CASE("Settings navigation reveals ordinary and custom rows while preserving staged edits",
          "[oiviewer][settings][custom][navigation]")
{
    auto source    = CustomTemplate();
    auto& children = source["children"][0]["children"];
    for (int i = 0; i < 20; ++i)
        children.insert(children.begin() + 1, nlohmann::json{{"type", "checkbox"},
                                                             {"id", "padding" + std::to_string(i)},
                                                             {"label", "Padding"},
                                                             {"default", false}});
    auto custom     = children.back();
    children.back() = {{"type", "group"},
                       {"id", "nested"},
                       {"label", "Nested"},
                       {"children", nlohmann::json::array({custom})}};
    DocumentConfig config;
    config.applyPolicy = ApplyPolicy::OnApply;
    SettingsDocument document(config);
    REQUIRE(SettingsStorage::LoadTemplate(document, source));
    REQUIRE(document.SetValue("enabled", true));
    LWSUI::Control* content{};
    SettingsView view(document, "Test", {}, false, true,
                      [&](const auto&) -> CustomContent
                      {
                          auto label = std::make_unique<LWSUI::Label>("Runtime content");
                          content    = label.get();
                          return {std::move(label), {}};
                      });
    REQUIRE(content);
    const auto arrange = [&]
    {
        view.Measure({640, 440});
        view.Arrange({0, 0, 640, 440});
    };
    arrange();
    view.SetFilter("missing");
    view.SetValueFilter(SettingValueFilter::Changed);
    REQUIRE_FALSE(view.RevealSetting("unknown"));
    CHECK(view.GetFilter() == "missing");
    CHECK(view.GetValueFilter() == SettingValueFilter::Changed);
    REQUIRE(view.RevealSetting("associations"));
    arrange();
    CHECK(view.GetFilter().empty());
    CHECK(view.GetValueFilter() == SettingValueFilter::All);
    auto* row = content->Parent();
    CHECK(row->Visible());
    LWSUI::ScrollView* scroll{};
    unsigned expanded = 0;
    for (auto* parent = row->Parent(); parent; parent = parent->Parent())
    {
        if (auto* branch = dynamic_cast<LWSUI::TreeView::Branch*>(parent))
        {
            CHECK(branch->Expanded());
            ++expanded;
        }
        if (auto* candidate = dynamic_cast<LWSUI::ScrollView*>(parent))
            scroll = candidate;
    }
    CHECK(expanded == 2);
    REQUIRE(scroll);
    CHECK(scroll->Offset() > 0);
    CHECK(row->Bounds().y >= scroll->Viewport().y);
    CHECK(row->Bounds().y < scroll->Viewport().y + scroll->Viewport().height);
    REQUIRE(view.RevealSetting("enabled"));
    arrange();
    CHECK(scroll->Offset() < 40);
    CHECK(document.WorkingValue("enabled") == Value{true});
    CHECK(document.AppliedValue("enabled") == Value{false});
    CHECK(document.HasPendingChanges());
}
TEST_CASE("Nested custom rows preserve tree indentation and compact editor width", "[oiviewer][settings][custom]")
{
    LWSUI::TreeView tree;
    const LWSSettings::SettingsTheme theme;
    tree.SetStyle(theme.tree);
    auto& group          = tree.Emplace<LWSUI::TreeView::Branch>("Filesystem");
    auto& ordinary       = group.Content().Emplace<LWSUI::TreeView::Row>(std::make_unique<LWSUI::Label>("Ordinary"),
                                                                         std::make_unique<LWSUI::Label>("Value"));
    auto& nested         = group.Content().Emplace<LWSUI::TreeView::Branch>("File associations");
    auto& nestedOrdinary = nested.Content().Emplace<LWSUI::TreeView::Row>(std::make_unique<LWSUI::Label>("Nested"),
                                                                          std::make_unique<LWSUI::Label>("Value"));
    auto editor          = std::make_unique<LWSUI::Label>("Extension lists");
    editor->SetMaxWidth(368);
    auto& custom   = nested.Content().Emplace<LWSUI::TreeView::Row>(std::make_unique<LWSUI::Label>("Extensions"),
                                                                    std::move(editor),
                                                                    std::make_unique<LWSUI::Button>("Action"));
    custom.stacked = true;
    for (float width : {480.f, 640.f})
    {
        tree.Measure({width, 480});
        tree.Arrange({0, 0, width, tree.DesiredSize().height});
        CHECK(nestedOrdinary.NameControl().Bounds().x - ordinary.NameControl().Bounds().x ==
              Catch::Approx(theme.tree.treeIndent));
        CHECK(nestedOrdinary.EditorControl().Bounds().x == ordinary.EditorControl().Bounds().x);
        CHECK(custom.NameControl().Bounds().x == nestedOrdinary.NameControl().Bounds().x);
        CHECK(custom.EditorControl().Bounds().x == custom.NameControl().Bounds().x + theme.tree.labelPadding);
        CHECK(custom.EditorControl().Bounds().y > custom.NameControl().Bounds().y);
        CHECK(custom.ActionControl()->Bounds().y + custom.ActionControl()->Bounds().height / 2 ==
              Catch::Approx(custom.NameControl().Bounds().y + custom.NameControl().Bounds().height / 2));
        CHECK(custom.EditorControl().Bounds().width <= 368);
        CHECK(custom.EditorControl().Bounds().x + custom.EditorControl().Bounds().width <= width);
    }
}

    #ifdef _WIN32
TEST_CASE("Association editor queries after the opening prompt releases its lock",
          "[oiviewer][settings][custom][windows]")
{
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    const auto directory = std::filesystem::temp_directory_path() /
                           ("oiv-selective-" +
                            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    {
        OIV::FileAssociationModel model;
        OIV::FileLock prompt(directory / "associations.lock");
        REQUIRE(prompt.Lock(true, false));
        EditorConfig config;
        config.customFactory = [&](const auto&)
        { return OIV::MakeFileAssociationPanel(platform, directory, {"oiv-selective-test"}, model); };
        SettingsEditor editor(platform, std::move(config));
        REQUIRE(SettingsStorage::LoadTemplate(editor.Document(), CustomTemplate()));
        REQUIRE(editor.Show());
        CHECK(model.Entries().empty());
        // Simulate Respond followed by session destruction before returning to event dispatch.
        prompt.Unlock();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (model.Entries().empty() && std::chrono::steady_clock::now() < deadline)
        {
            REQUIRE(platform.ProcessMessages() == LWS::LoopResult::Continue);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        REQUIRE(model.Entries().size() == 1);
        CHECK(model.Entries().front().extension == "oiv-selective-test");
    }
    std::filesystem::remove(directory / "associations.lock");
    std::filesystem::remove(directory);
    CHECK(platform.Shutdown() == LWS::Result::Success);
}
TEST_CASE("Hosted custom operations block closing and deliver completion safely",
          "[oiviewer][settings][custom][windows]")
{
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    {
        CustomCompletion pending;
        unsigned applied = 0, discarded = 0;
        EditorConfig config;
        config.revertOnClose = true;
        config.customFactory = [&](const auto&) -> CustomContent
        {
            return {std::make_unique<LWSUI::Label>("System panel"), [&](auto operation, auto complete)
                    {
                        if (operation == CustomOperation::Discard)
                        {
                            ++discarded;
                            complete({});
                        }
                        else
                        {
                            ++applied;
                            pending = std::move(complete);
                        }
                    }};
        };
        SettingsEditor editor(platform, std::move(config));
        REQUIRE(SettingsStorage::LoadTemplate(editor.Document(), CustomTemplate()));
        REQUIRE(editor.Show());
        Apply(*editor.View());
        CHECK(applied == 1);
        CHECK_FALSE(editor.Hide());
        CHECK(editor.IsVisible());
        pending(std::unexpected(Error{"test", "system rejected one extension"}));
        editor.View()->Host()->Update();
        CHECK(editor.IsVisible());
        Apply(*editor.View());
        CHECK(applied == 2);
        pending({});
        editor.View()->Host()->Update();
        REQUIRE(editor.Hide());
        CHECK_FALSE(editor.IsVisible());
        CHECK(discarded == 1);
    }
    CHECK(platform.Shutdown() == LWS::Result::Success);
}
    #endif

#endif

#include <catch2/catch_test_macros.hpp>
#include <OIVAppCore/ViewerHostPolicy.h>
#include <OIVAppCore/CommandController.h>

TEST_CASE("Hosted command policy restricts application actions while preserving content commands", "[appcore][wlx]")
{
    const auto allowed = [](const char* command, const char* arguments = "")
    { return OIV::IsHostedCommandAllowed({"", command, OIV::CommandManager::CommandArgs::FromString(arguments)}); };
    CHECK_FALSE(allowed("cmd_file_associations", "action=add"));
    CHECK_FALSE(allowed("cmd_file_associations", "action=remove"));
    CHECK_FALSE(allowed("cmd_set_window_size", "mode=maximized"));
    CHECK_FALSE(allowed("cmd_shell", "cmd=newWindow"));
    CHECK_FALSE(allowed("cmd_view_state", "type=toggleBorders"));
    CHECK_FALSE(allowed("cmd_toggle_keybindings", "type=settings"));
    CHECK(allowed("cmd_toggle_keybindings", "type=imageinfo"));
    CHECK(allowed("cmd_view_state", "type=toggleFullScreen"));
    CHECK(allowed("cmd_view_state", "type=quit"));
    CHECK(allowed("cmd_navigate", "direction=next"));
    CHECK(allowed("cmd_shell", "cmd=containingFolder"));
    CHECK(allowed("cmd_zoom", "steps=1"));
    CHECK(allowed("cmd_copyToClipboard"));
}

TEST_CASE("Command admission covers every entry point without leaking blocked bindings", "[appcore][wlx]")
{
    OIV::CommandController controller;
    unsigned calls{};
    controller.AddCommandCallbacks({{"cmd_file_associations", [&](const auto&, auto&) { ++calls; }},
                                    {"cmd_zoom", [&](const auto&, auto&) { ++calls; }}});
    controller.GetCommandManager().AddCommandGroup({"blocked", "", "cmd_file_associations", "action=add"});
    controller.SetRequestPredicate(OIV::IsHostedCommandAllowed);
    CHECK(controller.DispatchCommand({"", "missing", {}}) == OIV::CommandController::DispatchResult::Unknown);
    CHECK(controller.DispatchCommand({"", "cmd_file_associations", {}}) ==
          OIV::CommandController::DispatchResult::Blocked);
    CHECK(controller.ExecutePredefinedCommand("blocked"));
    CHECK(controller.ExecuteCommandInternal("cmd_file_associations", "action=add"));
    CHECK(calls == 0);
    CHECK(controller.ExecuteCommand({"", "cmd_zoom", {}}));
    CHECK(calls == 1);
    controller.SetRequestPredicate(nullptr);
    CHECK(controller.ExecutePredefinedCommand("blocked"));
    CHECK(calls == 2);
}

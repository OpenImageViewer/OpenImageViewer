#if defined(OIV_TEST_HAS_SETTINGS_EDITOR) && defined(_WIN32)
    #include <catch2/catch_test_macros.hpp>
    #include "MainWindow.h"
    #include "UI/AboutForm.h"
    #include "UI/ViewerMenu.h"
    #include <LWS/Platform.hpp>
    #include <LWS/Win32/Platform.hpp>
    #include <LWS/Win32/WindowExtensions.hpp>
    #include <Windows.h>
    #include <tuple>

TEST_CASE("Canvas layout follows the child's new monitor DPI", "[oiviewer][chrome][dpi]")
{
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    for (bool sidebar : {false, true})
    {
        CAPTURE(sidebar);
        OIV::MainWindow window(platform, {});
        REQUIRE(window.Create({.clientSize = {640, 480}}) == LWS::Result::Success);
        window.ShowStatusBar(false);
        window.SetShowImageControl(sidebar);
        const auto main      = *LWS::Win32::GetHwnd(window.GetWindow());
        const auto canvas    = *LWS::Win32::GetHwnd(window.GetCanvasWindow());
        const auto targetDpi = *LWS::Win32::GetDpi(window.GetWindow());
        for (const UINT previousDpi : {targetDpi / 2, targetDpi * 2})
        {
            RECT suggested{};
            REQUIRE(GetWindowRect(main, &suggested));
            suggested.right += 64;
            // Seed a stale child cache, then exercise the actual before/parent/after phases.
            SendMessageW(canvas, WM_DPICHANGED, MAKELONG(previousDpi, previousDpi),
                         reinterpret_cast<LPARAM>(&suggested));
            SendMessageW(canvas, WM_DPICHANGED_BEFOREPARENT, 0, 0);
            SendMessageW(main, WM_DPICHANGED, MAKELONG(targetDpi, targetDpi), reinterpret_cast<LPARAM>(&suggested));
            SendMessageW(canvas, WM_DPICHANGED_AFTERPARENT, 0, 0);
            const auto parentSize = window.GetWindow().GetClientAreaMetrics().logical;
            const auto childSize  = window.GetCanvasWindow().GetClientAreaMetrics();
            REQUIRE(childSize.pixels);
            CHECK(childSize.logical.x == parentSize.x - (sidebar ? 165 : 0));
            CHECK(childSize.pixels->x == MulDiv(childSize.logical.x, targetDpi, 96));
            CHECK(childSize.pixels->y == MulDiv(parentSize.y - 32, targetDpi, 96));
        }
        REQUIRE(window.Destroy() == LWS::Result::Success);
    }
}

TEST_CASE("Viewer title replaces the native caption without covering the canvas", "[oiviewer][chrome]")
{
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    OIV::MainWindow window(platform, {});
    unsigned closeRequests{};
    window.requestClose = [&] { ++closeRequests; };
    REQUIRE(window.Create({.clientSize = LWS::LogicalSize{640, 480},
                           .styles     = LWS::WindowStyleFlags(LWS::WindowStyle::ResizableBorder)}) ==
            LWS::Result::Success);
    const HWND main   = *LWS::Win32::GetHwnd(window.GetWindow());
    const HWND canvas = *LWS::Win32::GetHwnd(window.GetCanvasWindow());
    const auto style  = GetWindowLongPtrW(main, GWL_STYLE);
    CHECK((style & WS_CAPTION) == 0);
    CHECK((style & WS_THICKFRAME) != 0);
    CHECK(GetParent(canvas) == main);
    POINT imagePoint{100, 100};
    REQUIRE(ClientToScreen(canvas, &imagePoint));
    CHECK(SendMessageW(canvas, WM_NCHITTEST, 0, MAKELPARAM(imagePoint.x, imagePoint.y)) == HTCLIENT);
    // Right presses must reach the canvas listener rather than the parent's menu host.
    unsigned rightPresses{};
    auto input = window.GetCanvasWindow().Listen(
        [&](const LWS::AnyEvent& event)
        {
            if (const auto* button = std::get_if<LWS::EventMouseButton>(&event);
                button && button->button == LWS::MouseButton::Right && button->pressed)
                ++rightPresses;
            return LWS::EventResponse::Unhandled;
        });
    REQUIRE(input);
    SendMessageW(canvas, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(100, 100));
    SendMessageW(canvas, WM_RBUTTONUP, 0, MAKELPARAM(100, 100));
    CHECK(rightPresses == 1);

    window.SetTitle(L"Image - OIViewer");
    CHECK(window.GetWindow().GetTitle() == L"Image - OIViewer");
    auto canvasTop = [&]
    {
        POINT point{};
        MapWindowPoints(canvas, main, &point, 1);
        return point.y;
    };
    CHECK(canvasTop() > 0);
    for (unsigned mask = 0; mask < 8; ++mask)
    {
        window.ApplyInterfacePreferences(
            {.borders = bool(mask & 1), .menuBar = bool(mask & 2), .caption = bool(mask & 4)});
        CHECK((canvasTop() > 0) == bool(mask & 4));
        CHECK((GetWindowLongPtrW(main, GWL_STYLE) & WS_THICKFRAME) != 0);
        RECT outer{};
        REQUIRE(GetWindowRect(main, &outer));
        POINT origin{};
        REQUIRE(ClientToScreen(main, &origin));
        CHECK((origin.x > outer.left) == bool(mask & 1));
        CHECK((GetWindowLongPtrW(main, GWL_STYLE) & WS_CAPTION) == 0);
    }
    window.ApplyInterfacePreferences({});
    REQUIRE(window.SetWindowMode(LWS::WindowMode::Fullscreen) == LWS::Result::Success);
    CHECK(canvasTop() == 0);
    REQUIRE(window.SetWindowMode(LWS::WindowMode::Windowed) == LWS::Result::Success);
    CHECK(canvasTop() > 0);
    window.SetMenuProvider([] { return std::vector<LWSUI::MenuBarItem>{{"&File", {{.label = "&Open"}}}}; });
    unsigned restored{};
    window.restoreMenu = [&]
    {
        ++restored;
        window.ApplyInterfacePreferences({});
    };
    window.ApplyInterfacePreferences({.menuBar = false, .caption = false});
    CHECK(canvasTop() == 0);
    SendMessageW(main, WM_SYSKEYDOWN, VK_MENU, 0);
    SendMessageW(main, WM_SYSKEYUP, VK_MENU, 0);
    CHECK(restored == 0);
    CHECK(canvasTop() == 0);
    SendMessageW(main, WM_KEYDOWN, VK_F10, 0);
    SendMessageW(main, WM_KEYUP, VK_F10, 0);
    CHECK(restored == 1);
    CHECK(canvasTop() > 0);
    CHECK(window.MenuOwnsKeyboard());
    CHECK(window.DeferShortcutToMenu(LWS::KeyCode::Escape, false, false, false));
    CHECK_FALSE(window.DeferShortcutToMenu(LWS::KeyCode::O, true, false, false));
    CHECK_FALSE(window.DeferShortcutToMenu(LWS::KeyCode::F10, false, false, true));
    CHECK_FALSE(window.DeferShortcutToMenu(LWS::KeyCode::F11, false, false, true));
    SendMessageW(main, WM_KEYDOWN, VK_ESCAPE, 0);
    SendMessageW(main, WM_KEYUP, VK_ESCAPE, 0);
    CHECK_FALSE(window.MenuOwnsKeyboard());
    // The title buttons use client coordinates and defer actions to the normal UI loop.
    auto clickTitleButton = [&](int fromRight)
    {
        std::ignore = platform.ProcessMessages();
        RECT client{};
        GetClientRect(main, &client);
        const double scale   = window.GetWindow().GetClientAreaMetrics().Scale().value_or(LWS::ContentScale{}).x;
        const int halfButton = static_cast<int>(16 * scale);
        const auto point     = MAKELPARAM(client.right - (2 * fromRight + 1) * halfButton, halfButton);
        SendMessageW(main, WM_LBUTTONDOWN, MK_LBUTTON, point);
        SendMessageW(main, WM_LBUTTONUP, 0, point);
        std::ignore = platform.ProcessMessages();
    };
    clickTitleButton(1);
    CHECK(window.GetWindow().GetShowState() == LWS::WindowShowState::Maximized);
    clickTitleButton(1);
    CHECK(window.GetWindow().GetShowState() == LWS::WindowShowState::Restored);
    clickTitleButton(0);
    CHECK(closeRequests == 1);
    CHECK(window.GetWindow().IsCreated());
    REQUIRE(window.Destroy() == LWS::Result::Success);
}
TEST_CASE("View dropdown shows all rows with compact spacing", "[oiviewer][menus][chrome]")
{
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    OIV::MainWindow window(platform, {});
    REQUIRE(window.Create({.clientSize = {640, 480}, .visible = true}) == LWS::Result::Success);
    const auto commands = OIV::ConfigurationLoader::LoadCommandGroups(OIV_TEST_CONFIGURATION_DIR);
    const auto menu     = OIV::BuildViewerMenu({}, commands, {}, [](std::string) {});
    REQUIRE(menu.size() > 3);
    const auto view = menu[3];
    window.SetMenuProvider([view] { return std::vector<LWSUI::MenuBarItem>{view}; });
    std::ignore     = platform.ProcessMessages();
    const HWND main = *LWS::Win32::GetHwnd(window.GetWindow());
    SendMessageW(main, WM_KEYDOWN, VK_F10, 0);
    SendMessageW(main, WM_KEYUP, VK_F10, 0);
    SendMessageW(main, WM_KEYDOWN, VK_DOWN, 0);
    SendMessageW(main, WM_KEYUP, VK_DOWN, 0);
    std::ignore = platform.ProcessMessages();
    HWND popup{};
    EnumWindows(
        [](HWND candidate, LPARAM output) -> BOOL
        {
            DWORD process{};
            GetWindowThreadProcessId(candidate, &process);
            wchar_t title[32]{};
            GetWindowTextW(candidate, title, 32);
            if (process == GetCurrentProcessId() && IsWindowVisible(candidate) && std::wstring_view(title) == L"Menu")
                *reinterpret_cast<HWND*>(output) = candidate;
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&popup));
    REQUIRE(popup != nullptr);
    RECT client{};
    REQUIRE(GetClientRect(popup, &client));
    const auto getDpi = reinterpret_cast<UINT(WINAPI*)(HWND)>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    const double scale    = getDpi ? getDpi(popup) / 96.0 : 1.0;
    double expectedHeight = 0;
    for (const auto& row : view.items)
        expectedHeight += row.separator ? 5 : 24;
    const double height = (client.bottom - client.top) / scale;
    CHECK(height >= expectedHeight - 2);
    CHECK(height <= expectedHeight + 2);
    SendMessageW(popup, WM_KEYDOWN, VK_ESCAPE, 0);
    REQUIRE(window.Destroy() == LWS::Result::Success);
}

TEST_CASE("About is owned modal and closes with Escape", "[oiviewer][menus][windows]")
{
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    LWS::Window owner(platform);
    REQUIRE(owner.Create({.clientSize = {640, 480}, .visible = true}) == LWS::Result::Success);
    OIV::AboutForm about(platform);
    REQUIRE(about.Show(owner, {}, "2026-11-15 12:00") == LWS::Result::Success);
    const HWND parent = *LWS::Win32::GetHwnd(owner);
    HWND dialog{};
    EnumWindows(
        [](HWND candidate, LPARAM output) -> BOOL
        {
            DWORD process{};
            GetWindowThreadProcessId(candidate, &process);
            wchar_t title[64]{};
            GetWindowTextW(candidate, title, 64);
            if (process == GetCurrentProcessId() && std::wstring_view(title) == L"About OIViewer")
                *reinterpret_cast<HWND*>(output) = candidate;
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&dialog));
    REQUIRE(dialog != nullptr);
    RECT client{};
    REQUIRE(GetClientRect(dialog, &client));
    const double scale = owner.GetClientAreaMetrics().Scale().value_or(LWS::ContentScale{}).y;
    CHECK((client.bottom - client.top) / scale <= 300);
    CHECK((client.bottom - client.top) / scale >= 200);
    CHECK(GetWindow(dialog, GW_OWNER) == parent);
    CHECK((GetWindowLongPtrW(dialog, GWL_EXSTYLE) & WS_EX_APPWINDOW) == 0);
    CHECK_FALSE(IsWindowEnabled(parent));
    SendMessageW(dialog, WM_KEYDOWN, VK_ESCAPE, 1);
    CHECK_FALSE(IsWindowVisible(dialog));
    CHECK(IsWindowEnabled(parent));
    REQUIRE(about.Show(owner, {}, "2026-11-15 12:00") == LWS::Result::Success);
    CHECK(IsWindowVisible(dialog));
    SendMessageW(dialog, WM_CLOSE, 0, 0);
    CHECK_FALSE(IsWindowVisible(dialog));
    CHECK(IsWindowEnabled(parent));
}
#endif

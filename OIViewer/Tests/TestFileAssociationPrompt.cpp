#ifdef OIV_TEST_HAS_SETTINGS_EDITOR
    #include "UI/FileAssociationPrompt.h"
    #include <catch2/catch_test_macros.hpp>
    #ifdef _WIN32
        #include <LWS/Win32/Platform.hpp>
        #include <LWS/Win32/WindowExtensions.hpp>

TEST_CASE("Association prompt preserves modal ownership and safe dismissal",
          "[oiviewer][settings][associations][prompt][windows]")
{
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    {
        LWS::Window owner(platform);
        REQUIRE(owner.Create({.title = L"Prompt owner", .clientSize = {640, 480}}) == LWS::Result::Success);
        const auto native = LWS::Win32::GetHwnd(owner);
        REQUIRE(native);
        unsigned responses = 0;
        {
            OIV::FileAssociationPrompt prompt(platform);
            const auto respond = [&](auto choice)
            {
                CHECK(choice == OIV::FileAssociationPrompt::Choice::AskNextTime);
                CHECK(IsWindowEnabled(*native));
                ++responses;
            };
            REQUIRE(prompt.Show(owner, respond) == LWS::Result::Success);
            CHECK_FALSE(IsWindowEnabled(*native));
            // Drive the public native window boundary, without exposing the form's control tree.
            const HWND popup = GetWindow(*native, GW_ENABLEDPOPUP);
            REQUIRE(popup != nullptr);
            REQUIRE(popup != *native);
            SendMessageW(popup, WM_KEYDOWN, VK_RETURN, 1);
            CHECK(responses == 1);
            REQUIRE(prompt.Show(owner, respond) == LWS::Result::Success);
            SendMessageW(popup, WM_KEYDOWN, VK_ESCAPE, 1);
            CHECK(responses == 2);
            REQUIRE(prompt.Show(owner, respond) == LWS::Result::Success);
            SendMessageW(popup, WM_CLOSE, 0, 0);
            CHECK(responses == 3);
            REQUIRE(prompt.Show(owner, respond) == LWS::Result::Success);
            prompt.Hide();
            CHECK(IsWindowEnabled(*native));
            CHECK(responses == 3);
            REQUIRE(prompt.Show(owner, respond) == LWS::Result::Success);
            CHECK_FALSE(IsWindowEnabled(*native));
        }
        CHECK(IsWindowEnabled(*native));
        CHECK(responses == 3);
    }
    CHECK(platform.Shutdown() == LWS::Result::Success);
}
    #endif
#endif

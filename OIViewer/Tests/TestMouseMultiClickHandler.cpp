#include "MouseMultiClickHandler.h"

#include <LWS/Platform.hpp>
#ifdef LWS_HAS_WIN32_BACKEND
    #include <LWS/Win32/Platform.hpp>
#endif

#include <catch2/catch_test_macros.hpp>

#include <optional>

TEST_CASE("Mouse multi-click emits the configured press count", "[input][mouse]")
{
#ifdef LWS_HAS_WIN32_BACKEND
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    constexpr auto backend = LWS::BackendId::Win32;
#else
    constexpr auto backend = LWS::BackendId::Wayland;
#endif
    LWS::PlatformContext platform;
    if (platform.Init({.backend = backend}) != LWS::Result::Success)
        SKIP("No window-system backend is available");

    OIV::MouseMultiClickHandler handler(platform, 500, 2);
    std::optional<OIV::MouseMultiClickHandler::EventArgs> emitted;
    auto subscription = handler.OnMouseClickEvent.Subscribe([&](const auto& event) { emitted = event; });

    handler.SetButtonState(LWS::MouseButton::Left, true);
    handler.SetButtonState(LWS::MouseButton::Left, false);
    handler.SetButtonState(LWS::MouseButton::Left, true);

    REQUIRE(emitted.has_value());
    REQUIRE(emitted->button == LWS::MouseButton::Left);
    REQUIRE(emitted->clickCount == 2);

    handler.Reset();
    emitted.reset();
    handler.SetButtonState(LWS::MouseButton::Right, true);
    handler.SetButtonState(LWS::MouseButton::Right, false);
    handler.SetButtonState(LWS::MouseButton::Right, true);

    REQUIRE(emitted.has_value());
    REQUIRE(emitted->button == LWS::MouseButton::Right);
    REQUIRE(emitted->clickCount == 2);
}

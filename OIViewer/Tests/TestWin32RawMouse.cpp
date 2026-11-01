#include <catch2/catch_test_macros.hpp>

#ifdef _WIN32

    #include <LInput/Win32/RawInput/RawInput.h>

TEST_CASE("Win32 raw mouse translation preserves wheel and button data", "[oiviewer][win32][input]")
{
    RAWMOUSE mouse{};
    mouse.usButtonFlags = RI_MOUSE_WHEEL | RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_BUTTON_4_UP;
    mouse.usButtonData  = static_cast<USHORT>(static_cast<SHORT>(-30));
    mouse.lLastX        = 3;
    mouse.lLastY        = -4;

    const auto event = LInput::RawInput::TranslateRawInputMouse(mouse, 7);

    REQUIRE(event.deviceType == LInput::RawInput::RawInputDeviceType::Mouse);
    REQUIRE(event.deviceIndex == 7);
    REQUIRE(event.deltaX == 3);
    REQUIRE(event.deltaY == -4);
    REQUIRE(event.wheelDelta == -30);
    REQUIRE(event.buttonState[0] == LInput::ButtonState::Down);
    REQUIRE(event.buttonState[3] == LInput::ButtonState::Up);
    REQUIRE(event.buttonState[1] == LInput::ButtonState::NotSet);
}

#endif

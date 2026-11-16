#include <catch2/catch_test_macros.hpp>
#ifdef OIV_TEST_VIEWER
    #include "ImageControl.h"
    #include "MainWindow.h"
    #include <type_traits>
    #ifdef LWS_HAS_WIN32_BACKEND
        #include <LWS/Win32/Platform.hpp>
        #include <LWS/Win32/WindowExtensions.hpp>
        #include <Windows.h>
    #endif

static_assert(std::is_base_of_v<LWSUI::Panel, OIV::ImageControl>);
TEST_CASE("Sidebar panel retains row selection and scrolling", "[sidebar][ui]")
{
    OIV::ImageControl panel;
    auto& images = panel.GetImageList();
    for (uint32_t i = 0; i < 5; ++i)
        images.SetImage({.index = i, .title = "Frame"});
    panel.Measure({160, 160});
    panel.Arrange({10, 32, 160, 160});
    CHECK(images.GetNumberOfDisplayedElements() == 2);
    CHECK(panel.Dispatch({.kind = LWSUI::InputKind::Down, .x = 20, .y = 120}));
    CHECK(images.GetSelected() == 1);
    CHECK(panel.Dispatch({.kind = LWSUI::InputKind::Wheel, .wheel = -1}));
    CHECK(images.GetScrollPosition() == 1);
    CHECK(panel.Dispatch({.kind = LWSUI::InputKind::Down, .x = 20, .y = 40}));
    CHECK(images.GetSelected() == 1);
    auto& bar = static_cast<LWSUI::ScrollBar&>(*panel.Content());
    CHECK(bar.Visible());
    const auto bounds = bar.Bounds();
    const float barX  = bounds.x + bounds.width / 2;
    CHECK(bar.Dispatch({.kind = LWSUI::InputKind::Down, .x = barX, .y = bounds.y + 70}));
    CHECK(bar.Dispatch({.kind = LWSUI::InputKind::Move, .x = barX, .y = bounds.y + bounds.height}));
    CHECK(bar.Dispatch({.kind = LWSUI::InputKind::Up, .x = barX, .y = bounds.y + bounds.height}));
    CHECK(images.GetScrollPosition() == 3);
    images.SetSelected(0);
    CHECK(images.GetScrollPosition() == 0);
    CHECK(bar.Value() == 0);
    images.Clear();
    CHECK_FALSE(bar.Visible());
}
TEST_CASE("Sidebar thumbnails composite original alpha over a checkerboard", "[sidebar][alpha]")
{
    std::vector<unsigned> green;
    for (const unsigned alpha : {0, 128, 255})
    {
        OIV::ImageControl panel;
        std::vector<std::byte> pixels(46 * 46 * 4);
        for (size_t i = 0; i < pixels.size(); i += 4)
        {
            pixels[i + 2] = std::byte{255};
            pixels[i + 3] = std::byte(alpha);
        }
        const LWS::BitmapBuffer source{.pixels   = pixels,
                                       .format   = LWS::BitmapPixelFormat::Bgra8,
                                       .width    = 46,
                                       .height   = 46,
                                       .rowPitch = 184};
        panel.GetImageList().SetImage({.index = 0, .title = "Alpha", .bitmap = std::make_shared<LWS::Bitmap>(source)});
        const auto thumbnail = panel.GetImageList().GetImage(0).bitmap->GetBuffer();
        CHECK(std::to_integer<unsigned>(thumbnail.pixels[32 * thumbnail.rowPitch + 32 * 4 + 3]) == alpha);
        panel.Measure({160, 160});
        panel.Arrange({0, 0, 160, 160});
        LWSUI::Canvas canvas;
        canvas.Begin(160, 160);
        panel.Render(canvas);
        const auto rendered = canvas.End();
        const size_t sample = 29 * rendered.rowPitch + 76 * 4;
        green.push_back(std::to_integer<unsigned>(rendered.pixels[sample + 1]));
        CHECK(std::to_integer<unsigned>(rendered.pixels[sample + 3]) == 255);
        // The two checker tones remain visible through a fully transparent thumbnail.
        if (alpha == 0)
        {
            CHECK(rendered.pixels[sample + 1] != rendered.pixels[sample + 5 * 4 + 1]);
        }
    }
    CHECK(green[0] > green[1]);
    CHECK(green[1] > green[2]);
    CHECK(green[2] == 0);
}
    #ifdef LWS_HAS_WIN32_BACKEND
TEST_CASE("Sidebar shares the main UI surface without another child window", "[sidebar][win32]")
{
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    LWS::Window parent(platform);
    REQUIRE(parent.Create() == LWS::Result::Success);
    for (bool embedded : {false, true})
    {
        CAPTURE(embedded);
        OIV::MainWindow window(platform, {});
        REQUIRE(window.Create({.parent = embedded ? &parent : nullptr, .clientSize = {640, 480}}) ==
                LWS::Result::Success);
        window.ShowStatusBar(false);
        const auto original = window.GetCanvasWindow().GetClientAreaMetrics().logical;
        window.GetImageControl().GetImageList().SetImage({.index = 0, .title = "Image"});
        window.SetShowImageControl(true);
        CHECK(window.GetImageControl().Visible());
        CHECK(window.GetCanvasWindow().GetClientAreaMetrics().logical.x == original.x - 165);
        const auto hwnd   = *LWS::Win32::GetHwnd(window.GetWindow());
        unsigned children = 0;
        for (HWND child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
            ++children;
        CHECK(children == 1);
        auto& split              = static_cast<LWSUI::SplitPanel&>(*window.GetImageControl().Parent());
        const auto initialBounds = split.Bounds();
        const auto divider       = split.Divider().Bounds();
        const float x            = divider.x + divider.width / 2;
        const float y            = divider.y + divider.height / 2;
        REQUIRE(split.Divider().Dispatch({.kind = LWSUI::InputKind::Down, .x = x, .y = y}));
        REQUIRE(split.Divider().Dispatch({.kind = LWSUI::InputKind::Move, .x = x - 40, .y = y}));
        split.Arrange(initialBounds);
        CHECK(window.GetCanvasWindow().GetClientAreaMetrics().logical.x == original.x - 205);
        REQUIRE(split.Divider().Dispatch({.kind = LWSUI::InputKind::Up, .x = x - 40, .y = y}));
        const auto currentDivider = split.Divider().Bounds();
        const float currentX      = currentDivider.x + currentDivider.width / 2;
        REQUIRE(split.Divider().Dispatch({.kind = LWSUI::InputKind::Down, .x = currentX, .y = y}));
        REQUIRE(split.Divider().Dispatch({.kind = LWSUI::InputKind::Move, .x = 0, .y = y}));
        split.Arrange(initialBounds);
        REQUIRE(split.Divider().Dispatch({.kind = LWSUI::InputKind::Up, .x = 0, .y = y}));
        CHECK(window.GetImageControl().Bounds().width == 260);
        CHECK(window.GetCanvasWindow().GetClientAreaMetrics().logical.x == original.x - 265);
        LWSUI::Canvas textMetrics;
        auto font   = window.GetImageControl().Font();
        font.size   = 11;
        font.weight = 600;
        CHECK(textMetrics.Measure("65535/65535  65535 x 65535 x 32 BPP", font) + 24 +
                  window.GetImageControl().Style().scrollbarWidth <=
              260);
        window.SetShowImageControl(false);
        CHECK_FALSE(window.GetImageControl().Visible());
        CHECK(window.GetCanvasWindow().GetClientAreaMetrics().logical == original);
        window.SetShowImageControl(true);
        CHECK(window.GetImageControl().GetImageList().GetNumberOfElements() == 1);
        CHECK(window.GetCanvasWindow().GetClientAreaMetrics().logical.x == original.x - 265);
        REQUIRE(window.Destroy() == LWS::Result::Success);
    }
}
    #endif
#endif

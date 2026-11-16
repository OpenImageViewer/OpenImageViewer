#include <catch2/catch_test_macros.hpp>
#if defined(OIV_TEST_VIEWER) && (defined(LWS_HAS_WIN32_BACKEND) || defined(LWS_HAS_WAYLAND_BACKEND))
    #include "UI/ResizeForm.h"
    #include "ImageBitmap.h"
    #include "WindowPlacement.h"
    #include <ImageUtil/ImageUtil.h>
    #include <algorithm>
    #include <cstring>
    #include <filesystem>
    #include <fstream>
    #include <nlohmann/json.hpp>
    #include <LWSUI/UIHost.hpp>
    #include <LWS/source/internal/WindowBackendAccess.hpp>
    #include <thread>
    #include <chrono>
    #ifdef LWS_HAS_WIN32_BACKEND
        #include <LWS/Win32/Platform.hpp>
        #include <LWS/Win32/WindowExtensions.hpp>
        #include <Windows.h>
    #endif

TEST_CASE("Resize shortcut opens its dialog only once", "[ResizeForm][configuration]")
{
    const auto directory = std::filesystem::path(OIV_TEST_SOURCE_DIR).parent_path() /
                           "OIViewer/Resources/Configuration";
    std::ifstream bindingsFile(directory / "KeyBindings.json");
    std::ifstream commandsFile(directory / "Commands.json");
    const auto bindings = nlohmann::json::parse(bindingsFile);
    const auto commands = nlohmann::json::parse(commandsFile);
    // LInput deliberately executes every command attached to a shortcut, including duplicates.
    CHECK(std::ranges::count_if(
              bindings["KeyBindings"], [](const auto& binding)
              { return binding.contains("Control+R") && binding["Control+R"] == "ShowResizeForm"; }) == 1);
    CHECK(std::ranges::count_if(commands["commands"],
                                [](const auto& command) { return command["GroupID"] == "ShowResizeForm"; }) == 1);
}

TEST_CASE("Resize preview preserves straight and premultiplied transparency", "[ResizeForm][preview]")
{
    using Alpha = IMCodec::ImageItem::ColorInfo::AlphaMode;
    for (const auto alpha : {Alpha::Straight, Alpha::Premultiplied})
    {
        CAPTURE(alpha);
        auto item                                = std::make_shared<IMCodec::ImageItem>();
        item->descriptor.width                   = 2;
        item->descriptor.height                  = 1;
        item->descriptor.rowPitchInBytes         = 8;
        item->descriptor.texelFormatDecompressed = IMCodec::TexelFormat::I_B8_G8_R8_A8;
        item->colorInfo.alphaMode                = alpha;
        const uint8_t red                        = alpha == Alpha::Straight ? 255 : 128;
        const uint8_t blue                       = alpha == Alpha::Straight ? 255 : 0;
        const uint8_t pixels[]{0, 0, red, 128, blue, 0, 0, 0};
        item->data.Allocate(sizeof(pixels));
        std::memcpy(item->data.data(), pixels, sizeof(pixels));
        auto source  = std::make_shared<IMCodec::Image>(item, IMCodec::ImageItemType::Unknown);
        auto resized = OIV::ResizeImage(source, {{0, 0}, {2, 1}},
                                        {.size       = {1, 1},
                                         .aspect     = OIV::ResizeAspectRatio::DoNotPreserve,
                                         .autoFilter = false,
                                         .filter     = OIV::ResizeFilter::Box});
        REQUIRE(resized);
        auto bgra = IMUtil::ImageUtil::ConvertImageWithNormalization(*resized, IMCodec::TexelFormat::I_B8_G8_R8_A8,
                                                                     false);
        REQUIRE(bgra);
        LWS::Bitmap bitmap(OIV::MakeBgraBitmapView(*bgra));
        const auto buffer = bitmap.GetBuffer();
        CHECK(buffer.format == LWS::BitmapPixelFormat::Bgra8Premultiplied);
        CHECK(std::to_integer<unsigned>(buffer.pixels[0]) < 2);  // Hidden blue must not bleed into red edges.
        CHECK(std::to_integer<unsigned>(buffer.pixels[2]) >= 63);
        CHECK(std::to_integer<unsigned>(buffer.pixels[2]) <= 65);
        CHECK(std::to_integer<unsigned>(buffer.pixels[3]) >= 63);
        CHECK(std::to_integer<unsigned>(buffer.pixels[3]) <= 65);
    }
}

    #ifdef LWS_HAS_WIN32_BACKEND

TEST_CASE("Resize Escape and native close release the modal owner and allow reopening", "[ResizeForm][ResizeWindow]")
{
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    {
        LWS::Window owner(platform);
        REQUIRE(owner.Create() == LWS::Result::Success);
        int dismissed = 0;
        OIV::ResizeFormController form(platform, {},
                                       {.originalBounds = {{0, 0}, {130, 260}}, .onDismiss = [&] { ++dismissed; }});
        form.Show(&owner);
        struct Search
        {
            HWND owner;
            HWND dialog{};
        } search{*LWS::Win32::GetHwnd(owner)};
        EnumThreadWindows(
            GetCurrentThreadId(),
            [](HWND candidate, LPARAM data) -> BOOL
            {
                auto& target = *reinterpret_cast<Search*>(data);
                if (GetWindow(candidate, GW_OWNER) == target.owner)
                    target.dialog = candidate;
                return TRUE;
            },
            reinterpret_cast<LPARAM>(&search));
        REQUIRE(search.dialog != nullptr);
        CHECK(IsWindowVisible(search.dialog));
        CHECK_FALSE(IsWindowEnabled(search.owner));
        CHECK((GetWindowLongPtrW(search.dialog, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0);
        SendMessageW(search.dialog, WM_KEYDOWN, VK_ESCAPE, 0);
        CHECK_FALSE(IsWindowVisible(search.dialog));
        CHECK(IsWindowEnabled(search.owner));
        CHECK(dismissed == 1);
        form.Show(&owner);
        CHECK(IsWindowVisible(search.dialog));
        CHECK_FALSE(IsWindowEnabled(search.owner));
        SendMessageW(search.dialog, WM_CLOSE, 0, 0);
        CHECK_FALSE(IsWindowVisible(search.dialog));
        CHECK(IsWindowEnabled(search.owner));
        CHECK(dismissed == 2);
    }
    REQUIRE(platform.Shutdown() == LWS::Result::Success);
}

    #endif

namespace OIV
{
    struct ResizeFormTestAccess
    {
        static void CheckPreviewCheckerboard(ResizeFormController& form)
        {
            // A fully transparent source exposes the checker phase at a fixed pane coordinate.
            auto* dock    = static_cast<LWSUI::Container*>(form.fOriginalSize->Parent()->Parent());
            auto* preview = dock->Children()[1].get();
            form.fAspectRatioRadio->SetValue("preserve_bars");
            form.fLetterboxColor->SetValue(LLUtils::Color(0, 0, 0, 0));
            std::optional<unsigned> previous;
            for (const double width : {130., 260., 390.})
            {
                form.AcceptDimensionBasis({ResizeSizeMode::Absolute, ResizeUnits::Pixels, width, 260}, false);
                preview->Arrange({20, 30, 300, 240});
                LWSUI::Canvas canvas;
                canvas.Begin(340, 300);
                preview->Render(canvas);
                const auto bitmap = canvas.End();
                const auto sample = std::to_integer<unsigned>(bitmap.pixels[145 * bitmap.rowPitch + 165 * 4]);
                if (previous)
                    CHECK(sample == *previous);
                previous = sample;
            }
        }
        static void CheckCompactLayout(ResizeFormController& form)
        {
            auto* root = form.fUIHost->Root();
            root->Measure({500, 300});
            root->Arrange({0, 0, 500, 300});
            CHECK(form.fApplyButton->Bounds().y + form.fApplyButton->Bounds().height <= 300);
            CHECK(form.fWidthInput->Bounds().width > 50);
            // The final scaler controls remain reachable through the scroll view.
            auto* ancestor = form.fWidthInput->Parent();
            while (ancestor && !dynamic_cast<LWSUI::ScrollView*>(ancestor))
                ancestor = ancestor->Parent();
            REQUIRE(ancestor);
            auto* scroll = static_cast<LWSUI::ScrollView*>(ancestor);
            CHECK(scroll->Content()->Bounds().height > scroll->Viewport().height);
            scroll->SetOffset(10000);
            CHECK(form.fAutoFilter->Bounds().y >= scroll->Viewport().y);
            CHECK(form.fAutoFilter->Bounds().y + form.fAutoFilter->Bounds().height <=
                  scroll->Viewport().y + scroll->Viewport().height);
        }
        static void CheckLetterbox(ResizeFormController& form)
        {
            CHECK_FALSE(form.fLetterboxColor->Visible());
            form.fAspectRatioRadio->SetValue("preserve_bars");
            form.OnAspectRatioChanged();
            CHECK(form.fLetterboxColor->Visible());
            form.fLetterboxColor->SetValue(LLUtils::Color(30, 90, 180, 96));
            REQUIRE(form.CurrentRequest());
            CHECK(form.CurrentRequest()->letterboxColor == LLUtils::Color(30, 90, 180, 96));
            form.SetBusy(true);
            CHECK_FALSE(form.fLetterboxColor->Enabled());
            form.SetBusy(false);
            form.ResetAll();
            CHECK(form.fLetterboxColor->Value() == form.fInput.defaults.letterboxColor);
            CHECK_FALSE(form.fLetterboxColor->Visible());
        }
    #ifdef LWS_HAS_WIN32_BACKEND
        static void CheckParameterResizePosition(ResizeFormController& form)
        {
            auto& window       = *form.fDialogWindow;
            const auto initial = window.GetPlacement();
            REQUIRE(initial.position);
            REQUIRE(
                window.RequestPlacement({.position = LWS::Point{initial.position->x + 17, initial.position->y + 19}}) ==
                LWS::Result::Success);
            const auto position = window.GetPlacement().position;
            form.OnUnitsChanged("inches");
            CHECK(window.GetPlacement().position == position);
            CHECK(window.GetPlacement().clientSize.y > initial.clientSize.y);
            form.OnUnitsChanged("pixels");
            CHECK(window.GetPlacement().position == position);
            CHECK(window.GetPlacement().clientSize == initial.clientSize);
            const auto hwnd = *LWS::Win32::GetHwnd(window);
            RECT bounds{};
            MONITORINFO monitor{sizeof(monitor)};
            REQUIRE(GetWindowRect(hwnd, &bounds));
            REQUIRE(GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor));
            REQUIRE(SetWindowPos(hwnd, nullptr, bounds.left, monitor.rcWork.bottom - (bounds.bottom - bounds.top), 0, 0,
                                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
            const auto bottomPosition = window.GetPlacement().position;
            form.OnUnitsChanged("inches");
            CHECK(window.GetPlacement().position == bottomPosition);
            CheckWorkArea(form);
        }
        static void CheckOversizedPlacement(ResizeFormController& form)
        {
            auto& window    = *form.fDialogWindow;
            const auto hwnd = *LWS::Win32::GetHwnd(window);
            MONITORINFO monitor{sizeof(monitor)};
            REQUIRE(GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor));
            REQUIRE(SetWindowPos(hwnd, nullptr, monitor.rcWork.left, monitor.rcWork.top,
                                 monitor.rcWork.right - monitor.rcWork.left + 200,
                                 monitor.rcWork.bottom - monitor.rcWork.top + 200, SWP_NOZORDER | SWP_NOACTIVATE));
            REQUIRE(PlaceWindowNear(window, *window.GetOwner()) == LWS::Result::Success);
            CheckWorkArea(form);
        }
        static void CheckWorkArea(ResizeFormController& form)
        {
            const auto hwnd = *LWS::Win32::GetHwnd(*form.fDialogWindow);
            MONITORINFO monitor{sizeof(monitor)};
            REQUIRE(GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor));
            RECT bounds{};
            REQUIRE(GetWindowRect(hwnd, &bounds));
            CHECK(bounds.left >= monitor.rcWork.left);
            CHECK(bounds.top >= monitor.rcWork.top);
            CHECK(bounds.right <= monitor.rcWork.right);
            CHECK(bounds.bottom <= monitor.rcWork.bottom);
        }
    #endif
        static void CheckReinitialization(ResizeFormController& form)
        {
            auto* window = form.fDialogWindow.get();
            auto* host   = form.fUIHost.get();
            auto* width  = form.fWidthInput;
            REQUIRE(host->Focus(width));
            width->SetText("999");
            const auto position = window->GetPlacement().position;
            form.Reinitialize({.originalBounds = {{0, 0}, {640, 480}}});
            CHECK(form.fDialogWindow.get() == window);
            CHECK(form.fUIHost.get() == host);
            CHECK(form.fWidthInput == width);
            CHECK(host->IsFocused(*width));
            CHECK(window->GetPlacement().position == position);
            CHECK(width->Text() == "640.00");
            CHECK(form.fHeightInput->Text() == "480.00");
            CHECK(form.fOriginalWidth == 640);
            CHECK(form.fOriginalHeight == 480);
            form.ResetAll();
            CHECK(width->Text() == "640.00");
            CHECK(host->IsFocused(*width));
        }
        static void CheckFileDrop(ResizeFormController& form, const std::filesystem::path& path)
        {
            REQUIRE(LWS::internal::WindowBackendAccess::Dispatch(*form.fDialogWindow, LWS::EventDragDropFile{path}) ==
                    LWS::EventResponse::Handled);
        }
        static void CheckInitialization(ResizeFormController& form)
        {
            const auto initial = *form.PixelsFromBasis();
            form.OnThumbwheel(false, .5, LWSUI::EditPhase::Commit);
            form.OnUnitsChanged("pixels");
            form.SetSizeMode(ResizeSizeMode::Absolute);
            form.ResetAll();
            const auto restored = *form.PixelsFromBasis();
            REQUIRE(restored.width == initial.width);
            REQUIRE(restored.height == initial.height);
            REQUIRE(form.fCurrentUnits == form.fInput.defaults.units);
            REQUIRE(form.fCurrentSizeMode == form.fInput.defaults.sizeMode);
            REQUIRE(form.fSizeBasis.width == form.fInitialBasis.width);
            REQUIRE(form.fSizeBasis.height == form.fInitialBasis.height);
        }
        static void Check(ResizeFormController& form)
        {
            using namespace LWSUI;
            auto pixels      = [&] { return *form.PixelsFromBasis(); };
            auto checkPixels = [&](double w, double h)
            {
                REQUIRE(std::abs(pixels().width - w) < 1e-8);
                REQUIRE(std::abs(pixels().height - h) < 1e-8);
            };
            checkPixels(260, 520);
            REQUIRE(form.fInput.thumbwheel.acceleration == .4);
            auto tuning               = form.fInput.thumbwheel;
            tuning.acceleration       = .25;
            tuning.maximumSpeed       = 3000;
            tuning.deadZonePercent    = 10;
            tuning.dragSensitivity    = 2;
            tuning.fineModeMultiplier = .05;
            form.SetThumbwheelSettings(tuning);
            REQUIRE(form.fInput.thumbwheel.maximumSpeed == 3000);
            REQUIRE(form.fInput.thumbwheel.deadZonePercent == 10);
            REQUIRE(form.fInput.thumbwheel.dragSensitivity == 2);
            REQUIRE(form.fInput.thumbwheel.fineModeMultiplier == .05);
            REQUIRE(form.fInput.thumbwheel.acceleration == .25);
            for (const auto* units : {"pixels", "inches", "centimeters"})
            {
                form.OnUnitsChanged(units);
                for (const auto* aspect : {"preserve", "don't_preserve", "preserve_bars"})
                {
                    form.fAspectRatioRadio->SetValue(aspect);
                    form.AcceptDimensionBasis({ResizeSizeMode::Absolute, ResizeUnits::Pixels, 780, 1040}, false);
                    form.ResetDimension(false);
                    checkPixels(260, std::string_view(aspect) == "preserve" ? 520 : 1040);
                    form.AcceptDimensionBasis({ResizeSizeMode::Absolute, ResizeUnits::Pixels, 780, 1040}, false);
                    form.ResetDimension(true);
                    checkPixels(std::string_view(aspect) == "preserve" ? 260 : 780, 520);
                }
            }
            form.ResetAll();
            form.SetSizeMode(ResizeSizeMode::Relative);
            form.OnThumbwheel(false, 0, EditPhase::Preview);
            form.OnThumbwheel(false, 1, EditPhase::Preview);
            checkPixels(260.13, 520.26);
            form.OnThumbwheel(false, 0, EditPhase::Cancel);
            checkPixels(260, 520);
            form.OnThumbwheel(true, 1, EditPhase::Commit);
            checkPixels(260.13, 520.26);
            REQUIRE(form.fStandardResolutions->Value() == "custom");
            form.ResetAll();
            form.OnThumbwheel(false, .49, EditPhase::Preview);
            checkPixels(260, 520);
            form.OnThumbwheel(false, .51, EditPhase::Preview);
            checkPixels(261, 522);
            form.OnThumbwheel(false, .51, EditPhase::Commit);
            form.OnThumbwheel(false, 1, EditPhase::Commit);
            checkPixels(262, 524);
            form.ResetAll();
            form.OnThumbwheel(true, 1, EditPhase::Preview);
            checkPixels(260.5, 521);
            REQUIRE(form.fWidthInput->Text() == "261.00");
            REQUIRE(form.fHeightInput->Text() == "521.00");
            auto request = form.CurrentRequest();
            REQUIRE(request);
            REQUIRE(request->preserveHeight);
            REQUIRE(ResolveImageResize({130, 260}, *request)->canvas == LLUtils::PointI32{261, 521});
            form.OnThumbwheel(true, 1, EditPhase::Commit);
            for (int repeat = 0; repeat < 20; ++repeat)
            {
                form.OnUnitsChanged("inches");
                form.OnUnitsChanged("pixels");
                form.OnDimensionChanged(false, form.fWidthInput->Text(), EditPhase::Commit);
            }
            checkPixels(260.5, 521);
            form.OnThumbwheel(true, 1, EditPhase::Preview);
            form.OnThumbwheel(true, 0, EditPhase::Cancel);
            checkPixels(260.5, 521);
            form.ResetAll();
            form.AcceptDimensionBasis({ResizeSizeMode::Absolute, ResizeUnits::Pixels, 32000, 64000}, false);
            form.OnThumbwheel(false, 1, EditPhase::Commit);
            checkPixels(32001, 64002);
            form.ResetAll();
            form.OnThumbwheel(false, -1000, EditPhase::Preview);
            checkPixels(1, 2);
            form.OnThumbwheel(false, -999, EditPhase::Preview);
            checkPixels(2, 4);
            form.OnThumbwheel(false, 0, EditPhase::Cancel);
            checkPixels(260, 520);
            form.OnUnitsChanged("inches");
            form.OnThumbwheel(false, 1, EditPhase::Commit);
            checkPixels(261.2, 522.4);
            form.ResetAll();
            form.AcceptDimensionBasis({ResizeSizeMode::Absolute, ResizeUnits::Pixels, 260.123456789, 520.246913578},
                                      false);
            for (int i = 0; i < 20; ++i)
                form.OnThumbwheel(false, 0, EditPhase::Commit);
            checkPixels(260.123456789, 520.246913578);
            form.fWidthInput->EditText("");
            form.ResetAll();
            checkPixels(260, 520);
            REQUIRE(form.CurrentDPI() == 120);
            REQUIRE_FALSE(form.fAutoFilter->Value());
            REQUIRE(form.fManualFilter == ResizeFilter::Box);
            REQUIRE(form.fWidthInput->Text() == "260.00");
            REQUIRE(form.fCurrentSizeMode == ResizeSizeMode::Absolute);
            REQUIRE(form.fCurrentUnits == ResizeUnits::Pixels);
            REQUIRE(form.fAspectRatioRadio->Value() == "preserve");
            REQUIRE(form.fStandardResolutions->Value() == "current");
            form.OnUnitsChanged("inches");
            form.fDPIInput->SetText("240");
            form.OnDPIChanged("240", EditPhase::Commit);
            checkPixels(520, 1040);
            form.ResetDimension(false);
            checkPixels(260, 520);
            REQUIRE(form.fWidthInput->Text() == "1.08");
            form.fAutoFilter->SetValue(true);
            form.fManualFilter = ResizeFilter::Triangle;
            form.fAspectRatioRadio->SetValue("preserve_bars");
            form.ResetAll();
            REQUIRE(form.CurrentDPI() == 120);
            REQUIRE_FALSE(form.fAutoFilter->Value());
            REQUIRE(form.fManualFilter == ResizeFilter::Box);
            REQUIRE(form.fAspectRatioRadio->Value() == "preserve");
            // Wayland delivers the drawable viewport through an asynchronous configure.
            for (int attempt = 0; attempt < 500 && !form.fDialogWindow->IsConfigured(); ++attempt)
            {
                REQUIRE(form.fPlatform.ProcessMessages() == LWS::LoopResult::Continue);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            REQUIRE(form.fDialogWindow->IsConfigured());
            form.fUIHost->Update();
            auto* wheel       = form.fThumbwheels[0];
            const auto bounds = wheel->Bounds();
            const float x = bounds.x + bounds.width / 2, y = bounds.y + bounds.height / 2;
            REQUIRE(form.fUIHost->Root()->HitTest(x, y) == wheel);
            // Keep timed movement below the snapping threshold while checking quick clicks.
            wheel->SetSpeedRange(1, 1);
            for (const int direction : {-1, 1})
            {
                const float clickX = x + direction * (bounds.width / 2 - 4) * .5f;
                REQUIRE(form.fUIHost->Route({.kind = InputKind::Down, .x = clickX, .y = y}));
                checkPixels(260 + direction, 520 + direction * 2);
                REQUIRE(form.fUIHost->Route({.kind = InputKind::Up, .x = clickX, .y = y}));
                checkPixels(260 + direction, 520 + direction * 2);
                form.ResetAll();
            }
            REQUIRE(form.fUIHost->Route({.kind = InputKind::Down, .x = x, .y = y}));
            REQUIRE(form.fUIHost->Route({.kind = InputKind::Move, .x = x + 120, .y = y}));
            // Leaving the center applies the click step before timed movement.
            checkPixels(261, 522);
            form.OnThumbwheel(false, 400, EditPhase::Preview);
            checkPixels(660, 1320);
            REQUIRE(form.fUIHost->Route({.kind = InputKind::Cancel}));
            checkPixels(260, 520);
            for (auto* control : form.fThumbwheels)
            {
                REQUIRE(control->Bounds().width > 50);
                REQUIRE(control->Bounds().height == 20);
            }
            form.SetBusy(true);
            for (auto* button : form.fResetButtons)
                REQUIRE_FALSE(button->Enabled());
            for (auto* wheel : form.fThumbwheels)
                REQUIRE_FALSE(wheel->Enabled());
            form.ResetAll();
            form.SetBusy(false);
        }
    };
}  // namespace OIV

TEST_CASE("Resize preview keeps its checkerboard phase across dimension changes", "[ResizeForm][preview]")
{
    #ifdef LWS_HAS_WIN32_BACKEND
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    #endif
    LWS::PlatformContext platform;
    #ifdef LWS_HAS_WIN32_BACKEND
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    #else
    REQUIRE(platform.Init({.backend = LWS::BackendId::Wayland}) == LWS::Result::Success);
    #endif
    std::array<std::byte, 4> pixels{};
    auto bitmap = std::make_shared<LWS::Bitmap>(LWS::BitmapBuffer{.pixels   = pixels,
                                                                  .format   = LWS::BitmapPixelFormat::Bgra8,
                                                                  .width    = 1,
                                                                  .height   = 1,
                                                                  .rowPitch = 4});
    OIV::ResizeFormController form(platform, {}, {.sourceBitmap = bitmap, .originalBounds = {{0, 0}, {130, 260}}});
    form.Show(nullptr);
    OIV::ResizeFormTestAccess::CheckPreviewCheckerboard(form);
    OIV::ResizeFormTestAccess::CheckCompactLayout(form);
}

TEST_CASE("Resize image replacement retains the window and focused editor", "[ResizeForm][ResizeWindow][focus]")
{
    #ifdef LWS_HAS_WIN32_BACKEND
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    #endif
    LWS::PlatformContext platform;
    #ifdef LWS_HAS_WIN32_BACKEND
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    #else
    REQUIRE(platform.Init({.backend = LWS::BackendId::Wayland}) == LWS::Result::Success);
    #endif
    OIV::ResizeFormController form(platform, {}, {.originalBounds = {{0, 0}, {130, 260}}});
    form.Show(nullptr);
    OIV::ResizeFormTestAccess::CheckReinitialization(form);
    OIV::ResizeFormTestAccess::CheckLetterbox(form);
}

    #ifdef LWS_HAS_WIN32_BACKEND
TEST_CASE("Resize parameter changes preserve the user-positioned window", "[ResizeForm][ResizeWindow][placement]")
{
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    LWS::Window owner(platform);
    REQUIRE(owner.Create() == LWS::Result::Success);
    OIV::ResizeFormController form(platform, {}, {.originalBounds = {{0, 0}, {130, 260}}});
    form.Show(&owner);
    OIV::ResizeFormTestAccess::CheckParameterResizePosition(form);
}

TEST_CASE("Resize stays in the work area when its owner straddles the screen edge",
          "[ResizeForm][ResizeWindow][placement]")
{
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    LWS::Window owner(platform);
    REQUIRE(owner.Create() == LWS::Result::Success);
    const auto hwnd = *LWS::Win32::GetHwnd(owner);
    MONITORINFO monitor{sizeof(monitor)};
    REQUIRE(GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor));
    REQUIRE(SetWindowPos(hwnd, nullptr, monitor.rcWork.right - 100, monitor.rcWork.bottom - 100, 300, 200,
                         SWP_NOZORDER | SWP_NOACTIVATE));
    OIV::ResizeFormController form(platform, {}, {.originalBounds = {{0, 0}, {130, 260}}});
    form.Show(&owner);
    OIV::ResizeFormTestAccess::CheckWorkArea(form);
    OIV::ResizeFormTestAccess::CheckLetterbox(form);
    OIV::ResizeFormTestAccess::CheckWorkArea(form);
    OIV::ResizeFormTestAccess::CheckOversizedPlacement(form);
}
    #endif

TEST_CASE("Resize delegates dropped files and ignores drops while resizing", "[ResizeForm][ResizeWindow][drop]")
{
    #ifdef LWS_HAS_WIN32_BACKEND
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    #endif
    LWS::PlatformContext platform;
    #ifdef LWS_HAS_WIN32_BACKEND
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    #else
    REQUIRE(platform.Init({.backend = LWS::BackendId::Wayland}) == LWS::Result::Success);
    #endif
    std::filesystem::path dropped;
    unsigned calls = 0;
    OIV::ResizeFormController form(platform, {},
                                   {.onFileDrop = [&](const LWS::EventDragDropFile& event)
                                    {
                                        dropped = event.fileName;
                                        ++calls;
                                    }});
    form.Show(nullptr);
    const auto path = std::filesystem::path("image with spaces.png");
    OIV::ResizeFormTestAccess::CheckFileDrop(form, path);
    CHECK(calls == 1);
    CHECK(dropped == path);
    form.SetBusy(true);
    OIV::ResizeFormTestAccess::CheckFileDrop(form, "ignored.png");
    CHECK(calls == 1);
    form.SetBusy(false);
    OIV::ResizeFormTestAccess::CheckFileDrop(form, "replacement.png");
    CHECK(calls == 2);
    CHECK(dropped == "replacement.png");
}

TEST_CASE("Resize resets and thumbwheels preserve initialization, units and exact values", "[ResizeForm][ResizeWindow]")
{
    #ifdef LWS_HAS_WIN32_BACKEND
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    #endif
    LWS::PlatformContext platform;
    #ifdef LWS_HAS_WIN32_BACKEND
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    #else
    REQUIRE(platform.Init({.backend = LWS::BackendId::Wayland}) == LWS::Result::Success);
    #endif
    {
        LWS::Window owner(platform);
        REQUIRE(owner.Create() == LWS::Result::Success);
        OIV::ResizeFormController form(platform, {},
                                       {.originalBounds = {{0, 0}, {130, 260}},
                                        .defaults       = {.size       = OIV::ResizeFormDimensions{260, 520},
                                                           .dpi        = 120,
                                                           .autoFilter = false,
                                                           .filter     = OIV::ResizeFilter::Box}});
        REQUIRE(owner.SetVisible(true) == LWS::Result::Success);
        form.Show(&owner);
        OIV::ResizeFormTestAccess::Check(form);
        form.Hide();
    }
    for (const auto mode : {OIV::ResizeSizeMode::Absolute, OIV::ResizeSizeMode::Relative})
        for (const auto units : {OIV::ResizeUnits::Pixels, OIV::ResizeUnits::Inches, OIV::ResizeUnits::Centimeters})
        {
            OIV::ResizeFormController form(platform, {},
                                           {.originalBounds = {{0, 0}, {130, 260}},
                                            .defaults       = {.size     = OIV::ResizeFormDimensions{2.125, 4.25},
                                                               .sizeMode = mode,
                                                               .units    = units,
                                                               .dpi      = 120,
                                                               .aspect   = OIV::ResizeAspectRatio::DoNotPreserve}});
            form.Show(nullptr);
            OIV::ResizeFormTestAccess::CheckInitialization(form);
            form.Hide();
        }
    REQUIRE(platform.Shutdown() == LWS::Result::Success);
}
#endif

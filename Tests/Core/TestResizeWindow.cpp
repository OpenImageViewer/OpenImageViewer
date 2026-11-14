#include <catch2/catch_test_macros.hpp>
#if defined(OIV_TEST_VIEWER) && (defined(LWS_HAS_WIN32_BACKEND) || defined(LWS_HAS_WAYLAND_BACKEND))
    #include "UI/ResizeForm.h"
    #include <LWSUI/UIHost.hpp>
    #include <thread>
    #include <chrono>
    #ifdef LWS_HAS_WIN32_BACKEND
        #include <LWS/Win32/Platform.hpp>
        #include <LWS/Win32/WindowExtensions.hpp>
        #include <Windows.h>
    #endif

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
            REQUIRE(form.fInput.thumbwheel.acceleration == .5);
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

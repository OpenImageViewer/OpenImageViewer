#include "MouseGestureController.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    using Controller = OIV::MouseGestureController;

    Controller::Decision SetButton(Controller& controller, LWS::MouseButton button, bool pressed,
                                   bool mouseInside = true, const Controller::ButtonContext& context = {})
    {
        Controller::Decision decision;
        if (controller.UpdateButtonState(button, pressed, mouseInside))
            decision = controller.ResolveButton(button, pressed, mouseInside, context);
        return decision;
    }
}  // namespace

TEST_CASE("Mouse gesture controller gives selection cancellation precedence over fullscreen", "[input][mouse]")
{
    Controller controller;
    REQUIRE(controller.UpdateButtonState(LWS::MouseButton::Left, true, true));

    const auto cancelSelection = controller.OnMultiClick(LWS::MouseButton::Left, 2,
                                                         {.selectionActive = true, .multiFullScreen = false});
    REQUIRE(cancelSelection.action == Controller::Action::QueueCancelSelection);

    const auto consumePress = controller.ResolveButton(LWS::MouseButton::Left, true, true, {});
    REQUIRE(consumePress.action == Controller::Action::CancelInput);

    controller.Reset();
    REQUIRE(controller.UpdateButtonState(LWS::MouseButton::Left, true, true));
    const auto fullScreen = controller.OnMultiClick(LWS::MouseButton::Left, 2,
                                                    {.selectionActive = false, .multiFullScreen = true});
    REQUIRE(fullScreen.action == Controller::Action::QueueToggleFullScreen);
    REQUIRE(fullScreen.multiFullScreen);
}

TEST_CASE("Mouse gesture controller routes double right to clipboard paste", "[input][mouse]")
{
    Controller controller;
    REQUIRE(controller.UpdateButtonState(LWS::MouseButton::Right, true, true));

    const auto paste = controller.OnMultiClick(LWS::MouseButton::Right, 2, {});
    REQUIRE(paste.action == Controller::Action::QueuePaste);
    REQUIRE(controller.ResolveButton(LWS::MouseButton::Right, true, true, {}).action ==
            Controller::Action::CancelInput);
}

TEST_CASE("Mouse gesture controller recognizes both rocker directions once", "[input][mouse]")
{
    SECTION("right then left navigates backward")
    {
        Controller controller;
        const auto pan = SetButton(controller, LWS::MouseButton::Right, true);
        REQUIRE(pan.action == Controller::Action::None);
        REQUIRE(pan.pointerLock == Controller::PointerLockChange::Lock);
        REQUIRE(pan.contextMenuTimer == Controller::TimerChange::Start);

        const auto rocker = SetButton(controller, LWS::MouseButton::Left, true);
        REQUIRE(rocker.action == Controller::Action::JumpFiles);
        REQUIRE(rocker.fileStep == -1);
        REQUIRE(rocker.pointerLock == Controller::PointerLockChange::Unlock);
        REQUIRE(rocker.contextMenuTimer == Controller::TimerChange::Stop);
        REQUIRE(rocker.navigationTimer == Controller::TimerChange::Stop);
        REQUIRE(rocker.resetMultiClick);
        REQUIRE(rocker.stopAutoScroll);
        REQUIRE_FALSE(rocker.cancelSelection);
        REQUIRE(controller.IsRockerActive());

        REQUIRE(SetButton(controller, LWS::MouseButton::Left, false).action == Controller::Action::None);
        REQUIRE(SetButton(controller, LWS::MouseButton::Right, false).action == Controller::Action::None);
        REQUIRE_FALSE(controller.IsRockerActive());
    }

    SECTION("left then right navigates forward")
    {
        Controller controller;
        const auto pendingDrag = SetButton(controller, LWS::MouseButton::Left, true, true, {.windowed = true});
        REQUIRE(pendingDrag.action == Controller::Action::None);
        REQUIRE(pendingDrag.stopAutoScroll);

        const auto rocker = SetButton(controller, LWS::MouseButton::Right, true);
        REQUIRE(rocker.action == Controller::Action::JumpFiles);
        REQUIRE(rocker.fileStep == 1);
        REQUIRE_FALSE(rocker.cancelSelection);
    }
}

TEST_CASE("Rocker cancels an active selection before navigating", "[input][mouse]")
{
    Controller controller;
    const auto selection = SetButton(controller, LWS::MouseButton::Left, true, true, {.altPressed = true});
    REQUIRE(selection.action == Controller::Action::BeginSelection);

    const auto rocker = SetButton(controller, LWS::MouseButton::Right, true);
    REQUIRE(rocker.action == Controller::Action::JumpFiles);
    REQUIRE(rocker.fileStep == 1);
    REQUIRE(rocker.cancelSelection);
    REQUIRE(rocker.resetMultiClick);
}

TEST_CASE("Pan cancels its context menu only after intentional movement", "[input][mouse]")
{
    Controller controller;
    const auto beginPan = SetButton(controller, LWS::MouseButton::Right, true);
    REQUIRE(beginPan.action == Controller::Action::None);
    REQUIRE(beginPan.pointerLock == Controller::PointerLockChange::Lock);
    REQUIRE(beginPan.contextMenuTimer == Controller::TimerChange::Start);
    REQUIRE(beginPan.stopAutoScroll);

    const auto thresholdEdge = controller.Move({3, 4}, false);
    REQUIRE(thresholdEdge.action == Controller::Action::Pan);
    REQUIRE(thresholdEdge.contextMenuTimer == Controller::TimerChange::None);

    const auto beyondThreshold = controller.Move({1, 0}, false);
    REQUIRE(beyondThreshold.action == Controller::Action::Pan);
    REQUIRE((beyondThreshold.delta == LWS::Point{1, 0}));
    REQUIRE(beyondThreshold.contextMenuTimer == Controller::TimerChange::Stop);

    const auto endPan = SetButton(controller, LWS::MouseButton::Right, false);
    REQUIRE(endPan.action == Controller::Action::None);
    REQUIRE(endPan.pointerLock == Controller::PointerLockChange::Unlock);
    REQUIRE(endPan.contextMenuTimer == Controller::TimerChange::Stop);
    REQUIRE_FALSE(controller.IsRockerActive());
}

TEST_CASE("Visible context menu suppresses pan and reset permits a new pan", "[input][mouse]")
{
    Controller controller;
    REQUIRE(SetButton(controller, LWS::MouseButton::Right, true).pointerLock == Controller::PointerLockChange::Lock);
    REQUIRE(controller.Move({8, 3}, true).action == Controller::Action::None);

    controller.Reset();
    REQUIRE_FALSE(controller.IsRockerActive());
    REQUIRE(SetButton(controller, LWS::MouseButton::Right, true).pointerLock == Controller::PointerLockChange::Lock);
    REQUIRE(controller.Move({1, 0}, false).action == Controller::Action::Pan);
}

TEST_CASE("Window drag starts only after its movement threshold", "[input][mouse]")
{
    Controller controller;
    const auto pending = SetButton(controller, LWS::MouseButton::Left, true, true,
                                   {.controlPressed = true, .windowed = true});
    REQUIRE(pending.action == Controller::Action::None);
    REQUIRE(pending.stopAutoScroll);

    REQUIRE(controller.Move({3, 4}, false).action == Controller::Action::None);
    const auto drag = controller.Move({1, 0}, false);
    REQUIRE(drag.action == Controller::Action::BeginWindowDrag);
    REQUIRE(drag.windowDragOperation == LWS::WindowDragOperation::ResizeNearest);
    REQUIRE(drag.resetMultiClick);
    REQUIRE_FALSE(controller.IsRockerActive());
}

TEST_CASE("Quick browse state follows side button capture and rocker cancellation", "[input][mouse]")
{
    Controller controller;
    const auto start = SetButton(controller, LWS::MouseButton::X1, true);
    REQUIRE(start.navigationTimer == Controller::TimerChange::Start);
    REQUIRE(controller.GetNavigationDirection() == -1);

    REQUIRE(SetButton(controller, LWS::MouseButton::Right, true).pointerLock == Controller::PointerLockChange::Lock);
    const auto rocker = SetButton(controller, LWS::MouseButton::Left, true);
    REQUIRE(rocker.action == Controller::Action::JumpFiles);
    REQUIRE(rocker.navigationTimer == Controller::TimerChange::Stop);

    controller.Reset();
    REQUIRE(controller.GetNavigationDirection() == 0);
    REQUIRE_FALSE(controller.IsCaptured(LWS::MouseButton::X1));

    REQUIRE(SetButton(controller, LWS::MouseButton::X2, true).navigationTimer == Controller::TimerChange::Start);
    REQUIRE(controller.GetNavigationDirection() == 1);
    REQUIRE(SetButton(controller, LWS::MouseButton::X2, false).navigationTimer == Controller::TimerChange::Stop);
}

TEST_CASE("Middle button auto scroll does not collide with an active gesture", "[input][mouse]")
{
    Controller controller;
    REQUIRE(SetButton(controller, LWS::MouseButton::Middle, true).action == Controller::Action::ToggleAutoScroll);

    controller.Reset();
    REQUIRE(SetButton(controller, LWS::MouseButton::Right, true).pointerLock == Controller::PointerLockChange::Lock);
    REQUIRE(SetButton(controller, LWS::MouseButton::Middle, true).action == Controller::Action::None);
    REQUIRE(controller.Move({1, 0}, false).action == Controller::Action::Pan);
}

TEST_CASE("Mouse gesture reset clears every owned gesture state", "[input][mouse]")
{
    Controller controller;

    SECTION("pending window drag")
    {
        REQUIRE(SetButton(controller, LWS::MouseButton::Left, true, true, {.windowed = true}).stopAutoScroll);
    }

    SECTION("selection")
    {
        REQUIRE(SetButton(controller, LWS::MouseButton::Left, true, true, {.altPressed = true}).action ==
                Controller::Action::BeginSelection);
    }

    SECTION("pan")
    {
        REQUIRE(SetButton(controller, LWS::MouseButton::Right, true).pointerLock ==
                Controller::PointerLockChange::Lock);
    }

    SECTION("rocker")
    {
        REQUIRE(SetButton(controller, LWS::MouseButton::Right, true).pointerLock ==
                Controller::PointerLockChange::Lock);
        REQUIRE(SetButton(controller, LWS::MouseButton::Left, true).action == Controller::Action::JumpFiles);
    }

    controller.Reset();
    REQUIRE_FALSE(controller.IsRockerActive());
    REQUIRE_FALSE(controller.IsCaptured(LWS::MouseButton::Left));
    REQUIRE_FALSE(controller.IsCaptured(LWS::MouseButton::Right));
    REQUIRE(controller.GetNavigationDirection() == 0);
}

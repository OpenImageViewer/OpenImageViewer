#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "ViewerApplication.h"

#include <Windows.h>
#include <Version.h>

#include <functions.h>
#include <ApiGlobal.h>
#include <Win32/Win32Window.h>
#include <Win32/Win32Helper.h>
#include <Win32/MonitorInfo.h>
#include <Win32/FileDialog.h>

#include <LInput/Keys/KeyCombination.h>
#include <LInput/Keys/KeyBindings.h>
#include <LInput/Buttons/Extensions/ButtonsStdExtension.h>
#include <LInput/Mouse/MouseButton.h>

#include <LLUtils/Exception.h>
#include <LLUtils/FileHelper.h>
#include <LLUtils/PlatformUtility.h>
#include <LLUtils/StringUtility.h>
#include <LLUtils/UniqueIDProvider.h>
#include <LLUtils/Logging/LogPredefined.h>
#include <LLUtils/Logging/Logger.h>
#include <LLUtils/FileSystemHelper.h>
#include <LLUtils/Rect.h>

#include "Helpers/OIVHelper.h"
#include "Helpers/ClipboardSetup.h"
#include "Helpers/MessageHelper.h"
#include "Helpers/ShellIntegrationHelper.h"
#include "Helpers/ShellCommandHandler.h"

#include "win32/UserMessages.h"
#include "OIVCommands.h"

#include "OIVImage/OIVFileImage.h"
#include "OIVImage/OIVRawImage.h"
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"

#include "ContextMenu.h"
#include "globals.h"
#include "ConfigurationLoader.h"
#include "CommandRegistry.h"
#include "ExceptionHandler.h"
#include <oivappcore/ColorCountPolicy.h>
#include <oivappcore/ColorCorrectionCommandPolicy.h>
#include <oivappcore/FileChangePolicy.h>
#include <oivappcore/FrameLimiterPolicy.h>
#include <oivappcore/ImageEditPolicy.h>
#include <oivappcore/ImageFormatCatalogPolicy.h>
#include <oivappcore/ImageLoadPresentationPolicy.h>
#include <oivappcore/ImageTransformCommandPolicy.h>
#include <oivappcore/InputGesturePolicy.h>
#include <oivappcore/OIVImageHelper.h>
#include <oivappcore/SelectionWorkflowPolicy.h>
#include <oivappcore/SequencerPolicy.h>
#include <oivappcore/SortCommandPolicy.h>
#include <oivappcore/SubImagePolicy.h>
#include <oivappcore/ViewActionController.h>
#include <oivappcore/ViewCommandPolicy.h>
#include <oivappcore/ViewerPresentationPolicy.h>
#include <oivshared/PixelHelper.h>
#include <ImageUtil/ImageUtil.h>
#include "InterThreadMessages.h"

#include "resource.h"

namespace OIV
{
    namespace
    {
        struct FileIndexResidencyReadyData
        {
            std::wstring fileName;
            IMCodec::ImageSharedPtr image;
        };

        struct FolderLoadResidencyReadyData
        {
            BrowseResidencyManager::FileListSnapshot snapshot;
            std::wstring fileName;
            IMCodec::ImageSharedPtr image;
        };
    }  // namespace

    void ViewerApplication::OnMouseEvent(const LInput::ButtonStdExtension<MouseButtonType>::ButtonEvent& btnEvent)
    {
        using namespace LInput;
        bool isMouseCursorOnTopOfWindowAndInsideClientRect = fWindow.IsUnderMouseCursor() &&
                                                             fWindow.IsMouseCursorInClientRect();
        if (btnEvent.button == MouseButton::Middle && btnEvent.eventType == EventType::Pressed &&
            isMouseCursorOnTopOfWindowAndInsideClientRect)
        {
            fAutoScroll->ToggleAutoScroll();
            if (fAutoScroll->IsAutoScrolling() == false)
            {
                fWindow.SetCursorType(::OIV::Win32::MainWindow::CursorType::SystemDefault);
                fAutoScrollAnchor.reset();
            }
            else
            {
                std::wstring anchorPath = LLUtils::StringUtility::ToNativeString(
                                              LLUtils::PlatformUtility::GetExeFolder()) +
                                          L"./Resources/Cursors/arrow-C.cur";
                std::unique_ptr<OIVFileImage> fileImage = std::make_unique<OIVFileImage>(anchorPath);
                if (fileImage->Load(&fImageLoader, IMCodec::PluginTraverseMode::AnyPlugin) == RC_Success)
                {
                    fileImage->SetImageRenderMode(OIV_Image_Render_mode::IRM_Overlay);
                    fileImage->SetPosition(static_cast<LLUtils::PointF64>(
                        static_cast<LLUtils::PointI32>(fWindow.GetMousePosition()) -
                        static_cast<LLUtils::PointI32>(fileImage->GetImage()->GetDimensions()) / 2));
                    fileImage->SetScale({1.0, 1.0});
                    fileImage->SetOpacity(0.5);
                    fileImage->SetVisible(true);
                    fAutoScrollAnchor = std::move(fileImage);
                    // TODO: do we need update here when loading the cursor anchor ?
                }
            }
        }

        if (btnEvent.button == MouseButton::Left && btnEvent.eventType == EventType::Released)
        {
            fWindow.SetLockMouseToWindowMode(::Win32::LockMouseToWindowMode::NoLock);
        }

        using namespace ::Win32;
        LockMouseToWindowMode lockMode = LockMouseToWindowMode::NoLock;
        const auto& mouseState = fMouseDevicesState.find(btnEvent.parent->GetID())->second;
        const bool IsRightDown = mouseState.GetButtonState(MouseButtonType::Right) == ButtonState::Down;
        const bool IsLeftDown = mouseState.GetButtonState(MouseButtonType::Left) == ButtonState::Down;
        const bool IsRightCatured = fMouseCaptureState.IsCaptured(MouseButtonType::Right);

        if (btnEvent.button == MouseButton::Left)
        {
            if (btnEvent.eventType == EventType::Pressed && IsRightDown &&
                isMouseCursorOnTopOfWindowAndInsideClientRect)
            {
                // Rocker gesture - navigate backward
                fRockerGestureActivate = true;
                fContextMenuTimer.SetInterval(0);
                JumpFiles(-1);
            }
            else if (IsRightDown == false && IsRightCatured == false)
            {
                // Window drag and resize
                if (Win32Helper::IsKeyPressed(VK_MENU) == false && fWindow.IsFullScreen() == false)
                {
                    if (Win32Helper::IsKeyPressed(VK_CONTROL) == true)
                        lockMode = LockMouseToWindowMode::LockResize;
                    else
                        lockMode = LockMouseToWindowMode::LockMove;
                }
            }
            if (btnEvent.eventType == EventType::Released)
            {
                lockMode = LockMouseToWindowMode::NoLock;
            }

            fWindow.SetLockMouseToWindowMode(lockMode);

            if (Win32Helper::IsKeyPressed(VK_MENU))
            {
                SelectionRect::Operation op = SelectionRect::Operation::NoOp;
                if (btnEvent.eventType == EventType::Pressed && fWindow.IsUnderMouseCursor())
                    op = SelectionRect::Operation::BeginDrag;
                else if (btnEvent.eventType == EventType::Released && fWindow.IsUnderMouseCursor())
                    op = SelectionRect::Operation::EndDrag;

                fSelectionRect.SetSelection(op, SnapToScreenSpaceImagePixels(fWindow.GetMousePosition()));
                SaveImageSpaceSelection();
            }
        }
        if (btnEvent.button == MouseButton::Back || btnEvent.button == MouseButton::Forward)
        {
            if (btnEvent.eventType == EventType::Pressed && fWindow.IsUnderMouseCursor())
            {
                fTimerNavigation.SetInterval(fQuickBrowseDelay);
            }
            else
            {
                if (fMouseCaptureState.IsCaptured(MouseButton::Back) == false &&
                    fMouseCaptureState.IsCaptured(MouseButton::Forward) == false)
                    fTimerNavigation.SetInterval(0);
            }
        }

        if (btnEvent.button == MouseButton::Right && btnEvent.eventType == EventType::Pressed &&
            isMouseCursorOnTopOfWindowAndInsideClientRect)
        {
            // Rocker gesture - navigate forward
            if (IsLeftDown)
            {
                fRockerGestureActivate = true;
                fContextMenuTimer.SetInterval(0);
                JumpFiles(1);
            }

            if (fContextMenuTimer.GetInterval() == 0 && fRockerGestureActivate == false)
            {
                fContextMenuTimer.SetInterval(500);
                fDownPosition = ::Win32::Win32Helper::GetMouseCursorPosition();
            }
        }
    }

    void ViewerApplication::OnMouseInput(const LInput::RawInput::RawInputEventMouse& mouseInput)
    {
        using namespace LInput;
        using namespace ::Win32;

        const auto& mouseState = fMouseDevicesState.find(mouseInput.deviceIndex)->second;

        // const bool IsLeftDown = mouseState.GetButtonState(MouseState::Button::Left) == MouseState::State::Down;
        const bool IsLeftDown = mouseState.GetButtonState(MouseButtonType::Left) == ButtonState::Down;
        const bool IsRightDown = mouseState.GetButtonState(MouseButtonType::Right) == ButtonState::Down;

        const bool IsRightCatured = fMouseCaptureState.IsCaptured(MouseButtonType::Right);
        const bool IsLeftCaptured = fMouseCaptureState.IsCaptured(MouseButtonType::Left);
        // const bool IsRightDown = mouseState.GetButtonState(MouseState::Button::Right) == MouseState::State::Down;
        // const bool IsLeftReleased = evnt->GetButtonEvent(MouseState::Button::Left) == MouseState::EventType::Released;

        const bool isMouseUnderCursor = fWindow.IsUnderMouseCursor();

        // Quick browse feature
        // const bool isNavigationBackwardDown = (mouseState.GetButtonState(MouseButtonType::Back) ==
        // ButtonState::Down); const bool isNavigationBackwardUp = (mouseState.GetButtonState(MouseButtonType::Back) ==
        // ButtonState::Up); const bool isNavigationBackwardUp = (mouseState.GetButtonState(MouseState::Button::Third)
        // == MouseState::State::Up); const bool isNavigationForwardDown =
        // mouseState.GetButtonState(MouseButtonType::Forward) == ButtonState::Down; const bool isNavigationForwardUp =
        // mouseState.GetButtonState(MouseButtonType::Forward) == ButtonState::Up;

        // Selection rect
        if (Win32Helper::IsKeyPressed(VK_MENU))
        {
            if (IsLeftCaptured)
            {
                auto snappedPOsition = SnapToScreenSpaceImagePixels(fWindow.GetMousePosition());
                fSelectionRect.SetSelection(SelectionRect::Operation::Drag, snappedPOsition);
                SaveImageSpaceSelection();
            }
        }

        if (IsRightCatured == true && fContextMenu->IsVisible() == false)
        {
            if (mouseInput.deltaX != 0 || mouseInput.deltaY != 0)
                Pan(LLUtils::PointF64(mouseInput.deltaX, mouseInput.deltaY));
        }

        LONG wheelDelta = mouseInput.wheelDelta;

        if (wheelDelta != 0)
        {
            // Browse files
            if (isMouseUnderCursor && Win32Helper::IsKeyPressed(VK_MENU))
            {
                ExecutePredefinedCommand(wheelDelta > 0 ? "PreviousSubImage" : "NextSubImage");
            }
            else if (isMouseUnderCursor && Win32Helper::IsKeyPressed(VK_SHIFT))
            {
                ExecutePredefinedCommand(wheelDelta > 0 ? "PreviousImageInFolder" : "NextImageInFolder");
            }
            else if (IsRightCatured || isMouseUnderCursor)
            {
                POINT mousePos = fWindow.GetMousePosition();
                // 20% percent zoom in each wheel step
                if (IsRightCatured)
                    //  Zoom to center of the client area if currently panning.
                    Zoom(wheelDelta * 0.2);
                else
                    Zoom(wheelDelta * 0.2, mousePos.x, mousePos.y);
            }
        }

        if (IsRightDown)
        {
            LLUtils::PointI32 currentPosition = Win32Helper::GetMouseCursorPosition();
            if (currentPosition.DistanceSquared(fDownPosition) > 25)
                fContextMenuTimer.SetInterval(0);
        }
        else
        {
            fContextMenuTimer.SetInterval(0);
        }

        if (IsLeftDown == false && IsRightDown == false)
            fRockerGestureActivate = false;
    }

    void ViewerApplication::OnRawInput(const LInput::RawInput::RawInputEvent& evnt)
    {
        using namespace LInput;
        if (evnt.deviceType == RawInput::RawInputDeviceType::Mouse)
        {
            const auto& mouseEvent = static_cast<const RawInput::RawInputEventMouse&>(evnt);

            // Add button states for multiple mouses.
            auto it = fMouseDevicesState.find(evnt.deviceIndex);
            // if mouse ID not found add new buttonstates entry.
            if (it == std::end(fMouseDevicesState))
            {
                it = fMouseDevicesState.emplace(evnt.deviceIndex, decltype(fMouseDevicesState)::mapped_type()).first;
                // Add standard extension
                auto stdExtension = std::make_shared<ButtonStdExtension<MouseButtonType>>(evnt.deviceIndex, 250, 0);
                stdExtension->OnButtonEvent.Add(std::bind(&ViewerApplication::OnMouseEvent, this, std::placeholders::_1));
                it->second.AddExtension(std::static_pointer_cast<IButtonStateExtension<MouseButtonType>>(stdExtension));

                // Add multitap extension for click, double click and triple click
                /*
                auto multitapextension = std::make_shared<MultitapExtension<MouseButtonType>>(evnt.deviceIndex, 500, 2);
                multitapextension->OnButtonEvent.Add(std::bind(&ViewerApplication::OnMouseMultiTap, this,std::placeholders::_1));
                it->second.AddExtension(std::static_pointer_cast<IButtonStateExtension<MouseButtonType>>(multitapextension));
                */
            }

            for (size_t i = 0; i < RawInput::MaxMouseButtons; i++)
            {
                it->second.SetButtonState(
                    static_cast<decltype(fMouseDevicesState)::mapped_type::underlying_button_type>(i),
                    mouseEvent.buttonState[i]);

                fMouseCaptureState.Update(static_cast<MouseButton>(i),
                                           mouseEvent.buttonState[i],
                                           fWindow.IsUnderMouseCursor());

                fMouseClickEventHandler.SetButtonState(static_cast<MouseButton>(i), mouseEvent.buttonState[i]);
            }

            fMouseClickEventHandler.SetMouseDelta(mouseEvent.deltaX, mouseEvent.deltaY);

            OnMouseInput(mouseEvent);
        }
    }

    void ViewerApplication::OnMouseMultiClick(const MouseMultiClickHandler::EventArgs& args)
    {
        using namespace LInput;
        if (args.clickCount == 2 && fWindow.IsMouseCursorInClientRect() && fWindow.IsUnderMouseCursor())
        {
            const auto& mouseState = fMouseDevicesState.begin()->second;
            const bool IsRightDown = mouseState.GetButtonState(MouseButtonType::Right) == ButtonState::Down;
            const bool IsLeftDown = mouseState.GetButtonState(MouseButtonType::Left) == ButtonState::Down;

            if (args.button == MouseButton::Left)
            {
                if (IsRightDown == false)
                {
                    if (fSelectionRect.GetOperation() != SelectionRect::Operation::NoOp)
                    {
                        CancelSelection();
                    }
                    else
                    {
                        ToggleFullScreen(::Win32::Win32Helper::IsKeyPressed(VK_MENU) ? true : false);
                    }
                }
            }

            if (args.button == MouseButton::Right)
            {
                if (IsLeftDown == false)
                {
                    ExecutePredefinedCommand("PasteImageFromClipboard");
                }
            }
        }
    }

    bool ViewerApplication::handleKeyInput(const ::Win32::EventWinMessage* evnt)
    {
        LInput::KeyCombination keyCombination = LInput::KeyCombination::FromVirtualKey(
            static_cast<uint32_t>(evnt->message.wParam), static_cast<uint32_t>(evnt->message.lParam));
        LInput::KeyBindings<BindingElement>::ConcreteBindingType bindings;
        bool result = true;
        if (result == fKeyBindings.GetBinding(keyCombination, bindings))
        {
            for (const auto& binding : bindings)
                result |= ExecutePredefinedCommand(binding.commandDescription);
        }

        return result;
    }

    LRESULT ViewerApplication::ClientWindwMessage(const ::Win32::Event* evnt1)
    {
        using namespace ::Win32;
        const EventWinMessage* evnt = dynamic_cast<const EventWinMessage*>(evnt1);
        if (evnt == nullptr)
            return 0;

        const WinMessage& message = evnt->message;

        LRESULT retValue = 0;
        switch (message.message)
        {
            case WM_SIZE:
                fRefreshOperation.Begin();
                UpdateWindowSize();
                fRefreshOperation.End();
                break;
        }
        return retValue;
    }

    void ViewerApplication::SetTopMostUserMesage()
    {
        SetUserMessage(ViewerPresentationPolicy::FormatTopMostMessage(fTopMostCounter),
                       static_cast<GroupID>(UserMessageGroups::WindowOnTop),
                       MessageFlags::Interchangeable | MessageFlags::ManualRemove);
    }

    bool ViewerApplication::GetAppActive() const
    {
        return fIsActive;
    }

    void ViewerApplication::SetAppActive(bool active)
    {
        if (active != fIsActive)
        {
            fIsActive = active;
            if (fIsActive == true && fFileReloadPolicy.HasPendingReloadFor(GetOpenedFileName()))
            {
                PerformReloadFile(GetOpenedFileName());
            }
            else
            {
                UpdateTitle();
            }
        }
    }

    void ViewerApplication::ProcessTopMost()
    {
        if (fTopMostCounter > 0)
        {
            fTopMostCounter--;

            if (fTopMostCounter == 0)
            {
                fTimerTopMostRetention.SetInterval(0);
                fWindow.SetAlwaysOnTop(false);
                fMessageManager->RemoveGroup(static_cast<GroupID>(UserMessageGroups::WindowOnTop));
            }
            else
                SetTopMostUserMesage();
        }
    }

    bool ViewerApplication::HandleWinMessageEvent(const ::Win32::EventWinMessage* evnt)
    {
        bool handled = false;

        const ::Win32::WinMessage& uMsg = evnt->message;
        switch (uMsg.message)
        {
            case WM_SHOWWINDOW:
                if (fIsFirstFrameDisplayed == false && uMsg.wParam == TRUE)
                {
                    PostMessage(fWindow.GetHandle(), Win32::UserMessage::PRIVATE_WN_FIRST_FRAME_DISPLAYED, 0, 0);
                    fIsFirstFrameDisplayed = true;
                }
                break;
            case Win32::UserMessage::PRIVATE_WN_FIRST_FRAME_DISPLAYED:
                AfterFirstFrameDisplayed();
                break;

            case Win32::UserMessage::PRIVATE_WN_AUTO_SCROLL:
                fAutoScroll->PerformAutoScroll();
                break;
            case Win32::UserMessage::PRIVATE_WM_NOTIFY_FILE_CHANGED:
                OnFileChangedImpl(reinterpret_cast<FileWatcher::FileChangedEventArgs*>(uMsg.wParam));
                break;

            case WM_COPYDATA:
            {
                COPYDATASTRUCT* cds = (COPYDATASTRUCT*) uMsg.lParam;
                if (uMsg.wParam == ::OIV::Win32::UserMessage::PRIVATE_WM_LOAD_FILE_EXTERNALLY)
                {
                    wchar_t* fileToLoad = reinterpret_cast<wchar_t*>(cds->lpData);
                    LoadFile(fileToLoad, IMCodec::PluginTraverseMode::NoTraverse);
                    fWindow.SetVisible(true);
                }
            }
            break;

            case WM_SYSKEYUP:
            case WM_KEYUP:
            {
                using namespace LInput;
                KeyCombination keyCombination = KeyCombination::FromVirtualKey(
                    static_cast<uint32_t>(evnt->message.wParam), static_cast<uint32_t>(evnt->message.lParam));

                bool isAltup = (keyCombination.keydata().keycode == KeyCode::LALT ||
                                keyCombination.keydata().keycode == KeyCode::RIGHTALT ||
                                keyCombination.keydata().keycode == KeyCode::RALT);

                if (isAltup)
                    fDoubleTap.SetState(false);
            }
            break;
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                handled = handleKeyInput(evnt);
                break;

            case WM_MOUSEMOVE:
                UpdateTexelPos();
                break;
            case WM_CLOSE:
                CloseApplication(false);
                break;
            case WM_ACTIVATE:
                SetAppActive(uMsg.wParam != WA_INACTIVE);
                break;
        }

        return handled;
    }

    void ViewerApplication::CloseApplication(bool closeToTray)
    {
        HANDLE mutex = CreateMutex(
            NULL, FALSE, (LLUtils::native_string_type(Globals::ProgramGuid) + LLUTILS_TEXT("_CLOSEAPP")).c_str());
        if (mutex == nullptr)
        {
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex cannot be created.");
        }

        const DWORD result = WaitForSingleObject(mutex,      // handle to mutex
                                                 INFINITE);  // no time-out interval

        switch (result)
        {
            case WAIT_OBJECT_0:

                fWindow.SetVisible(false);

                if (closeToTray == false || FindTrayBarWindow() != nullptr)
                {
                    fWindow.Destroy();
                }
                else
                {
                    fWindow.SetIsTrayWindow(true);
                }

                break;
            default:
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex ownership cannot be acquired.");
        }

        if (!ReleaseMutex(mutex))
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex cannot be released.");

        if (CloseHandle(mutex) == FALSE)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex cannot be closed.");
    }

    bool ViewerApplication::HandleFileDragDropEvent(const ::Win32::EventDdragDropFile* event_ddrag_drop_file)
    {
        std::wstring normalizedPath =
            std::filesystem::path(event_ddrag_drop_file->fileName).lexically_normal().wstring();
        if (LoadFileOrFolder(normalizedPath,
                             IMCodec::PluginTraverseMode::AnyPlugin | IMCodec::PluginTraverseMode::AnyFileType))
        {
            fWindow.SetForground();
            return true;
        }

        return false;
    }

    bool ViewerApplication::HandleClientWindowMessages(const ::Win32::Event* evnt1)
    {
        using namespace ::Win32;
        const EventWinMessage* evnt = dynamic_cast<const EventWinMessage*>(evnt1);

        if (evnt != nullptr)
        {
            return ClientWindwMessage(evnt);
        }
        return false;
    }

    bool ViewerApplication::HandleMessages(const ::Win32::Event* evnt1)
    {
        using namespace ::Win32;
        const EventWinMessage* evnt = dynamic_cast<const EventWinMessage*>(evnt1);

        if (evnt != nullptr)
            return HandleWinMessageEvent(evnt);

        const EventDdragDropFile* dragDropEvent = dynamic_cast<const EventDdragDropFile*>(evnt1);

        if (dragDropEvent != nullptr)
            return HandleFileDragDropEvent(dragDropEvent);

        return false;
    }
}  // namespace OIV

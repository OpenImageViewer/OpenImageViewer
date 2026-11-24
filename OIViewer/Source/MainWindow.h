#pragma once

#include "ImageControl.h"
#include "UI/InterfacePreferences.h"
#include <LWSUI/Menu.hpp>

#include <LWS/Bitmap.hpp>
#include <LWS/Cursor.hpp>
#include <LWS/Window.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <filesystem>
#include <functional>

namespace LWSUI
{
    class UIHost;
    class Label;
    class Panel;
    class SplitPanel;
}  // namespace LWSUI

namespace OIV
{
    class MainWindow
    {
      public:

        enum class CursorType
        {
            SystemDefault,
            Arrow,
            East,
            NorthEast,
            North,
            NorthWest,
            West,
            SouthWest,
            South,
            SouthEast,
            SizeAll,
            ResizeEW,
            ResizeNS,
            ResizeNESW,
            ResizeNWSE,
            SelectionMove,
            Count
        };

        explicit MainWindow(LWS::PlatformContext& platform, const std::filesystem::path& resources);
        ~MainWindow();

        [[nodiscard]] LWS::Window& GetWindow() { return fWindow; }
        [[nodiscard]] const LWS::Window& GetWindow() const { return fWindow; }

        // Set before Create; the custom close button follows the viewer's orderly shutdown path.
        std::function<void()> requestClose;
        [[nodiscard]] LWS::Result Create(const LWS::WindowConfig& config = {});
        [[nodiscard]] LWS::Result SetVisible(bool visible);

        [[nodiscard]] bool GetShowImageControl() const;
        [[nodiscard]] bool GetShowStatusBar() const;
        [[nodiscard]] LWS::PixelSize GetCanvasPixelSize() const;
        [[nodiscard]] LWS::Point GetCanvasMousePosition() const;

        ImageControl& GetImageControl();
        void SetCursorType(CursorType type);
        void ShowCanvas();
        void UpdateLayout();
        [[nodiscard]] LWS::WindowMode GetWindowMode() const;
        [[nodiscard]] LWS::Result SetWindowMode(LWS::WindowMode mode);
        void FitToParent();
        void SetTitle(const LWS::string_type& title);
        void SetMenuProvider(std::function<std::vector<LWSUI::MenuBarItem>()> provider);
        void RefreshMenu();
        void ApplyInterfacePreferences(const InterfacePreferences& preferences);
        bool MenuOwnsKeyboard() const;
        bool DeferShortcutToMenu(LWS::KeyCode key, bool control, bool alt, bool shift) const;
        std::function<void()> restoreMenu;
        const LWS::BitmapSharedPtr& ApplicationIcon() const { return fApplicationIcon; }
        [[nodiscard]] LWS::Result Destroy();
        void ShowStatusBar(bool show);
        void SetShowImageControl(bool show);
        LWS::Window& GetCanvasWindow();
        void SetStatusBarText(LLUtils::native_string_type message, int part, int type);
        void SetIsTrayWindow(bool isTrayWindow);
        static bool GetIsTrayWindow(LWS::Handle windowHandle);

      private:

        const std::filesystem::path fResources;

        struct NativeState;

        bool HandleWindowEvent(const LWS::AnyEvent& eventData);
        void PrepareMenu(const LWS::AnyEvent& event);
        [[nodiscard]] LWS::Result OnCreate();
        void SetApplicationIcon();
        void InitializeCursors();
        void UpdateNativeStatusBar(LWS::LogicalSize& canvasSize);
        void UpdateNativeWindowBorders(bool visible);

        LWS::Window fWindow;
        LWS::Window fCanvasWindow;
        LWS::BitmapSharedPtr fApplicationIcon;
        std::unique_ptr<LWSUI::UIHost> fChromeHost;
        InterfacePreferences fInterface;
        std::function<std::vector<LWSUI::MenuBarItem>()> fMenuProvider;
        LWSUI::Label* fTitleLabel     = nullptr;
        bool fShowStatusBar           = true;
        bool fShowImageControl        = false;
        bool fHasBeenShown            = false;
        CursorType fCurrentCursorType = CursorType::SystemDefault;
        std::array<LWS::Cursor, static_cast<size_t>(CursorType::Count) - 1> fCursors{
            LWS::Cursor::FromShape(LWS::CursorShape::Arrow),    LWS::Cursor::FromShape(LWS::CursorShape::SizeEW),
            LWS::Cursor::FromShape(LWS::CursorShape::SizeNESW), LWS::Cursor::FromShape(LWS::CursorShape::SizeNS),
            LWS::Cursor::FromShape(LWS::CursorShape::SizeNWSE), LWS::Cursor::FromShape(LWS::CursorShape::SizeEW),
            LWS::Cursor::FromShape(LWS::CursorShape::SizeNESW), LWS::Cursor::FromShape(LWS::CursorShape::SizeNS),
            LWS::Cursor::FromShape(LWS::CursorShape::SizeNWSE), LWS::Cursor::FromShape(LWS::CursorShape::SizeAll),
            LWS::Cursor::FromShape(LWS::CursorShape::SizeEW),   LWS::Cursor::FromShape(LWS::CursorShape::SizeNS),
            LWS::Cursor::FromShape(LWS::CursorShape::SizeNESW), LWS::Cursor::FromShape(LWS::CursorShape::SizeNWSE),
            LWS::Cursor::FromShape(LWS::CursorShape::SizeAll),
        };
        bool fCursorsInitialized         = false;
        ImageControl* fImageControl      = nullptr;
        LWSUI::Panel* fSidebarLayout     = nullptr;
        LWSUI::SplitPanel* fSidebarSplit = nullptr;
        std::unique_ptr<NativeState> fNativeState;
        LWS::EventConnection fEventConnection;
    };
}  // namespace OIV

#pragma once

#include "ImageControl.h"

#include <LWS/Cursor.hpp>
#include <LWS/Window.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

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
            Count
        };

        explicit MainWindow(LWS::PlatformContext& platform);
        ~MainWindow();

        [[nodiscard]] LWS::Window& GetWindow() { return fWindow; }
        [[nodiscard]] const LWS::Window& GetWindow() const { return fWindow; }

        [[nodiscard]] LWS::Result Create(const LWS::WindowConfig& config = {});

        [[nodiscard]] bool GetShowImageControl() const;
        [[nodiscard]] bool GetShowStatusBar() const;
        [[nodiscard]] LWS::PixelSize GetCanvasPixelSize() const;
        [[nodiscard]] LWS::Point GetCanvasMousePosition() const;
        [[nodiscard]] LWS::Point GetWindowMousePosition() const;

        ImageControl& GetImageControl();
        void SetCursorType(CursorType type);
        void ShowCanvas();
        void UpdateLayout();
        void ShowStatusBar(bool show);
        void SetShowImageControl(bool show);
        LWS::Window& GetCanvasWindow();
        void SetStatusBarText(LLUtils::native_string_type message, int part, int type);
        void SetIsTrayWindow(bool isTrayWindow);
        static bool GetIsTrayWindow(LWS::Handle windowHandle);

      private:

        struct NativeState;

        bool HandleWindowEvent(const LWS::AnyEvent& eventData);
        [[nodiscard]] LWS::Result OnCreate();
        [[nodiscard]] int32_t GetImageControlClientWidth(int32_t layoutWidth) const;
        void PrepareImageControlLayout();
        void SetApplicationIcon();
        void InitializeCursors();
        void UpdateNativeStatusBar(LWS::LogicalSize& canvasSize);
        [[nodiscard]] bool UseMainWindowAsCanvas() const;

        LWS::Window fWindow;
        LWS::Window fCanvasWindow;
        bool fShowStatusBar           = true;
        bool fShowImageControl        = false;
        CursorType fCurrentCursorType = CursorType::SystemDefault;
        std::array<LWS::Cursor, static_cast<size_t>(CursorType::Count) - 1> fCursors{
            LWS::Cursor::FromShape(LWS::CursorShape::Arrow),    LWS::Cursor::FromShape(LWS::CursorShape::SizeEW),
            LWS::Cursor::FromShape(LWS::CursorShape::SizeNESW), LWS::Cursor::FromShape(LWS::CursorShape::SizeNS),
            LWS::Cursor::FromShape(LWS::CursorShape::SizeNWSE), LWS::Cursor::FromShape(LWS::CursorShape::SizeEW),
            LWS::Cursor::FromShape(LWS::CursorShape::SizeNESW), LWS::Cursor::FromShape(LWS::CursorShape::SizeNS),
            LWS::Cursor::FromShape(LWS::CursorShape::SizeNWSE), LWS::Cursor::FromShape(LWS::CursorShape::SizeAll),
        };
        bool fCursorsInitialized    = false;
        bool fUseMainWindowAsCanvas = false;
        ImageControl fImageControl;
        std::unique_ptr<NativeState> fNativeState;
        LWS::EventConnection fEventConnection;
    };
}  // namespace OIV

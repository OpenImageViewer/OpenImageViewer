#include "ImageControl.h"

#include <LLUtils/Exception.h>
#include <Windows.h>

#include <LWS/Win32/WindowExtensions.hpp>

#include <algorithm>

namespace OIV
{
    struct ImageControl::NativeState
    {
        explicit NativeState(int lineWidth)
        {
            LOGFONT fontDescription{};
            fontDescription.lfHeight  = 16;
            fontDescription.lfWeight  = FW_NORMAL;
            fontDescription.lfQuality = CLEARTYPE_QUALITY;
            wcscpy_s(fontDescription.lfFaceName, LLUTILS_TEXT("Segoe UI"));
            font           = CreateFontIndirect(&fontDescription);
            grayBrush      = CreateSolidBrush(RGB(245, 249, 213));
            lightGrayBrush = CreateSolidBrush(RGB(224, 249, 213));
            blueBrush      = CreateSolidBrush(RGB(0, 0, 200));
            pen            = CreatePen(PS_SOLID, lineWidth, RGB(0, 0, 0));
            verticalPen    = CreatePen(PS_SOLID, 1, RGB(255, 0, 255));
            verticalPen2   = CreatePen(PS_SOLID, 1, RGB(0, 255, 0));
        }

        ~NativeState()
        {
            if (verticalPen2 != nullptr)
                DeleteObject(verticalPen2);
            if (verticalPen != nullptr)
                DeleteObject(verticalPen);
            if (pen != nullptr)
                DeleteObject(pen);
            if (grayBrush != nullptr)
                DeleteObject(grayBrush);
            if (lightGrayBrush != nullptr)
                DeleteObject(lightGrayBrush);
            if (blueBrush != nullptr)
                DeleteObject(blueBrush);
            if (font != nullptr)
                DeleteObject(font);
        }

        HFONT font            = nullptr;
        HBRUSH grayBrush      = nullptr;
        HBRUSH lightGrayBrush = nullptr;
        HBRUSH blueBrush      = nullptr;
        HPEN pen              = nullptr;
        HPEN verticalPen      = nullptr;
        HPEN verticalPen2     = nullptr;

        void Draw(HDC deviceContext, const ImageList& imageList, const LWS::ClientAreaMetrics& size) const;
    };

    ImageControl::ImageControl(LWS::PlatformContext& platform) : fWindow(platform)
    {
        InitializePlatformRendering();
        InitializeEvents();
    }

    ImageControl::~ImageControl() = default;

    void ImageControl::InitializePlatformRendering()
    {
        fNativeState    = std::make_unique<NativeState>(fImageList.GetLineWidth());
        auto connection = LWS::Win32::Listen(fWindow,
                                             [this](const LWS::Win32::PlatformEvent& event) -> std::optional<LRESULT>
                                             {
                                                 if (const auto* paint = std::get_if<LWS::Win32::PaintEvent>(&event))
                                                 {
                                                     const auto size = fWindow.GetClientAreaMetrics();
                                                     if (size.pixels.has_value() && size.pixels->x > 0 &&
                                                         size.pixels->y > 0)
                                                         fNativeState->Draw(paint->deviceContext, fImageList, size);
                                                     return 0;
                                                 }
                                                 if (const auto* scroll = std::get_if<LWS::Win32::VerticalScrollEvent>(
                                                         &event))
                                                 {
                                                     const int oldPos = fImageList.GetScrollPosition();
                                                     int newPos       = oldPos;
                                                     switch (scroll->action)
                                                     {
                                                         case LWS::Win32::VerticalScrollAction::PageDown:
                                                             ++newPos;
                                                             break;
                                                         case LWS::Win32::VerticalScrollAction::PageUp:
                                                             --newPos;
                                                             break;
                                                         case LWS::Win32::VerticalScrollAction::ThumbTrack:
                                                         case LWS::Win32::VerticalScrollAction::ThumbPosition:
                                                             newPos = scroll->position;
                                                             break;
                                                     }

                                                     if (newPos != oldPos)
                                                         SetImagePos(newPos);
                                                     return 0;
                                                 }
                                                 return std::nullopt;
                                             });
        if (!connection.has_value())
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to register required window listener");
        fPlatformConnection = std::move(*connection);
    }

    void ImageControl::RefreshScrollInfo()
    {
        const auto window = LWS::Win32::GetHwnd(fWindow);
        if (!window.has_value())
            return;
        fImageList.SetViewportHeight(fWindow.GetClientAreaMetrics().logical.y);
        const size_t deltaElements = fImageList.GetNumberOfElements() - fImageList.GetNumberOfDisplayedElements();
        SCROLLINFO info{};
        info.cbSize = sizeof(info);
        info.fMask  = SIF_ALL | SIF_DISABLENOSCROLL;
        info.nMin   = 0;
        info.nMax   = static_cast<int>(deltaElements);
        info.nPage  = 1;
        info.nPos   = fImageList.GetScrollPosition();
        SetScrollInfo(*window, SB_VERT, &info, TRUE);
        RequestRepaint();
    }

    void ImageControl::RequestRepaint()
    {
        const auto window = LWS::Win32::GetHwnd(fWindow);
        if (window.has_value())
            InvalidateRect(*window, nullptr, TRUE);
    }

    void ImageControl::UpdateScrollPosition()
    {
        const auto window = LWS::Win32::GetHwnd(fWindow);
        if (window.has_value())
            SetScrollPos(*window, SB_VERT, fImageList.GetScrollPosition(), TRUE);
    }

    std::intptr_t ImageControl::SendMessage(std::uint32_t message, std::uintptr_t wParam, std::intptr_t lParam)
    {
        return ::SendMessage(*LWS::Win32::GetHwnd(fWindow), message, wParam, lParam);
    }

    bool ImageControl::HandleWindowEvent(const LWS::AnyEvent& eventData)
    {
        if (const auto* button = std::get_if<LWS::EventMouseButton>(&eventData);
            button != nullptr && button->button == LWS::MouseButton::Left && button->pressed)
        {
            if (const auto selected = fImageList.GetImageIndexAt(button->position.y))
                fImageList.SetSelected(static_cast<int>(*selected));
            return true;
        }
        if (const auto* wheel = std::get_if<LWS::EventMouseWheel>(&eventData))
        {
            fImageList.Scroll(wheel->delta < 0 ? 1 : -1);
            return true;
        }
        if (std::holds_alternative<LWS::EventClientAreaSizeChanged>(eventData))
        {
            RefreshScrollInfo();
            return true;
        }
        return false;
    }

    void ImageControl::NativeState::Draw(HDC deviceContext, const ImageList& imageList,
                                         const LWS::ClientAreaMetrics& clientArea) const
    {
        const LWS::LogicalSize size = clientArea.logical;
        if (deviceContext == nullptr || size.x <= 0 || size.y <= 0)
            return;

        const int savedState = SaveDC(deviceContext);
        SetMapMode(deviceContext, MM_ANISOTROPIC);
        SetWindowExtEx(deviceContext, size.x, size.y, nullptr);
        SetViewportExtEx(deviceContext, clientArea.pixels->x, clientArea.pixels->y, nullptr);

        constexpr int imageDestWidth  = 51;
        constexpr int imageDestHeight = 51;
        constexpr int imagePosition   = 24;
        const HGDIOBJ oldPen          = SelectObject(deviceContext, pen);
        const HGDIOBJ oldFont         = SelectObject(deviceContext, font);
        SetBkMode(deviceContext, TRANSPARENT);

        const auto visible = imageList.GetVisibleRange();
        for (size_t index = visible.first; index < visible.last; ++index)
        {
            const ImageList::ImageDesc& image = imageList.GetImage(index);
            const LWS::Rect row               = imageList.GetRowRect(index, size.x);
            const auto topLeft                = row.GetCorner(LLUtils::Corner::TopLeft);
            const auto bottomRight            = row.GetCorner(LLUtils::Corner::BottomRight);
            RECT entryRect{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
            if (imageList.IsSelected(index))
            {
                FillRect(deviceContext, &entryRect, blueBrush);
                SetTextColor(deviceContext, RGB(255, 255, 255));
            }
            else
            {
                FillRect(deviceContext, &entryRect, index % 2 == 0 ? grayBrush : lightGrayBrush);
                SetTextColor(deviceContext, RGB(0, 0, 0));
            }

            const int separatorY = bottomRight.y - imageList.GetLineWidth();
            MoveToEx(deviceContext, 0, separatorY, nullptr);
            LineTo(deviceContext, size.x, separatorY);

            RECT textRect{0, topLeft.y + 4, 0, topLeft.y + 23};
            DrawText(deviceContext, image.title.c_str(), static_cast<int>(image.title.length()), &textRect,
                     DT_CALCRECT);
            const int textOffset = (size.x - (textRect.right - textRect.left)) / 2;
            textRect.right += textOffset;
            textRect.left += textOffset;
            DrawText(deviceContext, image.title.c_str(), static_cast<int>(image.title.length()), &textRect, DT_CENTER);

            if (image.bitmap != nullptr)
            {
                const auto bitmap     = image.bitmap->GetBuffer();
                const int finalWidth  = std::min<int>(imageDestWidth, bitmap.width);
                const int finalHeight = std::min<int>(imageDestHeight, bitmap.height);
                const int finalY      = finalHeight < imageDestHeight ? (imageList.GetRowHeight() - finalHeight) / 2
                                                                      : imagePosition;
                BITMAPINFO info{};
                info.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
                info.bmiHeader.biWidth       = static_cast<LONG>(bitmap.width);
                info.bmiHeader.biHeight      = -static_cast<LONG>(bitmap.height);
                info.bmiHeader.biPlanes      = 1;
                info.bmiHeader.biBitCount    = 32;
                info.bmiHeader.biCompression = BI_RGB;
                StretchDIBits(deviceContext, (size.x - finalWidth) / 2, topLeft.y + finalY, finalWidth, finalHeight, 0,
                              0, bitmap.width, bitmap.height, bitmap.pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
            }
        }

        SelectObject(deviceContext, verticalPen);
        MoveToEx(deviceContext, 0, 0, nullptr);
        LineTo(deviceContext, 0, size.y);
        SelectObject(deviceContext, verticalPen2);
        MoveToEx(deviceContext, 1, 0, nullptr);
        LineTo(deviceContext, 1, size.y);
        SelectObject(deviceContext, verticalPen);
        MoveToEx(deviceContext, 2, 0, nullptr);
        LineTo(deviceContext, 2, size.y);
        SelectObject(deviceContext, oldPen);
        SelectObject(deviceContext, oldFont);
        RestoreDC(deviceContext, savedState);
    }
}  // namespace OIV

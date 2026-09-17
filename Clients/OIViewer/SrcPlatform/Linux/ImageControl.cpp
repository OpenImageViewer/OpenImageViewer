#include "ImageControl.h"

#include <LLUtils/Exception.h>

#include <algorithm>
#include <limits>
#include <vector>

#include <pango/pangocairo.h>

namespace OIV
{
    struct ImageControl::NativeState
    {
        std::vector<std::byte> frame;

        void Draw(ImageControl& owner, const ImageList& imageList);
    };

    ImageControl::ImageControl(LWS::PlatformContext& platform) : fWindow(platform)
    {
        InitializePlatformRendering();
        InitializeEvents();
    }

    ImageControl::~ImageControl() = default;

    void ImageControl::InitializePlatformRendering()
    {
        fNativeState = std::make_unique<NativeState>();
    }

    void ImageControl::RefreshScrollInfo()
    {
        fImageList.SetViewportHeight(fWindow.GetClientAreaMetrics().logical.y);
        RequestRepaint();
    }

    void ImageControl::RequestRepaint()
    {
        fNativeState->Draw(*this, fImageList);
    }

    void ImageControl::UpdateScrollPosition() {}

    std::intptr_t ImageControl::SendMessage([[maybe_unused]] std::uint32_t message,
                                            [[maybe_unused]] std::uintptr_t wParam,
                                            [[maybe_unused]] std::intptr_t lParam)
    {
        LL_EXCEPTION_NOT_IMPLEMENT("Native subimage messages are not implemented on Linux");
    }

    bool ImageControl::HandleWindowEvent(const LWS::AnyEvent& eventData)
    {
        if (std::holds_alternative<LWS::EventPaint>(eventData))
        {
            fNativeState->Draw(*this, fImageList);
            return true;
        }
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

    void ImageControl::NativeState::Draw(ImageControl& owner, const ImageList& imageList)
    {
        if (!owner.GetWindow().IsVisible())
            return;

        const auto clientArea = owner.GetWindow().GetClientAreaMetrics();
        if (!clientArea.pixels.has_value())
            return;
        const LWS::LogicalSize size      = clientArea.logical;
        const LWS::PixelSize framebuffer = *clientArea.pixels;
        if (size.x <= 0 || size.y <= 0 || framebuffer.x <= 0 || framebuffer.y <= 0 ||
            framebuffer.x > std::numeric_limits<int>::max() / 4 ||
            static_cast<size_t>(framebuffer.x) >
                std::numeric_limits<size_t>::max() / 4U / static_cast<size_t>(framebuffer.y))
            return;

        constexpr int thumbnailSize  = 51;
        constexpr int titleHeight    = 19;
        constexpr int scrollbarWidth = 6;
        const int contentWidth       = std::max(0, size.x - scrollbarWidth);
        const size_t stride          = static_cast<size_t>(framebuffer.x) * 4U;
        frame.resize(stride * static_cast<size_t>(framebuffer.y));

        cairo_surface_t* surface = cairo_image_surface_create_for_data(reinterpret_cast<unsigned char*>(frame.data()),
                                                                       CAIRO_FORMAT_ARGB32, framebuffer.x,
                                                                       framebuffer.y, static_cast<int>(stride));
        cairo_t* context         = cairo_create(surface);
        // Positive logical dimensions and available pixels were checked before creating the drawing surface.
        const LWS::ContentScale scale = *clientArea.Scale();
        cairo_scale(context, scale.x, scale.y);
        cairo_set_source_rgb(context, 0.96, 0.97, 0.92);
        cairo_paint(context);

        PangoLayout* layout        = pango_cairo_create_layout(context);
        PangoFontDescription* font = pango_font_description_from_string("sans-serif 8");
        pango_layout_set_font_description(layout, font);
        pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
        pango_layout_set_width(layout,
                               std::min(contentWidth, std::numeric_limits<int>::max() / PANGO_SCALE) * PANGO_SCALE);
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

        const auto visible = imageList.GetVisibleRange();
        for (size_t index = visible.first; index < visible.last; ++index)
        {
            const ImageList::ImageDesc& image = imageList.GetImage(index);
            const LWS::Rect row               = imageList.GetRowRect(index, contentWidth);
            const auto topLeft                = row.GetCorner(LLUtils::Corner::TopLeft);
            const auto bottomRight            = row.GetCorner(LLUtils::Corner::BottomRight);
            const bool selected               = imageList.IsSelected(index);
            if (selected)
                cairo_set_source_rgb(context, 0.0, 0.0, 0.78);
            else if (index % 2U == 0)
                cairo_set_source_rgb(context, 0.96, 0.98, 0.84);
            else
                cairo_set_source_rgb(context, 0.88, 0.98, 0.84);
            cairo_rectangle(context, topLeft.x, topLeft.y, row.GetWidth(), row.GetHeight());
            cairo_fill(context);

            const double textColor = selected ? 1.0 : 0.0;
            cairo_set_source_rgb(context, textColor, textColor, textColor);
            pango_layout_set_text(layout, image.title.c_str(), static_cast<int>(image.title.size()));
            cairo_move_to(context, 0, topLeft.y + 3);
            pango_cairo_show_layout(context, layout);

            if (image.bitmap != nullptr)
            {
                const LWS::BitmapBuffer bitmap = image.bitmap->GetBuffer();
                cairo_surface_t* thumbnail     = cairo_image_surface_create_for_data(
                    reinterpret_cast<unsigned char*>(const_cast<std::byte*>(bitmap.pixels.data())), CAIRO_FORMAT_ARGB32,
                    static_cast<int>(bitmap.width), static_cast<int>(bitmap.height), static_cast<int>(bitmap.rowPitch));
                cairo_set_source_surface(context, thumbnail, (contentWidth - thumbnailSize) / 2,
                                         topLeft.y + titleHeight +
                                             (imageList.GetRowHeight() - titleHeight - thumbnailSize) / 2);
                cairo_paint(context);
                cairo_surface_destroy(thumbnail);
            }

            cairo_set_source_rgb(context, 0.0, 0.0, 0.0);
            cairo_set_line_width(context, imageList.GetLineWidth());
            cairo_move_to(context, 0, bottomRight.y - 1);
            cairo_line_to(context, contentWidth, bottomRight.y - 1);
            cairo_stroke(context);
        }

        const size_t displayed = imageList.GetNumberOfDisplayedElements();
        if (displayed < imageList.GetNumberOfElements())
        {
            constexpr int minimumThumbHeight = 16;
            const int thumbHeight            = std::max(minimumThumbHeight,
                                                        static_cast<int>(static_cast<size_t>(size.y) * displayed /
                                                                         imageList.GetNumberOfElements()));
            const int maxPos                 = static_cast<int>(imageList.GetNumberOfElements() - displayed);
            const int thumbY = maxPos > 0 ? (size.y - thumbHeight) * imageList.GetScrollPosition() / maxPos : 0;
            cairo_set_source_rgb(context, 0.75, 0.75, 0.75);
            cairo_rectangle(context, contentWidth, 0, scrollbarWidth, size.y);
            cairo_fill(context);
            cairo_set_source_rgb(context, 0.35, 0.35, 0.35);
            cairo_rectangle(context, contentWidth, thumbY, scrollbarWidth, thumbHeight);
            cairo_fill(context);
        }

        pango_font_description_free(font);
        g_object_unref(layout);
        cairo_destroy(context);
        cairo_surface_flush(surface);
        cairo_surface_destroy(surface);

        std::ignore = owner.GetWindow().PresentBitmap({
            .pixels   = frame,
            .format   = LWS::BitmapPixelFormat::Bgra8Premultiplied,
            .rowOrder = LWS::BitmapRowOrder::TopDown,
            .width    = static_cast<uint32_t>(framebuffer.x),
            .height   = static_cast<uint32_t>(framebuffer.y),
            .rowPitch = static_cast<uint32_t>(stride),
        });
    }
}  // namespace OIV

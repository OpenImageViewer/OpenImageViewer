#include "ImageControl.h"
#include <algorithm>
#include <cmath>

namespace OIV
{
    ImageControl::ImageControl()
    {
        hitTestBackground = true;
        SetCornerRadius(0);
        auto bar   = std::make_unique<LWSUI::ScrollBar>();
        fScrollBar = bar.get();
        SetContent(std::move(bar));
        fConnections.Connect(fScrollBar->OnEdit,
                             [this](double value, LWSUI::EditPhase)
                             {
                                 const auto count   = fImageList.GetNumberOfElements();
                                 const auto visible = std::max<size_t>(1, fImageList.GetNumberOfDisplayedElements());
                                 const auto maximum = count > visible ? count - visible : 0;
                                 fImageList.SetPos(static_cast<int>(std::lround(value * maximum)));
                             });
        fImageListSubscription = fImageList.Changed.Subscribe([this](ImageList::ChangeType) { RefreshScrollInfo(); });
    }
    ImageList& ImageControl::GetImageList()
    {
        return fImageList;
    }
    void ImageControl::RefreshScrollInfo()
    {
        fImageList.SetViewportHeight(static_cast<int32_t>(Bounds().height));
        const auto count   = fImageList.GetNumberOfElements();
        const auto visible = std::max<size_t>(1, fImageList.GetNumberOfDisplayedElements());
        const auto maximum = count > visible ? count - visible : 0;
        fScrollBar->SetVisible(maximum != 0);
        fScrollBar->SetPage(count ? std::min(1.0, double(visible) / count) : 1.0);
        fScrollBar->keyboardStep = maximum ? 1.0 / maximum : 1.0;
        fScrollBar->SetValue(maximum ? double(fImageList.GetScrollPosition()) / maximum : 0);
        Invalidate();
    }
    void ImageControl::OnArrange()
    {
        RefreshScrollInfo();
        const auto b      = Bounds();
        const float width = std::min(b.width, Style().scrollbarWidth);
        fScrollBar->Measure({width, b.height});
        fScrollBar->Arrange({b.x + b.width - width, b.y, width, b.height});
    }
    void ImageControl::OnRender(LWSUI::Canvas& canvas)
    {
        const LLUtils::Color cardBackground(237, 240, 243);
        const LLUtils::Color alternateBackground(222, 227, 233);
        const auto b = Bounds();
        canvas.Fill(b.x, b.y, b.width, b.height, Style().background);
        const float width = std::max(0.f, b.width - (fScrollBar->Visible() ? Style().scrollbarWidth : 0));
        {
            LWSUI::Canvas::ClipScope clip(canvas, b.x, b.y, width, b.height);
            const auto visible = fImageList.GetVisibleRange();
            for (size_t index = visible.first; index < visible.last; ++index)
            {
                const auto& image     = fImageList.GetImage(index);
                const float y         = b.y + (index - visible.first) * fImageList.GetRowHeight();
                const bool selected   = fImageList.IsSelected(index);
                const float cardWidth = std::max(0.f, width - 12);
                const float rowHeight = float(fImageList.GetRowHeight());
                canvas.FillRoundedRect(b.x + 6, y + 4, cardWidth, rowHeight - 8, 6,
                                       selected ? Style().selectedRow
                                                : (index % 2 ? alternateBackground : cardBackground));
                if (selected)
                {
                    canvas.StrokeRoundedRect(b.x + 6, y + 4, cardWidth, rowHeight - 8, 6, 1, Style().accent);
                    canvas.FillRoundedRect(b.x + 6, y + 22, 3, rowHeight - 44, 1.5f, Style().accent);
                }
                if (image.bitmap)
                {
                    const auto bitmap = image.bitmap->GetBuffer();
                    // Fit each thumbnail without distorting wide or tall source images.
                    const float scale = std::min(
                        {1.f, std::max(0.f, cardWidth - 16) / bitmap.width, 46.f / bitmap.height});
                    const float imageWidth  = bitmap.width * scale;
                    const float imageHeight = bitmap.height * scale;
                    const float left        = b.x + (width - imageWidth) / 2;
                    const float top         = y + 10 + (46 - imageHeight) / 2;
                    // Composite the original alpha over a checkerboard, without flattening the bitmap.
                    constexpr float checker = 5;
                    for (int row = 0; row * checker < imageHeight; ++row)
                        for (int column = 0; column * checker < imageWidth; ++column)
                            canvas.Fill(left + column * checker, top + row * checker,
                                        std::min(checker, imageWidth - column * checker),
                                        std::min(checker, imageHeight - row * checker),
                                        (row + column) % 2 ? Style().line : Style().surface);
                    canvas.Image(bitmap, left, top, imageWidth, imageHeight);
                }
                auto font   = Font();
                font.size   = 11;
                font.weight = selected ? 600 : 400;
                canvas.CenteredText(image.title, b.x + 12, y + rowHeight - 23, std::max(0.f, width - 24), 16,
                                    selected ? Style().selectionForeground : Style().muted, font);
            }
        }
        fScrollBar->Render(canvas);
    }
    bool ImageControl::OnInput(const LWSUI::Input& input)
    {
        bool handled = false;
        if (input.kind == LWSUI::InputKind::Down && input.button == LWS::MouseButton::Left)
        {
            if (const auto index = fImageList.GetImageIndexAt(static_cast<int>(input.y - Bounds().y)))
                fImageList.SetSelected(static_cast<int>(*index));
            handled = true;
        }
        else if (input.kind == LWSUI::InputKind::Wheel && input.wheel != 0)
        {
            fImageList.Scroll(input.wheel < 0 ? 1 : -1);
            handled = true;
        }
        return handled;
    }
}  // namespace OIV

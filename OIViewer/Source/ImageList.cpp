#include "ImageList.h"

#include <algorithm>

int ImageList::GetSelected() const
{
    return fSelected;
}

void ImageList::Clear()
{
    const bool itemCountChanged = !fImages.empty();
    fImages.clear();
    fSelected = -1;
    fPos      = 0;
    Changed.Raise(itemCountChanged ? ChangeType::ItemCount : ChangeType::Visual);
}

void ImageList::SetSelected(int selected)
{
    if (selected == fSelected)
        return;

    const int oldPos     = fPos;
    fSelected            = selected;
    const int maxEntries = fViewportHeight / RowHeight;
    if (fSelected >= 0 && fSelected < fPos)
    {
        fPos = fSelected;
    }
    else if (maxEntries > 0 && fSelected >= fPos + maxEntries)
    {
        fPos = std::min(static_cast<int>(GetNumberOfElements()), fSelected - maxEntries + 1);
    }

    const ImageSelectionChangeArgs args{.imageIndex = selected};
    ImageSelectionChanged.Raise(args);
    Changed.Raise(fPos != oldPos ? ChangeType::ScrollPosition : ChangeType::Visual);
}

std::optional<size_t> ImageList::GetImageIndexAt(int yPos) const
{
    if (yPos < 0 || yPos >= fViewportHeight)
        return std::nullopt;

    const size_t index = static_cast<size_t>(yPos / RowHeight + fPos);
    return index < fImages.size() ? std::optional{index} : std::nullopt;
}

void ImageList::SetPos(int pos)
{
    const int imageCount  = static_cast<int>(GetNumberOfElements());
    const int displayed   = static_cast<int>(GetNumberOfDisplayedElements());
    const int visibleRows = imageCount == 0 ? 0 : std::max(displayed, 1);
    const int maxPos      = std::max(0, imageCount - visibleRows);
    const int newPos      = std::clamp(pos, 0, maxPos);
    if (newPos != fPos)
    {
        fPos = newPos;
        Changed.Raise(ChangeType::ScrollPosition);
    }
}

void ImageList::Scroll(int steps)
{
    SetPos(fPos + steps);
}

int ImageList::GetScrollPosition() const
{
    return fPos;
}

size_t ImageList::GetNumberOfElements() const
{
    return fImages.size();
}

bool ImageList::IsSelected(size_t index) const
{
    return index == static_cast<size_t>(fSelected);
}

void ImageList::SetViewportHeight(int32_t height)
{
    fViewportHeight       = std::max(height, 0);
    const int imageCount  = static_cast<int>(GetNumberOfElements());
    const int displayed   = static_cast<int>(GetNumberOfDisplayedElements());
    const int visibleRows = imageCount == 0 ? 0 : std::max(displayed, 1);
    fPos                  = std::clamp(fPos, 0, std::max(0, imageCount - visibleRows));
}

size_t ImageList::GetNumberOfDisplayedElements() const
{
    if (fViewportHeight <= 0)
        return 0;
    return std::min(GetNumberOfElements(), static_cast<size_t>(fViewportHeight) / RowHeight);
}

const ImageList::ImageDesc& ImageList::GetImage(size_t index) const
{
    return fImages[index];
}

ImageList::VisibleRange ImageList::GetVisibleRange() const
{
    const size_t first       = static_cast<size_t>(fPos);
    const size_t visibleRows = fViewportHeight <= 0
                                   ? 0
                                   : (static_cast<size_t>(fViewportHeight) + RowHeight - 1) / RowHeight;
    return {.first = first, .last = std::min(fImages.size(), first + visibleRows)};
}

LWS::Rect ImageList::GetRowRect(size_t index, int32_t width) const
{
    const int32_t y = (static_cast<int32_t>(index) - fPos) * RowHeight;
    return {{0, y}, {width, y + RowHeight}};
}

int32_t ImageList::GetRowHeight() const
{
    return RowHeight;
}

int32_t ImageList::GetLineWidth() const
{
    return LineWidth;
}

void ImageList::SetImage(const ImageDesc& imageDesc)
{
    const bool itemCountChanged = imageDesc.index >= fImages.size();
    fImages.resize(imageDesc.index + 1);
    fImages[imageDesc.index] = imageDesc;
    if (fImages[imageDesc.index].bitmap != nullptr)
        fImages[imageDesc.index].bitmap = fImages[imageDesc.index].bitmap->resize(64, 64, {255, 255, 255, 255});
    Changed.Raise(itemCountChanged ? ChangeType::ItemCount : ChangeType::Visual);
}

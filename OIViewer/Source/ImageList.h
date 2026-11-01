#pragma once

#include <LLUtils/Event.h>
#include <LLUtils/StringDefs.h>
#include <LWS/Bitmap.hpp>
#include <LWS/WindowTypes.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

class ImageList
{
  public:

    struct ImageSelectionChangeArgs
    {
        int imageIndex = -1;
    };

    using ImageSelectionChangEvent = LLUtils::Event<void(const ImageSelectionChangeArgs&)>;

    struct ImageDesc
    {
        uint32_t index{};
        LLUtils::native_string_type title;
        LWS::BitmapSharedPtr bitmap;
    };

    struct VisibleRange
    {
        size_t first{};
        size_t last{};
    };

    enum class ChangeType
    {
        Visual,
        ItemCount,
        ScrollPosition,
    };

    using ChangedEvent = LLUtils::Event<void(ChangeType)>;

    ImageList()                            = default;
    ImageList(const ImageList&)            = delete;
    ImageList& operator=(const ImageList&) = delete;

    ImageSelectionChangEvent ImageSelectionChanged;
    ChangedEvent Changed;

    [[nodiscard]] int GetSelected() const;
    [[nodiscard]] bool IsSelected(size_t index) const;
    void Clear();
    void SetSelected(int selected);
    [[nodiscard]] std::optional<size_t> GetImageIndexAt(int yPos) const;
    void SetPos(int pos);
    void Scroll(int steps);
    [[nodiscard]] int GetScrollPosition() const;
    void SetViewportHeight(int32_t height);
    [[nodiscard]] size_t GetNumberOfDisplayedElements() const;
    [[nodiscard]] size_t GetNumberOfElements() const;
    [[nodiscard]] const ImageDesc& GetImage(size_t index) const;
    [[nodiscard]] VisibleRange GetVisibleRange() const;
    [[nodiscard]] LWS::Rect GetRowRect(size_t index, int32_t width) const;
    [[nodiscard]] int32_t GetRowHeight() const;
    [[nodiscard]] int32_t GetLineWidth() const;
    void SetImage(const ImageDesc& imageDesc);

  private:

    static constexpr int32_t RowHeight = 80;
    static constexpr int32_t LineWidth = 2;
    int32_t fViewportHeight            = 0;
    int fSelected                      = -1;
    int fPos                           = 0;
    std::vector<ImageDesc> fImages;
};

#pragma once
#include <ImageProcessing/TextRasterizer.h>
#include <RendererCore/Renderer.h>
#include <optional>
#include <RendererCore/RenderFrame.h>
#include <memory>

namespace OIV
{
    // Viewer overlay composes lazy CPU text with desired presentation properties.
    // Font ownership outlives labels, and registrations are created in label order.
    class TextLabel
    {
      public:

        explicit TextLabel(FreeType::FreeTypeConnector* font) : fRasterizer(font) {}
        void SetText(const LLUtils::native_string_type& text) { fRasterizer.SetText(text); }
        void SetDPI(uint16_t dpix, uint16_t dpiy) { fRasterizer.SetDPI(dpix, dpiy); }
        void SetFontPath(const LLUtils::native_string_type& sFontPath) { fRasterizer.SetFontPath(sFontPath); }
        void SetFontSize(uint16_t fontSize) { fRasterizer.SetFontSize(fontSize); }
        void SetOutlineWidth(uint16_t outlineWidth) { fRasterizer.SetOutlineWidth(outlineWidth); }
        void SetBackgroundColor(LLUtils::Color color) { fRasterizer.SetBackgroundColor(color); }
        void SetTextColor(LLUtils::Color color) { fRasterizer.SetTextColor(color); }
        void SetUseMetaText(bool useMetaText) { fRasterizer.SetUseMetaText(useMetaText); }
        void SetMaxWidth(int32_t maxWidth) { fRasterizer.SetMaxWidth(maxWidth); }
        void SetTextRenderMode(FreeType::RenderMode renderMode) { fRasterizer.SetTextRenderMode(renderMode); }
        void SetLineEndFixedWidth(bool lineEndFixedWidth) { fRasterizer.SetLineEndFixedWidth(lineEndFixedWidth); }
        void SetPosition(LLUtils::PointF64 value)
        {
            if (fProperties.position != value)
            {
                fProperties.position = value;
                fPropertiesDirty     = true;
            }
        }
        LLUtils::PointF64 GetPosition() const { return fProperties.position; }
        void SetScale(LLUtils::PointF64 value)
        {
            if (fProperties.scale != value)
            {
                fProperties.scale = value;
                fPropertiesDirty  = true;
            }
        }
        LLUtils::PointF64 GetScale() const { return fProperties.scale; }
        void SetOpacity(double value)
        {
            if (fProperties.opacity != value)
            {
                fProperties.opacity = value;
                fPropertiesDirty    = true;
            }
        }
        double GetOpacity() const { return fProperties.opacity; }
        void SetVisible(bool value)
        {
            if (fProperties.visible != value)
            {
                fProperties.visible = value;
                fPropertiesDirty    = true;
            }
        }
        bool GetVisible() const { return fProperties.visible; }
        void SetFilterType(OIV::FilterType value)
        {
            if (fProperties.filterType != value)
            {
                fProperties.filterType = value;
                fPropertiesDirty       = true;
            }
        }
        OIV::FilterType GetFilterType() const { return fProperties.filterType; }
        void SetImageRenderMode(OIV::ImageRenderMode value)
        {
            if (fProperties.imageRenderMode != value)
            {
                fProperties.imageRenderMode = value;
                fPropertiesDirty            = true;
            }
        }
        OIV::ImageRenderMode GetImageRenderMode() const { return fProperties.imageRenderMode; }

        TextMetrics GetMetrics() { return fRasterizer.GetMetrics(); }
        bool IsDirty() const { return fPropertiesDirty || fRasterizer.IsBitmapDirty(); }
        void EnsureRegistration(Renderer& renderer)
        {
            if (!fPresentation)
                fPresentation.emplace(renderer.CreateImage({}, fProperties));
        }
        void Prepare(Renderer& renderer)
        {
            EnsureRegistration(renderer);
            fPresentation->SetProperties(fProperties);
            if (fProperties.visible && fProperties.opacity > 0.0 && fRasterizer.IsBitmapDirty())
                fPresentation->SetImage(fRasterizer.GetBitmap());
            fPropertiesDirty = false;
        }

      private:

        // CPU producer owns text options, layout and cached bitmap.
        TextRasterizer fRasterizer;
        // Desired appearance, applied without invalidating CPU pixels.
        RenderImageProperties fProperties{.filterType      = OIV::FilterType::None,
                                          .imageRenderMode = OIV::ImageRenderMode::Overlay};
        // Optional renderer registration released while its stable facade is alive.
        std::optional<RenderImage> fPresentation;
        // Property changes request redraw even when bitmap content is unchanged.
        bool fPropertiesDirty = true;
    };
}  // namespace OIV

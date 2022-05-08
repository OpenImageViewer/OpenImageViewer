#pragma once
#include <defs.h>
#include <Image.h>

namespace OIV
{
    class IRenderable
    {
    public:
        virtual double GetOpacity() const = 0;
        virtual LLUtils::PointF64 GetScale() const = 0;
        virtual LLUtils::PointF64 GetPosition() const = 0;
        virtual IMCodec::ImageSharedPtr GetImage() = 0;
        virtual OIV_Filter_type GetFilterType() const = 0;
        virtual bool GetVisible() const = 0;
        virtual OIV_Image_Render_mode GetImageRenderMode() const = 0;
        virtual bool GetIsImageDirty() const = 0;
        virtual void ClearImageDirty() = 0;
        virtual void PreRender() = 0;

        virtual ~IRenderable() {}
    };
}
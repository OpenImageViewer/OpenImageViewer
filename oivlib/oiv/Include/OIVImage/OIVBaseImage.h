#pragma once
#include <string>
#include <vector>
#include <memory>
#include <LLUtils/StopWatch.h>
#include <LLUtils/Point.h>
#include <Image.h>
#include <defs.h>
#include <Interfaces/IRenderable.h>

namespace OIV
{
    enum class ImageSource
    {
          None
        , File
        , Clipboard
        , InternalText
        , GeneratedByLib
    };

    class OIVBaseImage : public IRenderable
    {
    public:
        OIVBaseImage(ImageSource source);
        OIVBaseImage(ImageSource source, IMCodec::ImageSharedPtr image);
        ImageSource GetImageSource() const { return fSource; }
        
        std::wstring GetDescription() const
        {
            std::wstringstream ss;
            ss << fImage->GetWidth() << L" X " << fImage->GetHeight() << L" X "
                << fImage->GetBitsPerTexel() << L" BPP | loaded in " << std::fixed << std::setprecision(1)
                << fImage->GetRuntimeData().loadTime << L" ms"
                //<< L"/" << fDescriptor.DisplayTime + fDescriptor.LoadTime << L" ms"
                ;

            return ss.str();
        }


        void SetUnderlyingImage(IMCodec::ImageSharedPtr image);

        bool IsDirty() const
        {
            return PerformIsDirty();
        }


#pragma region IRenderable

        IMCodec::ImageSharedPtr GetImage() override { return fImage; }
        bool GetVisible() const override{ return fImagePropertiesCurrent.visible;}
        OIV_Filter_type GetFilterType() const override {return fImagePropertiesCurrent.filterType;}
        double GetOpacity() const override {return fImagePropertiesCurrent.opacity;}
        LLUtils::PointF64 GetPosition() const override {return fImagePropertiesCurrent.position;}
        LLUtils::PointF64 GetScale() const override {return fImagePropertiesCurrent.scale;}
        OIV_Image_Render_mode GetImageRenderMode() const override {return fImagePropertiesCurrent.imageRenderMode;}
        bool GetIsImageDirty() const override{return fIsImageDirty;}
        void ClearImageDirty() override {fIsImageDirty = false;}
        
        void PreRender() override {PerformPreRender();}


#pragma endregion IRenderable


#pragma region Text display

        //Text Display

        void SetPosition(LLUtils::PointF64 position)
        {
            if (fImagePropertiesCurrent.position != position)
            {
                fIsDirty = true;
                fImagePropertiesCurrent.position = position;
            }
        }

        void SetFilterType(OIV_Filter_type filterType)
        {
            if (fImagePropertiesCurrent.filterType != filterType)
            {
                fIsDirty = true;
                fImagePropertiesCurrent.filterType = filterType;
            }
        }
        void SetImageRenderMode(OIV_Image_Render_mode renderMode)
        {
            if (fImagePropertiesCurrent.imageRenderMode != renderMode)
            {
                fIsDirty = true;
                fImagePropertiesCurrent.imageRenderMode = renderMode;
            }
        }

        void SetVisible(bool visible)
        {
            if (fImagePropertiesCurrent.visible != visible)
            {
                fIsDirty = true;
                fImagePropertiesCurrent.visible = visible;
            }
        }

        

        void SetOpacity(double opacity)
        {
            if (fImagePropertiesCurrent.opacity != opacity)
            {
                fIsDirty = true;
                fImagePropertiesCurrent.opacity = opacity;
            }
        }

        void SetScale(LLUtils::PointF64 scale)
        {
            if (fImagePropertiesCurrent.scale != scale)
            {
                fIsDirty = true;
                fImagePropertiesCurrent.scale = scale;
            }
        }
#pragma endregion Text display

        //Keep OIVBaseImage polymorphic
        virtual ~OIVBaseImage();

    protected:

        virtual bool PerformIsDirty() const
        {
            return fIsDirty;
        }
        virtual void PerformPreRender() 
        {
            fIsDirty = false;
        };
        
    private:
        OIV_CMD_ImageProperties_Request fImagePropertiesCurrent{};
        OIV_CMD_ImageProperties_Request fImagePropertiesCached{};

        ImageSource fSource;
        IMCodec::ImageSharedPtr fImage;
        bool fIsImageDirty = true;
        bool fIsDirty = true;
    };

    using OIVBaseImageSharedPtr = std::shared_ptr<OIVBaseImage>;
}
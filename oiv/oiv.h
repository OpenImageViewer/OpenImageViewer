#pragma once
#include "ZoomScrollState.h"
#include "Interfaces\IPictureRenderer.h"
#include <ImageLoader.h>
#include "interfaces/IRenderer.h"
#include "ViewParameters.h"
#include "ImageManager.h"


namespace OIV
{
    class OIV  :
          public ZoomScrollState::Listener
        , public IPictureRenderer
    {
#pragma region  //-------------Scroll state listener------------------
        LLUtils::PointI32 GetClientSize() override;
        LLUtils::PointI32 GetImageSize() override;
        void NotifyDirty() override;
#pragma endregion 

    public:

#pragma region //-------------IPictureListener implementation------------------
        double Zoom(double percentage,int x,int y) override;
        int Pan(double x, double y) override;
        ResultCode UnloadFile(const ImageHandle handle) override;
        int LoadFile(void* buffer, std::size_t size, char* extension , bool onlyRegisteredExtension, ImageHandle& handle) override;
        ResultCode DisplayFile(const ImageHandle handle, const OIV_CMD_DisplayImage_Flags display_flags) override;
        int Init() override;
        int SetParent(std::size_t handle) override;
        int Refresh() override;
        
        IMCodec::Image* GetImage(ImageHandle handle) override;
        int SetFilterLevel(OIV_Filter_type filter_level) override;
        int GetFileInformation(QryFileInformation& information) override;
        int GetTexelAtMousePos(int mouseX, int mouseY, double& texelX, double& texelY) override;
        int SetTexelGrid(double gridSize) override;
        int GetNumTexelsInCanvas(double &x, double &y) override;
        int SetClientSize(uint16_t width, uint16_t height) override;
        ResultCode AxisAlignTrasnform(const OIV_AxisAlignedRTransform transform) override;
#pragma endregion

#pragma region //-------------Private methods------------------
        IRendererSharedPtr CreateBestRenderer();
        //int DisplayImage(ImageHandle handle);
        void HandleWindowResize();
        bool IsImageDisplayed() const;
        void UpdateGpuParams();
        OIV_AxisAlignedRTransform ResolveExifRotation(unsigned short exifRotation) const;
        IMCodec::ImageSharedPtr ApplyExifRotation(IMCodec::ImageSharedPtr image) const;
        void LoadSettings();
        IMCodec::ImageSharedPtr GetDisplayImage() const;
        IMCodec::ImageSharedPtr GetActiveImage() const;
#pragma endregion

#pragma region //-------------Private member fields------------------

    private:

        std::string fCurrentOpenedFile;
        IMCodec::ImageLoader fImageLoader;
        ImageManager fImageManager;
        IRendererSharedPtr fRenderer = nullptr;
        ZoomScrollState fScrollState = this;
        ViewParameters fViewParams = {};
        std::size_t fParent = 0;
        bool fIsRefresing = false;
        bool fShowGrid = false;
        ImageHandle fActiveHandle = ImageNullHandle;
        IMCodec::ImageSharedPtr fDisplayedImage = nullptr;
        OIV_Filter_type fFilterLevel = OIV_Filter_type::FT_Linear;

        LLUtils::PointI32 fClientSize = LLUtils::PointI32::Zero;
        
#pragma endregion
    };
}

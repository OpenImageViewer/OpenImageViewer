#pragma once
#include "ConsoleLogListener.h"
#include "ZoomScrollState.h"
#include "Interfaces\IPictureRenderer.h"
#include "Quad.h"
#include "ImageLoader.h"


namespace OIV
{
    class OIV  :
          public ZoomScrollState::Listener
        , public IPictureRenderer
    {
    private:


        Ogre::SceneManager*		fScene;
        Ogre::Camera*				fActiveCamera;
        Ogre::String				fActiveCameraName;
        Ogre::Viewport*			fViewPort;
        ZoomScrollState     fScrollState;
        Quad * rect;
        Ogre::GpuProgramParametersSharedPtr  fFragmentParameters;
        Ogre::Pass*         fPass;
        HWND                fParent;
        bool                fIsRefresing;
        std::string fCurrentOpenedFile;
        ImageLoader fImageLoader;
        bool fShowGrid;
        typedef std::unique_ptr<Image> ImageUniquePtr;
        ImageUniquePtr fOpenedImage;
        int fFilterLevel;
        int fClientWidth;
        int fClientHeight;

        void SetupRenderer();
        void InitAll();


        void UpdateGpuParams();

        //-------------Scroll state listener------------------

        virtual Ogre::Vector2 GetClientSize() override;
        virtual Ogre::Vector2 GetImageSize() override;
        virtual void NotifyDirty() override;
        //----------------------------------------------------

     
        void HandleWindowResize();


        bool IsImageLoaded() const
        {
            return fOpenedImage.get() != nullptr;
        }


    public:
        Ogre::Root *root;
        ~OIV();
        OIV();
        void TryLoadPlugin(std::string pluginName);

        void CreateCameraAndViewport();
        void ApplyFilter()
        {
            using namespace Ogre;
            switch (fFilterLevel)
            {
            case 0:
                fPass->getTextureUnitState(0)->setTextureFiltering(TFO_NONE);
                break;
            case 1:
                fPass->getTextureUnitState(0)->setTextureFiltering(TFO_BILINEAR);
                break;
            }
            

        
        }
        void CreateScene();
        void LoadSettings();


#pragma region IPictureListener
        //-------------IPictureListener------------------
        // 
        virtual double Zoom(double percentage,int x,int y) override;
        virtual int Pan(double x, double y) override;
        virtual int LoadFile(OIVCHAR* filePath,bool onlyRegisteredExtension) override;
        virtual int Init() override;
        virtual int SetParent(HWND handle) override;
        virtual int Refresh() override;
        virtual Image* GetImage() override;
        virtual int SetFilterLevel(int filter_level) override;
        virtual int GetFileInformation(QryFileInformation& information) override;
        virtual int GetTexelAtMousePos(int mouseX, int mouseY, double& texelX, double& texelY) override;
        virtual int SetTexelGrid(double gridSize) override;
        virtual int GetNumTexelsInCanvas(double &x, double &y) override;
        virtual int SetClientSize(uint16_t width, uint16_t height) override;
#pragma endregion


        //----------------------------------------------------
    };
}

#pragma once
#include "ConsoleLogListener.h"
#include "ZoomScrollState.h"
#include "Interfaces\IPictureRenderer.h"
#include "Quad.h"
#include "ImageLoader.h"


namespace OIV
{
    class OIV : public Ogre::WindowEventListener
        , public ZoomScrollState::Listener
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
        int fKeyboardPanSpeed;
        bool fShowGrid;
        typedef std::unique_ptr<Image> ImageUniquePtr;
        ImageUniquePtr fOpenedImage;
        int fFilterLevel;

        void SetupRenderer();
        void InitAll();

        Ogre::RenderWindow * GetWindow();

        void UpdateGpuParams();

        //-------------Scroll state listener------------------

        virtual Ogre::Vector2 GetMousePosition() override;
        virtual Ogre::Vector2 GetWindowSize() override;
        virtual Ogre::Vector2 GetImageSize() override;

        virtual void NotifyDirty() override;
        //----------------------------------------------------

        // 'Ogre::WindowEventListener' members decleration
        bool windowClosing(Ogre::RenderWindow* rw) override
        {
            ShutDown();
            return Ogre::WindowEventListener::windowClosing(rw);
        }

        void ShutDown()
        {
            Ogre::Root::getSingleton().queueEndRendering();
        }

        void windowClosed(Ogre::RenderWindow *  rw)  override;

        void windowResized(Ogre::RenderWindow* rw)  override;

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
        virtual double Zoom(double percentage) override;
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
        virtual int GetCanvasSize(double &x, double &y) override;
#pragma endregion


        //----------------------------------------------------
    };
}

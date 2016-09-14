#pragma once
#include "ConsoleLogListener.h"
#include "ZoomScrollState.h"
#include "Interfaces\IPictureRenderer.h"
#include "ImageFreeImage.h"
#include "Quad.h"


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
        Image* fImage;
        Image* fImage32Bit;
        
        
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
            return fImage->IsOpened();
        }


    public:
        Ogre::Root *root;
        ~OIV();
        OIV();
        void TryLoadPlugin(std::string pluginName);

        void CreateCameraAndViewport();
        void CreateScene();
        void LoadSettings();

        //-------------IPictureListener------------------
        // 
        //Commands
        virtual double Zoom(double percentage) override;
        virtual int Pan(double x, double y) override;
        virtual int LoadFile(OIVCHAR* filePath) override;
        virtual int Init() override;
        virtual int SetParent(HWND handle) override;
        virtual int Refresh() override;
        

        //Queries
        virtual int GetFileInformation(QryFileInformation& information) override;



        //----------------------------------------------------
    };
}
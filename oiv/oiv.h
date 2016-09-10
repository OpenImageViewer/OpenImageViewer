#pragma once

#include "PreCompiled.h"
#include <iostream>
#include "OgreFrameListener.h"
#include "OgreString.h"
#include "OgreResourceGroupManager.h"
#include "ConsoleLogListener.h"
#include "ZoomScrollState.h"
#include "Interfaces\IPictureRenderer.h"
#include <string>

namespace OIV
{
    class OIV : public WindowEventListener
        , public ZoomScrollState::Listener
        , public IPictureRenderer
    {
    private:


        SceneManager*		fScene;
        Camera*				fActiveCamera;
        String				fActiveCameraName;
        Viewport*			fViewPort;
        ZoomScrollState     fScrollState;
        Ogre::Rectangle2D* rect;
        GpuProgramParametersSharedPtr  fFragmentParameters;
        Ogre::Pass*         fPass;
        HWND                fParent;
        Ogre::Image         fImageOpened;
        String              fTextureName;
        Ogre::TexturePtr    fActiveTexture;
        
        
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
        bool windowClosing(RenderWindow* rw) override
        {
            ShutDown();
            return WindowEventListener::windowClosing(rw);
        }

        void ShutDown()
        {
            Root::getSingleton().queueEndRendering();
        }

        void windowClosed(RenderWindow *  rw)  override;

        void windowResized(RenderWindow* rw)  override;

        void HandleWindowResize();


        bool IsImageLoaded() const
        {
            return fActiveTexture.isNull() == false;
        }


    public:
        Root *root;
        void SetTextureName(const char* textureName)
        {
            fTextureName = textureName;
        }
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
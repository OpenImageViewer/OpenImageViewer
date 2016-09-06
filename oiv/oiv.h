#pragma once

#include "PreCompiled.h"
#include <iostream>
#include "OgreFrameListener.h"
#include "OgreString.h"
#include "OgreResourceGroupManager.h"
#include "ConsoleLogListener.h"
#include "ZoomScrollState.h"
#include <string>

namespace OIV
{
    class OIV : public WindowEventListener
        , public FrameListener
        , public OIS::KeyListener
        , public OIS::MouseListener
        , public ZoomScrollState::Listener
    {
    private:


        SceneManager*		fScene;
        OIS::InputManager*	fInputManager;
        OIS::Keyboard*		fKeyboard;
        OIS::Mouse*			fMouse;
        Camera*				fActiveCamera;
        String				fActiveCameraName;
        Viewport*			fViewPort;
        ZoomScrollState     fScrollState;
        map<String, String>::type mSettings;
        Ogre::Rectangle2D* rect;
        GpuProgramParametersSharedPtr  fFragmentParameters;
        

        void InitializeInput();
        void SetupRenderer();
        const String& GetSetting(const String& key, bool throwifNotFound = false);
        void InitAll();
        void Update(const FrameEvent &  evt);
        

        Ogre::RenderWindow * GetWindow();

        void UpdateGpuParams();



        String fTextureName;
        Ogre::TexturePtr fActiveTexture;


        //-------------Scroll state listener------------------

        virtual Ogre::Vector2 GetMousePosition() override;
        virtual Ogre::Vector2 GetWindowSize() override;
        virtual Ogre::Vector2 GetImageSize() override;

        virtual void NotifyDirty() override;
        //----------------------------------------------------



        float Y1;
        float Y2;

        // 'Ogre::FrameListener' members decleration

        bool frameRenderingQueued(const FrameEvent& evt) override;
        bool frameStarted(const FrameEvent& evt) override;
        bool frameEnded(const FrameEvent &  evt)  override;

        // 'Ogre::WindowEventListener' members decleration
        bool windowClosing(RenderWindow* rw) override
        {
            ShutDown();
            return WindowEventListener::windowClosing(rw);
        }

        void ShutDown()
        {
            //fSubpart.setNull();
            Root::getSingleton().queueEndRendering();
        }

        void windowClosed(RenderWindow *  rw)  override;

        void windowResized(RenderWindow* rw)  override;

        // 'OIS::KeyListner' members decleration

        virtual bool keyPressed(const OIS::KeyEvent &arg) override;
        virtual bool keyReleased(const OIS::KeyEvent &arg) override;

        

        // 'OIS::MouseListener' members declaaration
        virtual bool mouseMoved(const OIS::MouseEvent &arg) override;
        virtual bool mousePressed(const OIS::MouseEvent &arg, OIS::MouseButtonID id) override;
        virtual bool mouseReleased(const OIS::MouseEvent &arg, OIS::MouseButtonID id) override;

    public:
        Root *root;
        void Start();
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
    };
}
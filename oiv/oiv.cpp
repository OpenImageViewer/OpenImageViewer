#pragma warning(disable : 4275 ) // disables warning 4275, pop and push from exceptions
#pragma warning(disable : 4251 ) // disables warning 4251, the annoying warning which isn't needed here...
#include "precompiled.h"
#include "oiv.h"
#include "OgrePrerequisites.h"
#include <set>
#include "StringUtil.h"
#include "Utility.h"
#include "OgreRectangle2D.h"


namespace OIV
{
    OIV::~OIV()
    {
        if (fInputManager != NULL)
        {
            fInputManager->destroyInputObject(fKeyboard);
            fInputManager->destroyInputObject(fMouse);
            OIS::InputManager::destroyInputSystem(fInputManager);
        }
    }

    OIV::OIV() :
        fScrollState(this)
        , fInputManager(NULL)
    {

        LoadSettings();
    }

    void OIV::InitAll()
    {
        this->SetupRenderer();
        this->CreateScene();
        this->InitializeInput();
    }

    void OIV::Start()
    {
        try
        {
            this->InitAll();
            root->startRendering();
        }

        catch (Ogre::Exception e)
        {
            MessageBoxA(0, e.getFullDescription().c_str(), "System Message", MB_OK);
        }

        catch (...)
        {
            MessageBoxA(0, "Unknown error has occured", "System Message", MB_OK);
        }
    }


    void OIV::InitializeInput()
    {
        OIS::ParamList pl;
        size_t windowHnd = 0;
        std::ostringstream windowHndStr;
        Root::getSingleton().getRenderSystem()->getRenderTarget("MainWindow")->getCustomAttribute("WINDOW", &windowHnd);

        windowHndStr << windowHnd;

        pl.insert(std::make_pair(std::string("WINDOW"), windowHndStr.str()));
        bool nograb = true;
        if (nograb)
        {
            pl.insert(std::make_pair("x11_keyboard_grab", "false"));
            pl.insert(std::make_pair("x11_mouse_grab", "false"));
            pl.insert(std::make_pair("w32_mouse", "DISCL_FOREGROUND"));
            pl.insert(std::make_pair("w32_mouse", "DISCL_NONEXCLUSIVE"));
            pl.insert(std::make_pair("w32_keyboard", "DISCL_FOREGROUND"));
            pl.insert(std::make_pair("w32_keyboard", "DISCL_NONEXCLUSIVE"));
        }

        fInputManager = OIS::InputManager::createInputSystem(pl);
        fMouse = static_cast<OIS::Mouse*>(fInputManager->createInputObject(OIS::OISMouse, true));
        fKeyboard = static_cast<OIS::Keyboard*>(fInputManager->createInputObject(OIS::OISKeyboard, true));
        fKeyboard->setEventCallback(this);
        fMouse->setEventCallback(this);
    }



    void OIV::TryLoadPlugin(std::string pluginName)
    {
        try
        {
            root->loadPlugin(pluginName.c_str());
        }
        catch (...)
        {

        }
    }

    void OIV::SetupRenderer()
    {
        using namespace std;

#ifdef _DEBUG
        root = new Root();
#else
        // No log file
        root = new Root(BLANKSTRING, BLANKSTRING, BLANKSTRING);
#endif


#ifdef _DEBUG
        TryLoadPlugin("RenderSystem_Direct3D9_d");
        TryLoadPlugin("RenderSystem_Direct3D11_d");
        TryLoadPlugin("RenderSystem_GLES2_d.dll");
        TryLoadPlugin("RenderSystem_GL_d.dll");
#else
        TryLoadPlugin("RenderSystem_Direct3D9");
        TryLoadPlugin("RenderSystem_Direct3D11");
        TryLoadPlugin("RenderSystem_GLES2.dll");
        TryLoadPlugin("RenderSystem_GL.dll");
#endif


        const RenderSystemList &rlist = root->getAvailableRenderers();
        RenderSystemList::const_iterator it = rlist.begin();
        const String& renderer = GetSetting("RenderSystem", true);
        while (it != rlist.end())
        {
            RenderSystem *rSys = *(it++);
            String name = rSys->getName();

            if (rSys->getName().find(renderer) != String::npos)
            {
                root->setRenderSystem(rSys);
                break;
            }
        }

        root->initialise(false);


        ResourceGroupManager& rgm = ResourceGroupManager::getSingleton();
        std::string exeFolder = StringUtility::ToAString(Utility::GetExeFolder());
        rgm.addResourceLocation(exeFolder + String("\\Resources\\programs"), "FileSystem");
        rgm.addResourceLocation(exeFolder + String("\\Resources\\scripts"), "FileSystem");


        //Log for console output    
        LogManager::getSingleton().getDefaultLog()->addListener(new GameLogListener());

        NameValuePairList options;
        std::string vsync = GetSetting("vsync");
        if (vsync != BLANKSTRING)
            options["vsync"] = vsync;
        int width = 1440;
        int height = 900;

        string widthStr = GetSetting("width");
        if (widthStr != BLANKSTRING)
            width = StringConverter::parseInt(widthStr);

        string heightStr = GetSetting("height");
        if (heightStr != BLANKSTRING)
            height = StringConverter::parseInt(heightStr);


        //options["alwaysWindowedMode"] = "true";

        RenderWindow *window = root->createRenderWindow(
            "MainWindow",			// window name
            width,                   // window width, in pixels
            height,                   // window height, in pixels
            false,                 // fullscreen or not
            &options);                    // use defaults for all other values

        WindowEventUtilities::addWindowEventListener(window, this);

        root->addFrameListener(this);


        fScene = root->createSceneManager(ST_GENERIC, "MySceneManager");

        ResourceGroupManager::getSingleton().initialiseAllResourceGroups();
    }


    const String& OIV::GetSetting(const String& key, bool throwifNotFound)
    {
        map<String, String>::const_iterator it = mSettings.find(key);
        if (it != mSettings.end())
            return it->second;
        else
        {
            if (throwifNotFound == true)
            {
                throw std::exception("setting key must exists");

            }
            return BLANKSTRING;
        }

    }



    void  OIV::Update(const FrameEvent &  evt)
    {

    }


    Ogre::Vector2 OIV::GetMousePosition()
    {
        RenderWindow* rw = dynamic_cast<RenderWindow*>(Root::getSingleton().getRenderTarget("MainWindow"));
        Ogre::StringStream ss;
        POINT p;
        GetCursorPos(&p);
        HWND hwnd;
        rw->getCustomAttribute("WINDOW", &hwnd);
        ScreenToClient(hwnd, &p);
        return Vector2(p.x, p.y);
    }




    Ogre::Vector2 OIV::GetImageSize()
    {
        return Vector2(fActiveTexture->getWidth(), fActiveTexture->getHeight());
    }

    Ogre::RenderWindow* OIV::GetWindow()
    {
        return dynamic_cast<RenderWindow*>(Root::getSingleton().getRenderTarget("MainWindow"));
    }

    Ogre::Vector2 OIV::GetWindowSize()
    {
        RenderWindow* wnd = GetWindow();
        return Vector2(wnd->getWidth(), wnd->getHeight());
    }

    void OIV::NotifyDirty()
    {
        UpdateGpuParams();
    }

    void OIV::UpdateGpuParams()
    {
        Vector2 uvScaleFixed = fScrollState.GetARFixedUVScale();
        Vector2 uvOffset = fScrollState.GetOffset();

        fFragmentParameters->setNamedConstant("uvScale", uvScaleFixed);
        fFragmentParameters->setNamedConstant("uvOffset", uvOffset);
    }


    // 'Ogre::FrameListener' implementation
    bool OIV::frameRenderingQueued(const FrameEvent& evt)
    {
        this->Update(evt);
        /*if (!fSubpart.isNull())
        fSubpart->RenderSubParts();*/
        return true;
    }
    bool OIV::frameStarted(const FrameEvent& evt)
    {
        fKeyboard->capture();
        fMouse->capture();
        //fSubpart->RenderSubParts();
        return true;
    }
    bool OIV::frameEnded(const FrameEvent &  evt)
    {
        return true;
    }

    // 'Ogre::WindowEventListener' implementation 
    void OIV::windowClosed(RenderWindow *  rw)
    {
        Root::getSingleton().queueEndRendering();
    }



    void OIV::windowResized(RenderWindow* rw)
    {
        fScrollState.RefreshOffset();
    }

    //'OIS::KeyListner' Implemetation 
    bool OIV::keyPressed(const OIS::KeyEvent &arg)
    {
        bool isAltDOwn = fKeyboard->isModifierDown(OIS::Keyboard::Alt);
        switch (arg.key)
        {
        case OIS::KC_SPACE:
            break;
        case OIS::KC_UP:
            break;
        case OIS::KC_DOWN:
            break;

        case OIS::KC_ESCAPE:
            ShutDown();
            break;

        case OIS::KC_X:
            if (isAltDOwn)
                ShutDown();
            break;
        case OIS::KC_F2:


            break;

        case OIS::KC_F1:

            break;
        case OIS::KC_C:

            break;

        }
        return true;
    }

    bool OIV::keyReleased(const OIS::KeyEvent &arg)
    {
        return true;
    }


    bool OIV::mouseMoved(const OIS::MouseEvent & arg)
    {
        fScrollState.Zoom(-arg.state.Z.rel / 600.0);

        if (arg.state.buttonDown(OIS::MouseButtonID::MB_Left))
            
            fScrollState.Pan(-Vector2(arg.state.X.rel, arg.state.Y.rel) * 0.002);
        return false;
    }

    bool OIV::mousePressed(const OIS::MouseEvent & arg, OIS::MouseButtonID id)
    {
        return false;
    }

    bool OIV::mouseReleased(const OIS::MouseEvent & arg, OIS::MouseButtonID id)
    {
        return false;
    }

    void OIV::CreateCameraAndViewport()
    {

        /*Camera*  freeCamera = fScene->createCamera("FreeCamera");
        freeCamera->setNearClipDistance(1);
        freeCamera->setFarClipDistance(50000);
        freeCamera->setAutoAspectRatio(true);
        SceneNode *freeCamNode = fScene->getRootSceneNode()->createChildSceneNode("FreeCameraNode");
        freeCamNode->setPosition(0,0,1400);
        freeCamNode->attachObject(freeCamera);*/

        RenderTarget* window = Root::getSingleton().getRenderSystem()->getRenderTarget("MainWindow");

        this->fActiveCameraName = "MainCamera";
        Camera* camera = fScene->createCamera(this->fActiveCameraName);
        camera->setNearClipDistance(1);
        camera->setFarClipDistance(500000.0);
        camera->setAutoAspectRatio(true);
        fActiveCamera = camera;
        fViewPort = window->addViewport(camera);
        //fViewPort->setDepthClear(0);
        fViewPort->setClearEveryFrame(true);
        fViewPort->setOverlaysEnabled(true);

        float grayLevel = 0.0f;
        fViewPort->setBackgroundColour(ColourValue::ColourValue(grayLevel, grayLevel, grayLevel));
    }

    bool OgreLoadImage(const Ogre::String& texture_name, const Ogre::String& texture_path)
    {
        bool image_loaded = false;
        std::ifstream ifs(texture_path.c_str(), std::ios::binary | std::ios::in);
        if (ifs.is_open())
        {
            Ogre::String tex_ext;
            Ogre::String::size_type index_of_extension = texture_path.find_last_of('.');
            if (index_of_extension != Ogre::String::npos)
            {
                tex_ext = texture_path.substr(index_of_extension + 1);
                Ogre::DataStreamPtr data_stream(new Ogre::FileStreamDataStream(texture_path, &ifs, false));
                Ogre::Image img;
                img.load(data_stream, tex_ext);
                Ogre::TextureManager::getSingleton().loadImage(texture_name,
                    Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, img, Ogre::TEX_TYPE_2D, 0, 1.0f, true, PF_R8G8B8A8, false);
                image_loaded = true;
            }
            ifs.close();
        }
        return image_loaded;
    }


    void OIV::CreateScene()
    {
        using namespace Ogre;

        rect = new Ogre::Rectangle2D(true);

        rect->setCorners(-1, 1, 1, -1);
        AxisAlignedBox bb;
        bb.setInfinite();
        rect->setBoundingBox(bb);
        //rect->setUVs(Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(1, 0));

        MaterialPtr material = MaterialManager::getSingleton().getByName("OgreViewer/QuadMaterial");
        rect->setMaterial("OgreViewer/QuadMaterial");
        Pass* pass = material->getTechnique(0)->getPass(0);
        fFragmentParameters = pass->getFragmentProgramParameters();
        pass->getTextureUnitState(0)->setTextureAddressingMode(TextureUnitState::TAM_BORDER);
        pass->getTextureUnitState(0)->setTextureFiltering(TFO_NONE);

        String textureName = fTextureName;
        if (OgreLoadImage(textureName, textureName) == false)
        {
            exit(1);

        }

        fActiveTexture = Ogre::TextureManager::getSingleton().getByName(textureName);



        /*img. load(textureName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);

        TexturePtr texture = TextureManager::getSingleton().loadImage(textureName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, img);*/

        pass->getTextureUnitState(0)->setTextureName(textureName);
        fScene->getRootSceneNode()->createChildSceneNode()->attachObject(rect);

        fScene->setAmbientLight(ColourValue(0.8f, 0.8f, 0.8f));
        fScene->setShadowTechnique(Ogre::ShadowTechnique::SHADOWTYPE_NONE);

        CreateCameraAndViewport();
        // Refresh scroll state offset and scale after image load.
        
        fScrollState.RefreshScale();
        fScrollState.RefreshOffset();
    }

    void OIV::LoadSettings()
    {
        //tstring filePath = Utility::GetAppDataFolder() + "\\settings.ini";
        std::string filePath = StringUtility::ToAString(Utility::GetExeFolder()) + "\\settings.ini";

        std::ifstream t(filePath);
        std::string str((std::istreambuf_iterator<char>(t)),
            std::istreambuf_iterator<char>());

        StringVector entries = StringUtil::split(str, "\n");
        StringVector::const_iterator it_end = entries.end();



        for (StringVector::const_iterator it = entries.begin(); it != it_end; it++)
        {
            const String& pair = *it;
            if (pair.length() > 0 && pair[0] == ';')
                continue;
            StringVector pairSplitted = StringUtil::split(pair, "=");
            if (pairSplitted.size() != 2)
                continue;
            String key = pairSplitted[0];
            StringUtil::trim(key);
            String value = pairSplitted[1];
            StringUtil::trim(value);

            mSettings[key] = value;
        }
    }
}
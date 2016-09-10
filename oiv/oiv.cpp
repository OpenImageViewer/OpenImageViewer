#pragma warning(disable : 4275 ) // disables warning 4275, pop and push from exceptions
#pragma warning(disable : 4251 ) // disables warning 4251, the annoying warning which isn't needed here...
#include "precompiled.h"
#include "oiv.h"
#include "OgrePrerequisites.h"
#include <set>
#include "StringUtility.h"
#include "Utility.h"
#include "OgreRectangle2D.h"
#include "API\functions.h"
#include "OgreHelper.h"

namespace OIV
{


    OIV::OIV() :
        fScrollState(this)
        , fPass(NULL)
        , fParent(NULL)
    {

        LoadSettings();
    }


    OIV::~OIV()
    {

    }
    void OIV::InitAll()
    {
        this->SetupRenderer();
        this->CreateScene();
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

        if (fParent != NULL)
            //externalWindowHandle
            //parentWindowHandle
            options["externalWindowHandle"] = Ogre::StringConverter::toString((size_t)fParent);


        //options["alwaysWindowedMode"] = "true";

        RenderWindow *window = root->createRenderWindow(
            "MainWindow",			// window name
            width,                   // window width, in pixels
            height,                   // window height, in pixels
            false,                 // fullscreen or not
            &options);                    // use defaults for all other values

        WindowEventUtilities::addWindowEventListener(window, this);


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
        Refresh();
    }

    void OIV::UpdateGpuParams()
    {
        Vector2 uvScaleFixed = fScrollState.GetARFixedUVScale();
        Vector2 uvOffset = fScrollState.GetOffset();

        fFragmentParameters->setNamedConstant("uvScale", uvScaleFixed);
        fFragmentParameters->setNamedConstant("uvOffset", uvOffset);
    }



    // 'Ogre::WindowEventListener' implementation 
    void OIV::windowClosed(RenderWindow *  rw)
    {
        PostQuitMessage(0);
    }

   
    void OIV::windowResized(RenderWindow* rw)
    {
        HandleWindowResize();
    }

    void OIV::HandleWindowResize()
    {
        if (IsImageLoaded())
            fScrollState.RefreshOffset();
    }

    void OIV::CreateCameraAndViewport()
    {

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

    


    void OIV::CreateScene()
    {
        using namespace Ogre;

        rect = new Ogre::Rectangle2D(true);

        rect->setCorners(-1, 1, 1, -1);
        AxisAlignedBox bb;
        bb.setInfinite();
        rect->setBoundingBox(bb);

        MaterialPtr material = MaterialManager::getSingleton().getByName("OgreViewer/QuadMaterial");
        rect->setMaterial("OgreViewer/QuadMaterial");
        fPass = material->getTechnique(0)->getPass(0);
        fFragmentParameters = fPass->getFragmentProgramParameters();
        fPass->getTextureUnitState(0)->setTextureAddressingMode(TextureUnitState::TAM_BORDER);
        //fPass->getTextureUnitState(0)->setTextureFiltering(TFO_NONE);
        fPass->getTextureUnitState(0)->setTextureFiltering(TFO_TRILINEAR);


        fScene->getRootSceneNode()->createChildSceneNode()->attachObject(rect);

        fScene->setAmbientLight(ColourValue(0.8f, 0.8f, 0.8f));
        fScene->setShadowTechnique(Ogre::ShadowTechnique::SHADOWTYPE_NONE);

        CreateCameraAndViewport();
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

////'OIS::KeyListner' implementation. 
//bool OIV::keyPressed(const OIS::KeyEvent &arg)
//{
//    bool isAltDOwn = fKeyboard->isModifierDown(OIS::Keyboard::Alt);
//    switch (arg.key)
//    {
//        
//    case OIS::KC_ADD:

//        fScrollState.Zoom(0.1);
//        break;
//    case OIS::KC_SUBTRACT:
//        fScrollState.Zoom(-0.1);
//        break;
//    case OIS::KC_SPACE:
//    {
//        {
//            CmdDataLoadFile loadFile;
//            //loadFile.filePath = L"d:/1.png";
//            loadFile.filePath = L"d:/PNG_transparency_demonstration_1.png";
//            loadFile.FileNamelength = _tcslen(loadFile.filePath);
//            OIV_Execute(CommandExecute::CE_LoadFile, sizeof(loadFile), &loadFile);
//        };
//            
//            //LoadFile(L"d:/PNG_transparency_demonstration_1.png");
//    }
//        break;
//    case OIS::KC_UP:
//        break;
//    case OIS::KC_DOWN:
//        break;

//    case OIS::KC_ESCAPE:
//        ShutDown();
//        break;

//    case OIS::KC_X:
//        if (isAltDOwn)
//            ShutDown();
//        break;
//    case OIS::KC_F2:


//        break;

//    case OIS::KC_F1:

//        break;
//    case OIS::KC_C:

//        break;

//    }
//    return true;
//}

//bool OIV::keyReleased(const OIS::KeyEvent &arg)
//{
//    return true;
//}


//bool OIV::mouseMoved(const OIS::MouseEvent & arg)
//{
//    fScrollState.Zoom(-arg.state.Z.rel / 600.0);

//    if (arg.state.buttonDown(OIS::MouseButtonID::MB_Left))
//        
//        fScrollState.Pan(-Vector2(arg.state.X.rel, arg.state.Y.rel) * 0.002);
//    return false;
//}

//bool OIV::mousePressed(const OIS::MouseEvent & arg, OIS::MouseButtonID id)
//{
//    return false;
//}

//bool OIV::mouseReleased(const OIS::MouseEvent & arg, OIS::MouseButtonID id)
//{
//    return false;
//}

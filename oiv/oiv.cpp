#pragma warning(disable : 4275 ) // disables warning 4275, pop and push from exceptions
#pragma warning(disable : 4251 ) // disables warning 4251, the annoying warning which isn't needed here...
#include "precompiled.h"
#include "oiv.h"
#include "OgreRectangle2D.h"
namespace OIV
{


    OIV::OIV() :
        fScrollState(this)
        , fIsRefresing(false)
        , fPass(NULL)
        , fParent(NULL)
    {
         
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
        //TryLoadPlugin("RenderSystem_Direct3D9_d");
        TryLoadPlugin("RenderSystem_Direct3D11_d");
        //TryLoadPlugin("RenderSystem_GLES2_d.dll");
        //TryLoadPlugin("RenderSystem_GL_d.dll");
#else
        //TryLoadPlugin("RenderSystem_Direct3D9");
        TryLoadPlugin("RenderSystem_Direct3D11");
        //TryLoadPlugin("RenderSystem_GLES2.dll");
        //TryLoadPlugin("RenderSystem_GL.dll");
#endif


        const RenderSystemList &rlist = root->getAvailableRenderers();
        RenderSystemList::const_iterator it = rlist.begin();
        while (it != rlist.end())
        {
            RenderSystem *rSys = *(it++);
            String name = rSys->getName();

            if (rSys->getName().find("Direct3D11") != String::npos)
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

        options["vsync"] = "true";
        if (fParent != NULL)
            options["externalWindowHandle"] = Ogre::StringConverter::toString((size_t)fParent);


        RenderWindow *window = root->createRenderWindow(
            "MainWindow",			// window name
            800,                   // window width, in pixels
            600,                   // window height, in pixels
            false,                 // fullscreen or not
            &options);                    // use defaults for all other values

        WindowEventUtilities::addWindowEventListener(window, this);


        fScene = root->createSceneManager(ST_GENERIC, "MySceneManager");

        ResourceGroupManager::getSingleton().initialiseAllResourceGroups();
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
            fScrollState.Refresh();
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

#pragma warning(disable : 4275 ) // disables warning 4275, pop and push from exceptions
#pragma warning(disable : 4251 ) // disables warning 4251, the annoying warning which isn't needed here...
#include "precompiled.h"
#include "oiv.h"
#include "quad.h"
#include "PluginJpeg.h"
#include "PluginPng.h"
#include "PluginFreeImage.h"
#ifdef _MSC_VER
#include <RenderSystems\Direct3D11\include\OgreD3D11Plugin.h>
#endif
namespace OIV
{


    OIV::OIV() :
        fScrollState(this)
        , fIsRefresing(false)
        , fPass(NULL)
        , fParent(NULL)
        , fFilterLevel(2)
    {
        fImageLoader.InstallPlugin(new PluginJpeg());
        fImageLoader.InstallPlugin(new PluginPng());
        fImageLoader.InstallPlugin(new PluginFreeImage());
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
        using namespace Ogre;

#ifdef _DEBUG
        root = new Root();
#else
        // No log file
        root = new Root(BLANKSTRING, BLANKSTRING, BLANKSTRING);
#endif

#ifndef OGRE_STATIC_LIB
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
#else
        D3D11Plugin* plugin = new D3D11Plugin();
        plugin->install();

#endif
        

        using namespace Ogre;
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
            1280,                   // window width, in pixels
            800,                   // window height, in pixels
            false,                 // fullscreen or not
            &options);                    // use defaults for all other values

        window->setDeactivateOnFocusChange(false);
        

        WindowEventUtilities::addWindowEventListener(window, this);


        fScene = root->createSceneManager(ST_GENERIC, "MySceneManager");

        ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

        // If Direct3D11 then invert Y.
        //fScrollState.SetInvertedVCoordinate(true);
    }

    Ogre::Vector2 OIV::GetMousePosition()
    {
        using namespace Ogre;
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
        return Ogre::Vector2(fOpenedImage->GetWidth(), fOpenedImage->GetHeight());
    }

    Ogre::RenderWindow* OIV::GetWindow()
    {
        using namespace Ogre;
        return dynamic_cast<RenderWindow*>(Root::getSingleton().getRenderTarget("MainWindow"));
    }

    Ogre::Vector2 OIV::GetWindowSize()
    {
        using namespace Ogre;
        RenderWindow* wnd = GetWindow();
        return Vector2(wnd->getWidth(), wnd->getHeight());
    }

    void OIV::NotifyDirty()
    {
        Refresh();
    }

    void OIV::UpdateGpuParams()
    {
        using namespace Ogre;
        Vector2 uvScaleFixed = fScrollState.GetARFixedUVScale();
        Vector2 uvOffset = fScrollState.GetOffset();

        fFragmentParameters->setNamedConstant("uvScale", uvScaleFixed);
        fFragmentParameters->setNamedConstant("uvOffset", uvOffset);
    }



    // 'Ogre::WindowEventListener' implementation 
    void OIV::windowClosed(Ogre::RenderWindow *  rw)
    {
        PostQuitMessage(0);
    }

   
    void OIV::windowResized(Ogre::RenderWindow* rw)
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
        using namespace Ogre;
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

        rect = new Quad(true);

        rect->setCorners(-1, 1, 1, -1);
        AxisAlignedBox bb;
        bb.setInfinite();
        rect->setBoundingBox(bb);

        MaterialPtr material = MaterialManager::getSingleton().getByName("OgreViewer/QuadMaterial");
        rect->setMaterial("OgreViewer/QuadMaterial");
        fPass = material->getTechnique(0)->getPass(0);
        fFragmentParameters = fPass->getFragmentProgramParameters();
        fPass->getTextureUnitState(0)->setTextureAddressingMode(TextureUnitState::TAM_BORDER);
        ApplyFilter();
        
        fPass->setCullingMode(CULL_NONE);


        fScene->getRootSceneNode()->createChildSceneNode()->attachObject(rect);

        fScene->setAmbientLight(ColourValue(0.8f, 0.8f, 0.8f));
        fScene->setShadowTechnique(Ogre::ShadowTechnique::SHADOWTYPE_NONE);

        CreateCameraAndViewport();
    }
}
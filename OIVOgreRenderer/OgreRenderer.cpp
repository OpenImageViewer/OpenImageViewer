#include "OgreRenderer.h"
#include "OgreLogManager.h"
#include "ConsoleLogListener.h"
#include "ViewParameters.h"
#include "StringUtility.h"
#include "Utility.h"
#include <OgreViewport.h>
#include <OgreCamera.h>
#include "OgreRenderWindow.h"
#include "OgreD3D11Plugin.h"
#include "OgreMaterialManager.h"
#include "OgreTechnique.h"
#include "OgreSceneNode.h"
#include "OgreTextureManager.h"
#include "OgreHardwarePixelBuffer.h"

namespace OIV
{
    OgreRenderer::OgreRenderer() :
          fPass(nullptr)
        , fTextureName("OIVOpenedtexture")
    {

    }


    void OgreRenderer::TryLoadPlugin(std::string pluginName)
    {
        try
        {
            fRoot->loadPlugin(pluginName.c_str());
        }
        catch (...)
        {

        }
    }




    void OgreRenderer::SetupRenderer()
    {
        using namespace std;
        using namespace Ogre;

#ifdef _DEBUG
        fRoot = new Root();
#else
        // No log file
            fRoot = new Root(BLANKSTRING, BLANKSTRING, BLANKSTRING);
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
        const RenderSystemList& rlist = fRoot->getAvailableRenderers();
        RenderSystemList::const_iterator it = rlist.begin();
        while (it != rlist.end())
        {
            RenderSystem* rSys = *(it++);
            String name = rSys->getName();

            if (rSys->getName().find("Direct3D11") != String::npos)
            {
                fRoot->setRenderSystem(rSys);
                break;
            }
        }

        fRoot->initialise(false);


        ResourceGroupManager& rgm = ResourceGroupManager::getSingleton();
        std::string exeFolder = StringUtility::ToAString(Utility::GetExeFolder());
        rgm.addResourceLocation(exeFolder + String("\\Resources\\programs"), "FileSystem");
        rgm.addResourceLocation(exeFolder + String("\\Resources\\scripts"), "FileSystem");


        //Log for console output    
        LogManager::getSingleton().getDefaultLog()->addListener(new GameLogListener());

        NameValuePairList options;

        options["vsync"] = "true";
        if (fWindowContainer != NULL)
            options["externalWindowHandle"] = Ogre::StringConverter::toString(fWindowContainer);


        RenderWindow* window = fRoot->createRenderWindow(
            "MainWindow", // window name
            1280, // window width, in pixels
            800, // window height, in pixels
            false, // fullscreen or not
            &options); // use defaults for all other values

        window->setDeactivateOnFocusChange(false);


        //WindowEventUtilities::addWindowEventListener(window, this);


        fScene = fRoot->createSceneManager(ST_GENERIC, "MySceneManager");

        ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

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
        fPass->setCullingMode(CULL_NONE);
        fScene->getRootSceneNode()->createChildSceneNode()->attachObject(rect);
        fScene->setShadowTechnique(Ogre::ShadowTechnique::SHADOWTYPE_NONE);
        
        Camera* camera = fScene->createCamera("MainCamera");
        fViewPort = window->addViewport(camera);
        fViewPort->setClearEveryFrame(true);
    }

    Ogre::Vector2 ToOgre(const OIV::Vector2& vector)
    {
        return Ogre::Vector2(static_cast<Ogre::Real>(vector.x), static_cast<Ogre::Real>(vector.y));
    }

#pragma region Irenderer override
    int OgreRenderer::SetViewParams(const ViewParameters& viewParams)
    {
        fFragmentParameters->setNamedConstant("uvScale", ToOgre(viewParams.uvscale));
        fFragmentParameters->setNamedConstant("uvOffset", ToOgre(viewParams.uvOffset));
        fFragmentParameters->setNamedConstant("uViewportSize", ToOgre(viewParams.uViewportSize));
        fFragmentParameters->setNamedConstant("uShowGrid", viewParams.showGrid == true ? 1 : 0);
        return 0;
    }

    Ogre::PixelFormat OgreRenderer::ResolveSourcePixelFormat(ImageType imageType)
    {
        using namespace  Ogre;
        PixelFormat format;
        switch (imageType)
        {
        case  IT_BYTE_RGB:
            format = PF_R8G8B8;
            break;
        case IT_BYTE_BGRA:
            format = PF_B8G8R8A8;
            break;
        case  IT_BYTE_RGBA:
            format = PF_A8B8G8R8;
            break;
        case IT_BYTE_BGR:
            format = PF_B8G8R8;
            break;
        case IT_BYTE_ARGB:
            format = PF_A8R8G8B8;
            break;
        case IT_BYTE_ABGR:
            format = PF_A8B8G8R8;
        case IT_BYTE_8BIT:
            format = PF_L8;
            break;
        default:
            format = PF_UNKNOWN;

        }
        return format;
    }

    int OgreRenderer::SetImage(const ImageSharedPtr image)
    {
        using namespace Ogre;
        TextureManager& texMan = TextureManager::getSingleton();
        TexturePtr tex = texMan.getByName(fTextureName);
        if (tex.isNull() == false)
            texMan.remove(fTextureName);

        if (image.get())
        {
            tex = texMan.createManual(
                fTextureName
                , ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME
                , TEX_TYPE_2D
                , image->GetWidth()// width
                , image->GetHeight() // height
                , 1 // depth
                , 0 // num mipmaps
                , Ogre::PF_R8G8B8A8); // pixel format


            PixelFormat format = ResolveSourcePixelFormat(image->GetImageType());

            if (format != PF_UNKNOWN)
            {

                PixelBox src(static_cast<uint32>(image->GetWidth())
                    , static_cast<uint32>(image->GetHeight())
                    , 1
                    , format
                    , const_cast<void*>(image->GetBuffer()));
                src.rowPitch = image->GetRowPitchInTexels();
                src.slicePitch = image->GetSlicePitchInTexels();
                Ogre::Image::Box dest(0, 0, image->GetWidth(), image->GetHeight());
                HardwarePixelBufferSharedPtr buf = tex->getBuffer();
                buf->blitFromMemory(src, dest);

                fPass->getTextureUnitState(0)->setTextureName(fTextureName);

                fFragmentParameters->setNamedConstant("uImageSize", Ogre::Vector2(image->GetWidth(), image->GetHeight()));

                return 0;
            }

        }
        return 1;
    }

    int OgreRenderer::Init(size_t container)
    {
        fWindowContainer = container;
        SetupRenderer();
        return 0;
    }

    int OgreRenderer::Redraw()
    {
        fRoot->renderOneFrame();
        return 0;
    }

    int OgreRenderer::SetFilterLevel(int filterLevel)
    {
        using namespace Ogre;
        switch (filterLevel)
        {
        case 0:
            fPass->getTextureUnitState(0)->setTextureFiltering(TFO_NONE);
            break;
        case 1:
            fPass->getTextureUnitState(0)->setTextureFiltering(TFO_BILINEAR);
            break;
        }

        return 0;
    }
#pragma endregion 
}

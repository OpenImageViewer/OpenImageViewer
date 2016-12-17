#pragma once
#include <interfaces/IRenderer.h>
#include <Image/Image.h>
#include <OgrePrerequisites.h>
#include "quad.h"
#include "OgrePixelFormat.h"

namespace OIV
{

    class OgreRenderer : public IRenderer
    {
    
    public:
        OgreRenderer();
        
#pragma region  Irenderer overrides
    public:
        int SetViewParams(const ViewParameters& viewParams) override;
        int SetImage(const ImageSharedPtr image) override;
        int Init(size_t container) override;
        int Redraw() override;
        int SetFilterLevel(OIV_Filter_type filterType) override;

#pragma endregion

#pragma region Private methods
        private:
        void SetupRenderer();
        void TryLoadPlugin(std::string pluginName);
        Ogre::PixelFormat ResolveSourcePixelFormat(ImageType image);
#pragma endregion
        
#pragma region  Private member fields 
        private:
            size_t fWindowContainer;
            Ogre::Pass* fPass;
            const std::string fTextureName;
            Ogre::Root* fRoot;
            Ogre::SceneManager* fScene;
            Ogre::Viewport* fViewPort;
            Quad* rect;
            Ogre::GpuProgramParametersSharedPtr  fFragmentParameters;
            
#pragma endregion
    };
}


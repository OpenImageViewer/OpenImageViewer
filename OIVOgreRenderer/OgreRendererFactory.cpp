#include "OgreRendererFactory.h"
#include "OgreRenderer.h"
namespace  OIV
{

    IRendererSharedPtr OgreRendererFactory::Create()
    {
        return IRendererSharedPtr(new OgreRenderer());
    }
}
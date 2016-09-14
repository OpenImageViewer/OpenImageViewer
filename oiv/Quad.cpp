#include "OgreStableHeaders.h"
#include "quad.h"

#include "OgreHardwareBufferManager.h"

namespace OIV {
#define POSITION_BINDING 0

    Quad::Quad(bool includeTextureCoords, Ogre::HardwareBuffer::Usage vBufUsage)
        : SimpleRenderable()
    {
        _initRectangle2D(includeTextureCoords, vBufUsage);
    }

    Quad::Quad(const Ogre::String& name, bool includeTextureCoords, Ogre::HardwareBuffer::Usage vBufUsage)
        : SimpleRenderable(name)
    {
        _initRectangle2D(includeTextureCoords, vBufUsage);
    }

    void Quad::_initRectangle2D(bool includeTextureCoords, Ogre::HardwareBuffer::Usage vBufUsage)
    {
        using namespace Ogre;
        // use identity projection and view matrices
        mUseIdentityProjection = true;
        mUseIdentityView = true;

        mRenderOp.vertexData = OGRE_NEW VertexData();

        mRenderOp.indexData = 0;
        mRenderOp.vertexData->vertexCount = 4;
        mRenderOp.vertexData->vertexStart = 0;
        mRenderOp.operationType = RenderOperation::OT_TRIANGLE_STRIP;
        mRenderOp.useIndexes = false;
        mRenderOp.useGlobalInstancingVertexBufferIsAvailable = false;

        VertexDeclaration* decl = mRenderOp.vertexData->vertexDeclaration;
        VertexBufferBinding* bind = mRenderOp.vertexData->vertexBufferBinding;

        decl->addElement(POSITION_BINDING, 0, VET_FLOAT2, VES_POSITION);


        HardwareVertexBufferSharedPtr vbuf =
            HardwareBufferManager::getSingleton().createVertexBuffer(
                decl->getVertexSize(POSITION_BINDING),
                mRenderOp.vertexData->vertexCount,
                vBufUsage);

        // Bind buffer
        bind->setBinding(POSITION_BINDING, vbuf);
        
    }

    Quad::~Quad()
    {
        OGRE_DELETE mRenderOp.vertexData;
    }

    void Quad::setCorners(Ogre::Real left, Ogre::Real top, Ogre::Real right, Ogre::Real bottom, bool updateAABB)
    {
        using namespace Ogre;
        HardwareVertexBufferSharedPtr vbuf =
            mRenderOp.vertexData->vertexBufferBinding->getBuffer(POSITION_BINDING);
        float* pFloat = static_cast<float*>(vbuf->lock(HardwareBuffer::HBL_DISCARD));

        *pFloat++ = -1.0f;
        *pFloat++ = 1.0f;

        *pFloat++ = 1.0f;
        *pFloat++ = 1.0f;

        *pFloat++ = -1.0f;
        *pFloat++ = -1.0f;

        *pFloat++ = 1.0f;
        *pFloat++ = -1.0f;

        vbuf->unlock();

        if (updateAABB)
        {
            mBox.setExtents(
                std::min(left, right), std::min(top, bottom), 0,
                std::max(left, right), std::max(top, bottom), 0);
        }
    }


    // Override this method to prevent parent transforms (rotation,translation,scale)
    void Quad::getWorldTransforms(Ogre::Matrix4* xform) const
    {
        using namespace Ogre;
        // return identity matrix to prevent parent transforms
        *xform = Matrix4::IDENTITY;
    }


}


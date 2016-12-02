#ifndef _Rectangle2DC_H__
#define _Rectangle2DC_H__

#include "OgrePrerequisites.h"

#include "OgreSimpleRenderable.h"

namespace OIV {

    /** \addtogroup Core
    *  @{
    */
    /** \addtogroup Scene
    *  @{
    */
    /** Allows the rendering of a simple 2D rectangle
    This class renders a simple 2D rectangle; this rectangle has no depth and
    therefore is best used with specific render queue and depth settings,
    like RENDER_QUEUE_BACKGROUND and 'depth_write off' for backdrops, and
    RENDER_QUEUE_OVERLAY and 'depth_check off' for fullscreen quads.
    */
    class  Quad : public Ogre::SimpleRenderable
    {
    protected:
        /** Override this method to prevent parent transforms (rotation,translation,scale)
        */
        void getWorldTransforms(Ogre::Matrix4* xform) const;

        void _initRectangle2D(bool includeTextureCoords, Ogre::HardwareBuffer::Usage vBufUsage);

    public:

        Quad(bool includeTextureCoordinates = false, Ogre::HardwareBuffer::Usage vBufUsage = Ogre::HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY);
        Quad(const Ogre::String& name, bool includeTextureCoordinates = false, Ogre::HardwareBuffer::Usage vBufUsage = Ogre::HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY);
        ~Quad();

        /** Sets the corners of the rectangle, in relative coordinates.
        @param
        left Left position in screen relative coordinates, -1 = left edge, 1.0 = right edge
        @param top Top position in screen relative coordinates, 1 = top edge, -1 = bottom edge
        @param right Right position in screen relative coordinates
        @param bottom Bottom position in screen relative coordinates
        @param updateAABB Tells if you want to recalculate the AABB according to
        the new corners. If false, the axis aligned bounding box will remain identical.
        */
        void setCorners(Ogre::Real left, Ogre::Real top, Ogre::Real right, Ogre::Real bottom, bool updateAABB = true);

        /** Sets the normals of the rectangle
        */
        

        /** Sets the UVs of the rectangle
        @remarks
        Doesn't do anything if the rectangle wasn't built with texture coordinates
        */

        Ogre::Real getSquaredViewDepth(const Ogre::Camera* cam) const
        {
            (void)cam; return 0;
        }

        Ogre::Real getBoundingRadius(void) const { return 0; }

    };
    /** @} */
    /** @} */

}// namespace

#endif



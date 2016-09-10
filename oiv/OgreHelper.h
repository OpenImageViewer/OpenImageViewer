#pragma once
#include "OgreCommon.h"
namespace OIV
{
    class OgreHelper
    {
    public:

        static bool OgreLoadImage(const Ogre::String& texture_name, const Ogre::String& texture_path)
        {
            using namespace Ogre;
            Image image;
            if (OgreOpenImage(texture_path, image))
            {
                OgreLoadImageToTexture(image, texture_name);
                return true;
            }
            
            return false;
        }

        static TexturePtr OgreLoadImageToTexture(const Ogre::Image& image, const Ogre::String& texture_name)
        {
            return Ogre::TextureManager::getSingleton().loadImage(texture_name,
                Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, image, Ogre::TEX_TYPE_2D, 0, 1.0f, true, PF_R8G8B8A8, false);
        }

        static bool OgreOpenImage(const Ogre::String& imagePath, Ogre::Image& image)
        {
            bool image_loaded = false;
            std::ifstream ifs(imagePath.c_str(), std::ios::binary | std::ios::in);
            if (ifs.is_open())
            {
                Ogre::String tex_ext;
                Ogre::String::size_type index_of_extension = imagePath.find_last_of('.');
                if (index_of_extension != Ogre::String::npos)
                {
                    tex_ext = imagePath.substr(index_of_extension + 1);
                    Ogre::DataStreamPtr data_stream(new Ogre::FileStreamDataStream(imagePath, &ifs, false));
                    image.load(data_stream, tex_ext);
                    image_loaded = true;
                }
                ifs.close();
            }
            return image_loaded;
        }

    };
}
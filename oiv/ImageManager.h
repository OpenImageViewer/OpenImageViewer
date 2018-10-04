#pragma once
#include <list>
#include <map>
#include "APi/defs.h"
#include <image.h>


namespace OIV
{
    class ImageManager
    {
    public:
        const int16_t MAX_IMAGES = 100;
        using VecImageHandles = std::vector<ImageHandle>;
        typedef std::list<ImageHandle> ListImageHandles;
        typedef std::map<ImageHandle,IMCodec::ImageSharedPtr> MapHandleToImage;
        using MapHandleToChildren = std::map<ImageHandle, VecImageHandles>;
        ImageManager();
        std::size_t GetNumLoadedImages() const;
        std::size_t GetNumImagesVacancy() const;
        ImageHandle AddImage(const IMCodec::ImageSharedPtr& image);
        ImageHandle AddChildImage(const IMCodec::ImageSharedPtr& image, ImageHandle parent);
        bool RemoveImage(ImageHandle handle);
        IMCodec::ImageSharedPtr GetImage(ImageHandle handle) const;
        void ReplaceImage(ImageHandle handle, IMCodec::ImageSharedPtr image);
        VecImageHandles GetChildrenOf(ImageHandle handle);

    private: //methods
        ImageHandle AllocateImageHandle();
        void DeallocateHandle(ImageHandle handle);
        bool RemoveChildren(ImageHandle handle);
        

    private: // member fields
        MapHandleToChildren fMapHandleToChildren;
        MapHandleToImage fMapHandleToImage;
        ListImageHandles fListFreeHandles;
    };
}

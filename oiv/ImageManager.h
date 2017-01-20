#pragma once
#include <unordered_map>
#include "APi/defs.h"
#include <image.h>

namespace OIV
{
    class ImageManager
    {
    public:
        const int16_t MAX_IMAGES = 100;
        typedef std::list<ImageHandle> ListImageHandles;
        typedef std::unordered_map<ImageHandle,IMCodec::ImageSharedPtr> MapHandleToImage;

        ImageManager();


        std::size_t GetNumLoadedImages() const;


        std::size_t GetNumImagesVacancy() const;

        ImageHandle AllocateImageHandle();

        void DeallocateHandle(ImageHandle handle);

        ImageHandle AddImage(const IMCodec::ImageSharedPtr& image);

        bool RemoveImage(ImageHandle handle);

        IMCodec::ImageSharedPtr GetImage(ImageHandle handle) const;


    private:
        MapHandleToImage fMapHandleToImage;
        ListImageHandles fListFreeHandles;
    };
}

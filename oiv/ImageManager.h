#pragma once
#include <unordered_set>
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

        ImageManager()
        {
            for (int16_t i = 1; i < MAX_IMAGES + 1; i++)
                fListFreeHandles.push_back(static_cast<ImageHandle>(i));
        }

        std::size_t GetNumLoadedImages() const
        {
            return MAX_IMAGES - GetNumImagesVacancy();
        }


        std::size_t GetNumImagesVacancy() const
        {
            return fListFreeHandles.size();
        }

        ImageHandle AllocateImageHandle()
        {
            ImageHandle handle = fListFreeHandles.back();
            fListFreeHandles.pop_back();
            return handle;
        }

        void  DeallocateHandle(ImageHandle handle)
        {
            fListFreeHandles.push_back(handle);
        }
        
        ImageHandle AddImage(const IMCodec::ImageSharedPtr& image)
        {
            if (GetNumImagesVacancy() > 0 )
            {
                auto pair = fMapHandleToImage.insert(std::make_pair(AllocateImageHandle(), image));
                if (pair.second == false)
                    throw std::logic_error("Bad or corrupted data");

                return pair.first->first;
            }

            return ImageNullHandle;
        }

        bool RemoveImage(ImageHandle handle)
        {
            auto it = fMapHandleToImage.find(handle);
            if (it != fMapHandleToImage.end())
            {
                DeallocateHandle(handle);
                fMapHandleToImage.erase(it);
                return true;
            }
            return false;
        }

        IMCodec::ImageSharedPtr GetImage(ImageHandle handle) const
        {
            IMCodec::ImageSharedPtr image = nullptr;
            if (handle != ImageNullHandle)
            {
                auto it = fMapHandleToImage.find(handle);
                if (it != fMapHandleToImage.end())
                    image = it->second;
            }

            return image;
        }

        

    private:
        MapHandleToImage fMapHandleToImage;
        ListImageHandles fListFreeHandles;
    };
}

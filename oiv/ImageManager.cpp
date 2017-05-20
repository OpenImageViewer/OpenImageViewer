#include "ImageManager.h"

namespace OIV
{
    ImageManager::ImageManager()
    {
        for (int16_t i = MAX_IMAGES + 1; i >= 1; i--)
            fListFreeHandles.push_back(static_cast<ImageHandle>(i));
    }

    std::size_t ImageManager::GetNumLoadedImages() const
    {
        return MAX_IMAGES - GetNumImagesVacancy();
    }

    std::size_t ImageManager::GetNumImagesVacancy() const
    {
        return fListFreeHandles.size();
    }

    ImageHandle ImageManager::AllocateImageHandle()
    {
        ImageHandle handle = fListFreeHandles.back();
        fListFreeHandles.pop_back();
        return handle;
    }

    void ImageManager::DeallocateHandle(ImageHandle handle)
    {
        fListFreeHandles.push_back(handle);
    }

    ImageHandle ImageManager::AddImage(const IMCodec::ImageSharedPtr& image)
    {
        if (GetNumImagesVacancy() > 0)
        {
            auto pair = fMapHandleToImage.insert(std::make_pair(AllocateImageHandle(), image));
            if (pair.second == false)
                throw std::logic_error("Bad or corrupted data");

            return pair.first->first;
        }

        return ImageNullHandle;
    }

    bool ImageManager::RemoveImage(ImageHandle handle)
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

    IMCodec::ImageSharedPtr ImageManager::GetImage(ImageHandle handle) const
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

    void ImageManager::ReplaceImage(ImageHandle handle, IMCodec::ImageSharedPtr image)
    {
        if (handle != ImageNullHandle)
        {
            auto it = fMapHandleToImage.find(handle);
            if (it != fMapHandleToImage.end())
                it->second = image;
        }
    }
}

#include "ImageManager.h"
#include "Exception.h"

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
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::DuplicateItem, "Image manager, duplicate image found");

            return pair.first->first;
        }

        return ImageHandleNull;
    }

    ImageHandle ImageManager::AddChildImage(const IMCodec::ImageSharedPtr& image,ImageHandle parent)
    {
        IMCodec::ImageSharedPtr parentImage = GetImage(parent);
        if (parentImage == nullptr)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::DuplicateItem, "Image manager, parent image not found");

        ImageHandle childHandle = AddImage(image);
        fMapHandleToChildren[parent].push_back(childHandle);
        return childHandle;
    }


    bool ImageManager::RemoveImage(ImageHandle handle)
    {
        auto it = fMapHandleToImage.find(handle);
        if (it != fMapHandleToImage.end())
        {
            RemoveChildren(handle);
            DeallocateHandle(handle);
            fMapHandleToImage.erase(it);
            return true;
        }
        return false;
    }


    bool ImageManager::RemoveChildren(ImageHandle handle)
    {
        auto it = fMapHandleToChildren.find(handle);

        if (it != fMapHandleToChildren.end())
        {
            auto& children = it->second;
            for (ImageHandle handle : children)
                RemoveImage(handle);

            fMapHandleToChildren.erase(it);
        }
        else
        {
            return false;
        }

        return true;
    }

    IMCodec::ImageSharedPtr ImageManager::GetImage(ImageHandle handle) const
    {
        IMCodec::ImageSharedPtr image = nullptr;
        if (handle != ImageHandleNull)
        {
            auto it = fMapHandleToImage.find(handle);
            if (it != fMapHandleToImage.end())
                image = it->second;
        }

        return image;
    }

    void ImageManager::ReplaceImage(ImageHandle handle, IMCodec::ImageSharedPtr image)
    {
        fMapHandleToImage[handle] = image;
    }

    ImageManager::VecImageHandles ImageManager::GetChildrenOf(ImageHandle handle)
    {
        auto it = fMapHandleToChildren.find(handle);

        if (it != fMapHandleToChildren.end())
            return it->second;
        else
            return VecImageHandles();
    }
}

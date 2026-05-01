#pragma once

#include <OIVAppCore/CommandManager.h>

#include <ImageUtil/AxisAlignedTransform.h>

#include <string>

namespace OIV
{
    struct AxisAlignedTransformCommand
    {
        IMUtil::AxisAlignedRotation rotation = IMUtil::AxisAlignedRotation::None;
        IMUtil::AxisAlignedFlip flip = IMUtil::AxisAlignedFlip::None;

        bool HasTransform() const
        {
            return rotation != IMUtil::AxisAlignedRotation::None || flip != IMUtil::AxisAlignedFlip::None;
        }
    };

    class ImageTransformCommandPolicy
    {
      public:
        static AxisAlignedTransformCommand ParseAxisAlignedTransform(const CommandManager::CommandArgs& args);
        static std::wstring FormatAxisAlignedTransformResult(IMUtil::AxisAlignedRotation rotation,
                                                             IMUtil::AxisAlignedFlip flip);
    };
}

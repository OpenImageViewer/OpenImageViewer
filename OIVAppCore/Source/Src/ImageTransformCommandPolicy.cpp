#include <OIVAppCore/ImageTransformCommandPolicy.h>

namespace OIV
{
    AxisAlignedTransformCommand ImageTransformCommandPolicy::ParseAxisAlignedTransform(
        const CommandManager::CommandArgs& args)
    {
        const std::string type = args.GetArgValue("type");

        if (type == "hflip")
            return {IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Horizontal};
        if (type == "vflip")
            return {IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Vertical};
        if (type == "rotatecw")
            return {IMUtil::AxisAlignedRotation::Rotate90CW, IMUtil::AxisAlignedFlip::None};
        if (type == "rotateccw")
            return {IMUtil::AxisAlignedRotation::Rotate90CCW, IMUtil::AxisAlignedFlip::None};

        return {};
    }

    std::wstring ImageTransformCommandPolicy::FormatAxisAlignedTransformResult(
        IMUtil::AxisAlignedRotation rotation,
        IMUtil::AxisAlignedFlip flip)
    {
        std::wstring rotationText;
        switch (rotation)
        {
            case IMUtil::AxisAlignedRotation::Rotate90CW:
                rotationText = L"90 degrees clockwise";
                break;
            case IMUtil::AxisAlignedRotation::Rotate180:
                rotationText = L"180 degrees";
                break;
            case IMUtil::AxisAlignedRotation::Rotate90CCW:
                rotationText = L"90 degrees counter clockwise";
                break;
            case IMUtil::AxisAlignedRotation::None:
                break;
        }

        std::wstring result;
        if (rotationText.empty() == false)
            result += std::wstring(L"Rotation <textcolor=#7672ff>(") + rotationText + L')';

        std::wstring flipText;
        switch (flip)
        {
            case IMUtil::AxisAlignedFlip::Horizontal:
                flipText = L"horizontal";
                break;
            case IMUtil::AxisAlignedFlip::Vertical:
                flipText = L"vertical";
                break;
            case IMUtil::AxisAlignedFlip::None:
                break;
        }

        if (flipText.empty() == false)
        {
            if (rotationText.empty() == false)
                result += L'\n';

            result += std::wstring(L"<textcolor=#ff8930>Flip <textcolor=#7672ff>(") + flipText + L')';
        }

        if (result.empty())
            result = L"No transformation";

        return result;
    }
}

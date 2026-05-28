#include <LLUtils/StringDefs.h>
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

    LLUtils::native_string_type ImageTransformCommandPolicy::FormatAxisAlignedTransformResult(
        IMUtil::AxisAlignedRotation rotation, IMUtil::AxisAlignedFlip flip)
    {
        LLUtils::native_string_type rotationText;
        switch (rotation)
        {
            case IMUtil::AxisAlignedRotation::Rotate90CW:
                rotationText = LLUTILS_TEXT("90 degrees clockwise");
                break;
            case IMUtil::AxisAlignedRotation::Rotate180:
                rotationText = LLUTILS_TEXT("180 degrees");
                break;
            case IMUtil::AxisAlignedRotation::Rotate90CCW:
                rotationText = LLUTILS_TEXT("90 degrees counter clockwise");
                break;
            case IMUtil::AxisAlignedRotation::None:
                break;
        }

        LLUtils::native_string_type result;
        if (rotationText.empty() == false)
            result += LLUtils::native_string_type(LLUTILS_TEXT("Rotation <textcolor=#7672ff>(")) + rotationText +
                      LLUTILS_TEXT(')');

        LLUtils::native_string_type flipText;
        switch (flip)
        {
            case IMUtil::AxisAlignedFlip::Horizontal:
                flipText = LLUTILS_TEXT("horizontal");
                break;
            case IMUtil::AxisAlignedFlip::Vertical:
                flipText = LLUTILS_TEXT("vertical");
                break;
            case IMUtil::AxisAlignedFlip::None:
                break;
        }

        if (flipText.empty() == false)
        {
            if (rotationText.empty() == false)
                result += LLUTILS_TEXT('\n');

            result += LLUtils::native_string_type(LLUTILS_TEXT("<textcolor=#ff8930>Flip <textcolor=#7672ff>(")) +
                      flipText + LLUTILS_TEXT(')');
        }

        if (result.empty())
            result = LLUTILS_TEXT("No transformation");

        return result;
    }
}  // namespace OIV

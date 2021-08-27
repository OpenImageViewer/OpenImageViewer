#include "OIVTextImage.h"
#include "../OIVCommands.h"
namespace OIV
{
    ResultCode OIVTextImage::DoUpdate()
    {

        ResultCode result = RC_Success;

        if (fTextOptionsCached != fTextOptionsCurrent)
        {
            FreeImage();
            result = CreateText();
            ResetActiveImageProperties();
        }

        if (result == RC_Success)
            result = OIVBaseImage::DoUpdate();

        return result;
    }

    ResultCode OIVTextImage::CreateText()
    {
        OIV_CMD_CreateText_Response textResponse = {};
        OIV_CMD_CreateText_Request  textRequest = {};

        textRequest.text = fTextOptionsCurrent.text.c_str();
        textRequest.fontPath = fTextOptionsCurrent.fontPath.c_str();
        textRequest.fontSize = fTextOptionsCurrent.fontSize;
        reinterpret_cast<LLUtils::Color&>(textRequest.backgroundColor) = reinterpret_cast<LLUtils::Color&>(fTextOptionsCurrent.backgroundColor);
        textRequest.outlineWidth = fTextOptionsCurrent.outlineWidth;
        textRequest.outlineColor = fTextOptionsCurrent.outlineColor;
        textRequest.DPIx = fTextOptionsCurrent.DPIx;
        textRequest.DPIy = fTextOptionsCurrent.DPIy;
        textRequest.renderMode = fTextOptionsCurrent.renderMode;


        ResultCode result = OIVCommands::ExecuteCommand(OIV_CMD_CreateText, &textRequest, &textResponse);
        if (result == RC_Success)
        {
            fTextOptionsCached = fTextOptionsCurrent;
            SetImageHandle(textResponse.imageHandle);
            QueryImageInfo();
        }

        return result;
    }
}
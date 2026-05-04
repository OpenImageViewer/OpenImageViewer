#include "ApiGlobal.h"
#include "IPictureRenderer.h"
#include "OIV.h"
namespace OIV
{
    std::unique_ptr<IPictureRenderer> ApiGlobal::sPictureRenderer = std::make_unique<OIV>();
    CommandProcessor ApiGlobal::sCommandProcessor;
}
#include "ApiGlobal.h"
#include "oiv.h"
namespace OIV
{
    std::unique_ptr<IPictureRenderer> ApiGlobal::sPictureRenderer = std::unique_ptr<IPictureRenderer>(new OIV());
    CommandProcessor ApiGlobal::sCommandProcessor;
}
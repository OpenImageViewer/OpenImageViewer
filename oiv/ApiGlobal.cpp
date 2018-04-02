#include "ApiGlobal.h"
#include "oiv.h"
LL_EXCEPTION_DECLARE_HANDLER;
namespace OIV
{
    std::unique_ptr<IPictureRenderer> ApiGlobal::sPictureRenderer = std::unique_ptr<IPictureRenderer>(new OIV());
    CommandProcessor ApiGlobal::sCommandProcessor;
}
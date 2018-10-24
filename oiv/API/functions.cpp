#include "functions.h"
#include "../ApiImpl.h"

ResultCode __cdecl OIV_Execute(int command, std::size_t requestSize, void* requestData, std::size_t responseSize, void* responseData)
{
    //Call the c++ implementation
    return OIV::Execute_impl(static_cast<CommandExecute>(command), requestSize, requestData, responseSize, responseData);
}

ResultCode __cdecl OIV_Util_GetBPPFromTexelFormat(OIV_TexelFormat in_texelFormat, uint8_t* out_bpp)
{
    //Call the c++ implementation
    return OIV::Util::GetBPPFromTexelFormat_impl(in_texelFormat, out_bpp);

}
#pragma once
#include "API\defs.h"
namespace OIV
{
    ResultCode Execute_impl(int command, std::size_t requestSize, void* requestData, std::size_t responseSize, void* responseData);
}


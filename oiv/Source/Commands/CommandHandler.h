#pragma once
#include <API/defs.h>

namespace OIV
{
#define VERIFY_OPTIONAL_RESPONSE(REQ,REQ_SIZE,RES,RES_SIZE) sizeof(REQ) != REQ_SIZE ?\
     ResultCode::RC_BadRequestSize :\
    (sizeof(RES) != responseSize && sizeof(CmdNull) != responseSize) ? ResultCode::RC_BadResponseSize\
        : ResultCode::RC_Success;

#define VERIFY(REQ,REQ_SIZE,RES,RES_SIZE) (sizeof(REQ) != REQ_SIZE ?\
     ResultCode::RC_BadRequestSize :\
    sizeof(RES) != responseSize ? ResultCode::RC_BadResponseSize\
        : ResultCode::RC_Success); 

#define VERIFY_REQUEST(REQ,REQ_SIZE) (sizeof(REQ) != REQ_SIZE ?\
     ResultCode::RC_BadRequestSize : ResultCode::RC_Success);

#define VERIFY_RESPONSE(RES,RES_SIZE) (sizeof(RES) != RES_SIZE ?\
     ResultCode::RC_BadResponseSize : ResultCode::RC_Success);


    class CommandHandler
    {
    public:
        ResultCode Execute(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize)
        {
            ResultCode rc = Verify(requestSize, responseSize);
            if (rc == ResultCode::RC_Success)
                rc = ExecuteImpl(request, requestSize, response, responseSize);

            return rc;
        }

    protected:
        virtual ResultCode Verify(std::size_t requestSize, std::size_t responseSize) { return RC_Success; }

        virtual ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) = 0;
    };
}
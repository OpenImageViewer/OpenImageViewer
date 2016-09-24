
#pragma once
#include <cwchar>

#ifdef OIV_NO_CLIENT_BUILD
    #define OIV_EXPOSE_FUNCTION __declspec(dllexport)
#else
    #define OIV_EXPOSE_FUNCTION __declspec( dllimport )
#endif

extern "C"
{

#ifdef UNICODE
    typedef wchar_t OIVCHAR;
#else
    typedef char OIVCHAR;
#endif



    enum CommandExecute
    {
          CE_NoOperation
        , CE_Init
        , CE_Destory
        , CE_LoadFile
        , CE_Zoom
        , CE_Pan
        , CE_Filter
        , CE_Refresh
        , CE_GetFileInformation
    };

    
    enum ResultCode
    {
          RC_Success
        , RC_InvalidParameters
        , RC_FileNotFound
        , RC_FileNotSupported
        , RC_WrongDataSize
        , RC_NotInitialized
        , RC_AlreadyInitialized
        , RC_UknownError = 0xFF

    };

#pragma pack(push, 1) 
    //-------Command Structs-------------------------

    struct CmdNull
    {
        
    };


    struct CmdDataZoom
    {
        double amount;
    };

    struct CmdResponseLoad
    {
        double loadTime;
        int width;
        int height;
        int bpp;
    };

    struct CmdDataPan
    {
        double x;
        double y;
    };

    struct CmdDataLoadFile
    {
        size_t FileNamelength;
        OIVCHAR* filePath;
    };

    struct CmdDataInit
    {
        size_t parentHandle;
    };

  /*  struct CmdDataClientMetrics
    {
        int width;
        int height;
    };*/

    //-------Query Structs-------------------------


    struct QryFileInformation
    {
        int width;
        int height;
        int bitsPerPixel;
        int numMipMaps;
        int numChannels;
        int imageDataSize;
        int rowPitchInBytes;
        int hasTransparency;
    };

#pragma pack(pop) 

}

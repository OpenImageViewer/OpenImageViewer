
#pragma once
#include <cwchar>
#include <stdint.h>

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

    // Naming convention.
    // Command identifier   - OIV_CMD_[CommandName].
    // Command request      - OIV_CMD_[CommandName]Request.
    // Command response     - OIV_CMD_[CommandName]Response.




    enum CommandExecute
    {
          CE_NoOperation
        , CE_Init
        , CE_Destory
        , CE_LoadFile
        , CE_Zoom
        , CE_Pan
        , CE_FilterLevel
        , CE_Refresh
        , CE_GetFileInformation
        , CE_TexelAtMousePos
        , CE_TexelGrid
        , CMD_SetClientSize
        , CMD_GetNumTexelsInCanvas
        , OIV_CMD_Destroy
    };

    
    enum ResultCode
    {
          RC_Success
        , RC_InvalidParameters
        , RC_FileNotFound
        , RC_FileNotSupported
        , RC_UnsupportedFormat
        , RC_WrongDataSize
        , RC_NotInitialized
        , RC_AlreadyInitialized
        , RC_WrongParameters
        , RC_PixelFormatConversionFailed
        , RC_UknownError = 0xFF
        , RC_InternalError = 0xFF + 1

    };

    //-------Command Structs-------------------------
#pragma pack(1)

    struct CmdNull
    {
        
    };

    struct CmdSetClientSizeRequest
    {
        uint16_t width;
        uint16_t height;
    };

    struct CmdGetNumTexelsInCanvasResponse
    {
        double width;
        double height;
    };

    struct CmdRequestTexelGrid
    {
        double gridSize;
    };

    struct CmdRequestTexelAtMousePos
    {
        int x;
        int y;
    };
    
    struct CmdResponseTexelAtMousePos
    {
        double x;
        double y;
    };

    enum OIV_Filter_type
    {
          FT_None
        , FT_Linear
        , FT_Lanczos3
        , FT_Count
    };

    struct OIV_CMD_Filter_Request
    {
        OIV_Filter_type filterType;
    };

    struct CmdDataZoom
    {
        double amount;
        int zoomX;
        int zoomY;
    };

    struct CmdResponseLoad
    {
        double loadTime;
        uint32_t width;
        uint32_t height;
        uint8_t bpp;
    };

    struct CmdDataPan
    {
        double x;
        double y;
    };

    struct CmdDataLoadFile
    {
        static const int EXTENSION_SIZE = 16;
        size_t length;
        void* buffer;
        char extension[EXTENSION_SIZE];
        bool onlyRegisteredExtension;
    };

    struct CmdDataInit
    {
        size_t parentHandle;
    };

  

    //-------Query Structs-------------------------


    struct QryFileInformation
    {
        size_t width;
        size_t height;
        size_t bitsPerPixel;
        size_t numMipMaps;
        size_t numChannels;
        size_t imageDataSize;
        size_t rowPitchInBytes;
        size_t hasTransparency;
    };

#pragma pack() 

}

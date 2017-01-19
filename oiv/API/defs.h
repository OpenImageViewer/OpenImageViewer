
#pragma once
#include <cwchar>
#include <cstdint>

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
        , OIV_CMD_AxisAlignedTransform
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
        , RC_UnknownCommand
        , RC_BadRequestSize
        , RC_BadResponseSize
        , RC_UknownError = 0xFF
        , RC_InternalError = 0xFF + 1

    };

    //-------Command Structs-------------------------
    enum OIV_AxisAlignedRTransform
    {
          AAT_None
        , AAT_Rotate90CW
        , AAT_Rotate270CCW = AAT_Rotate90CW
        , AAT_Rotate90CCW
        , AAT_Rotate270CW = AAT_Rotate90CCW
        , AAT_Rotate180
        , AAT_FlipVertical
        , AAT_FlipHorizontal
    };

#pragma pack(1)

    struct  CmdNull
    {

    };

    struct OIV_CMDAxisalignedTransformRequest
    {
        OIV_AxisAlignedRTransform transform;
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
        std::size_t length;
        void* buffer;
        char extension[EXTENSION_SIZE];
        bool onlyRegisteredExtension;
    };

    struct CmdDataInit
    {
        std::size_t parentHandle;
    };

  

    //-------Query Structs-------------------------


    struct QryFileInformation
    {
        std::size_t width;
        std::size_t height;
        std::size_t bitsPerPixel;
        std::size_t numMipMaps;
        std::size_t numChannels;
        std::size_t imageDataSize;
        std::size_t rowPitchInBytes;
        std::size_t hasTransparency;
    };

#pragma pack() 

}

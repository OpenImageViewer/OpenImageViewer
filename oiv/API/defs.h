
#pragma once
#include <cwchar>
#include <cstdint>


#ifdef __cplusplus
extern "C"
{
#endif

#ifdef UNICODE
    typedef wchar_t OIVCHAR;
#else
    typedef char OIVCHAR;
#endif



    // Naming convention.
    // Command identifier   - OIV_CMD_[CommandName].
    // Command request      - OIV_CMD_[CommandName]_Request.
    // Command response     - OIV_CMD_[CommandName]_Response.



    typedef int16_t ImageHandle;
    const ImageHandle ImageNullHandle = -1;
    const ImageHandle ImageHandleDisplayed = -2;



    enum CommandExecute
    {
          CE_NoOperation
        , CE_Init
        , CE_Destory
        , OIV_CMD_LoadFile
        , OIV_CMD_LoadRaw
        , OIV_CMD_UnloadFile
        , OIV_CMD_DisplayImage
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
        , OIV_CMD_ZoomScrollState
        , OIV_CMD_SetSelectionRect
        , OIV_CMD_GetPixelBuffer
        , OIV_CMD_WindowToimage
        , OIV_CMD_CropImage
        , OIV_CMD_GetPixels
        , OIV_CMD_ConvertFormat
        , OIV_CMD_ColorExposure
        , OIV_CMD_TexelInfo
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
        , RC_RenderError
        , RC_InvalidImageHandle
        , RC_InvalidHandle
        , RC_BadConversion
        , RC_ImageNotFound
        , RC_UknownError = 0xFF
        , RC_InternalError = 0xFF + 1,
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

    enum OIV_TexelFormat : uint16_t
    {
          TF_BEGIN
        , TF_UNKNOWN = TF_BEGIN
        , TF_I_R8_G8_B8
        , TF_I_R8_G8_B8_A8
        , TF_I_B8_G8_R8
        , TF_I_B8_G8_R8_A8
        , TF_I_A8_R8_G8_B8
        , TF_I_A8_B8_G8_R8
        , TF_I_X1
        , TF_I_X8
        , TF_F_X16
        , TF_F_X24
        , TF_F_X32
        , TF_F_X64
        , TF_COUNT
    };

#pragma pack(1)

    struct CmdNull
    {

    };

    struct OIV_RECT_I
    {
        int32_t x0;
        int32_t y0;
        int32_t x1;
        int32_t y1;
    };

    struct OIV_RECT_F
    {
        double x0;
        double y0;
        double x1;
        double y1;
    };

    struct OIV_CMD_TexelInfo_Request
    {
        ImageHandle handle;
        int32_t x;
        int32_t y;
    };


    struct OIV_CMD_TexelInfo_Response
    {
        OIV_TexelFormat type;
        uint8_t size;
        uint8_t buffer[32];
    };

    struct OIV_CMD_ConvertFormat_Request
    {
        ImageHandle handle;
        OIV_TexelFormat format;
    };

    struct OIV_CMD_GetPixels_Request
    {
        ImageHandle handle;
    };

    struct OIV_CMD_GetPixels_Response
    {
        const uint8_t* pixelBuffer;
        uint32_t width;
        uint32_t height;
        uint32_t rowPitch;
        OIV_TexelFormat texelFormat;
    };
    

    struct OIV_CMD_WindowToImage_Request
    {
        OIV_RECT_I rect;
    };

    struct OIV_CMD_WindowToImage_Response
    {
        OIV_RECT_F rect;
    };


    struct OIV_CMD_ColorExposure_Request
    {
        double exposure;
        double offset;
        double gamma;
    };

    struct OIV_CMD_CropImage_Request
    {
        OIV_RECT_I rect;
        ImageHandle imageHandle;
    };

    struct OIV_CMD_CropImage_Response
    {
        ImageHandle imageHandle;
    };

    struct OIV_CMD_SetSelectionRect_Request
    {
        OIV_RECT_I rect;
    };


    struct OIV_CMD_LoadRaw_Request
    {
        uint32_t width;
        uint32_t height;
        OIV_TexelFormat texelFormat;
        uint8_t* buffer;
        OIV_AxisAlignedRTransform transformation;
    };


    struct OIV_CMD_LoadRaw_Response
    {
        double loadTime;
        ImageHandle handle;
    };


    enum OIV_PROP_Scale_Mode
    {
          SM_None = 0 << 0
        , SM_FitToWindow = 1 << 0
        , SM_OriginalSize = 1 << 1
        , SM_LockScale = 1 << 2
        , SM_KeepAspectRatio = 1 << 3
    };


    struct OIV_CMD_ZoomScrollState_Request
    {
        double innerMarginsX;
        double innerMarginsY;
        double outermarginsX;
        double outermarginsY;
        uint8_t SmallImageOffsetStyle;
        OIV_PROP_Scale_Mode scaleMode;
    };


    struct OIV_CMD_AxisAlignedTransform_Request
    {
        OIV_AxisAlignedRTransform transform;
        ImageHandle handle;
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
    };

    

    struct CmdDataPan
    {
        double x;
        double y;
    };

    
    enum OIV_CMD_LoadFile_Flags
    {
          None = 0 << 1
        , OnlyRegisteredExtension = 1 << 0
        , Load_Exif_Data = 1 << 1
    };

    struct OIV_CMD_LoadFile_Request
    {
        static const uint8_t EXTENSION_SIZE = 16;
        static const uint8_t MAX_SUBIMAGE_HIERARCHY = 10;
        void* buffer;
        std::size_t length;
        char extension[EXTENSION_SIZE];
        OIV_CMD_LoadFile_Flags flags;
        int16_t subImageIndices[MAX_SUBIMAGE_HIERARCHY];
        
    };



    struct OIV_CMD_LoadFile_Response
    {
        double loadTime;
        uint32_t width;
        uint32_t height;
        uint8_t bpp;
        uint32_t sizeInMemory;
        ImageHandle handle;
    };

    struct OIV_CMD_UnloadFile_Request
    {
        ImageHandle handle;
    };

    ////
    enum OIV_CMD_DisplayImage_Flags
    {
          DF_None                       = 0 << 0
        , DF_ApplyExifTransformation    = 1 << 0
        , DF_ResetScrollState           = 1 << 1
        , DF_RefreshRenderer            = 1 << 2
    };

    enum OIV_PROP_Normalize_Mode
    {
          NM_Default    = 0
        , NM_Monochrome = 1
        , NM_Rainbow    = 2
    };

    struct OIV_CMD_DisplayImage_Request
    {
        ImageHandle handle;
        OIV_CMD_DisplayImage_Flags displayFlags;
        OIV_PROP_Normalize_Mode normalizeMode;
    };

    /*struct OIV_CMD_DisplayImage_Response
    {
        double loadTime;
        uint32_t width;
        uint32_t height;
        uint8_t bpp;
        uint32_t sizeInMemory;
        ImageHandle handle;
    }; */
    

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

#ifdef __cplusplus
}
#endif


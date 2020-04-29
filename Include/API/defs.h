
#pragma once
#include <cstdint>
#include <Point.h>
#include <Color.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef wchar_t OIVCHAR;
#define OIV_TEXT(T) (L ## T)


    // Naming convention.
    // Command identifier   - OIV_CMD_[CommandName].
    // Command request      - OIV_CMD_[CommandName]_Request.
    // Command response     - OIV_CMD_[CommandName]_Response.



    typedef int16_t ImageHandle;
    const ImageHandle ImageHandleNull = 0;


    enum CommandExecute
    {
          CE_NoOperation
        , CE_Init
        , CE_Destory
        , OIV_CMD_LoadFile
        , OIV_CMD_LoadRaw
        , OIV_CMD_UnloadFile
        , OIV_CMD_ImageProperties
        , CE_Refresh
        , OIV_CMD_QueryImageInfo
        , CE_TexelGrid
        , CMD_SetClientSize
        , OIV_CMD_Destroy
        , OIV_CMD_AxisAlignedTransform
        , OIV_CMD_SetSelectionRect
        , OIV_CMD_CropImage
        , OIV_CMD_GetPixels
        , OIV_CMD_ConvertFormat
        , OIV_CMD_ColorExposure
        , OIV_CMD_TexelInfo
        , OIV_CMD_CreateText
        , OIV_CMD_GetKnownFileTypes
        , OIV_CMD_RegisterCallbacks
        , OIV_CMD_GetSubImages
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
        , RC_NotImplemented
        , RC_UknownError = 0xFF
        , RC_InternalError = 0xFF + 1,

    };

    //-------Command Structs-------------------------
    enum OIV_AxisAlignedRotation
    {
          AAT_None = 0
        , AAT_Rotate90CW 
        , AAT_Rotate180
        , AAT_Rotate90CCW
    };

    //flags.
    enum OIV_AxisAlignedFlip
    {
          AAF_None = 0 << 0
        , AAF_Horizontal = 1 << 0
        , AAF_Vertical = 1 << 1
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
        , TF_I_A8
        , TF_I_X1
        , TF_I_X8
        , TF_F_X16
        , TF_F_X24
        , TF_F_X32
        , TF_F_X64
        , TF_COUNT
    };

#pragma pack(16)

    struct CmdNull
    {

    };

    struct OIV_RendererInitializationParams
    {
        size_t container;
        const OIVCHAR* dataPath;
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
        uint32_t x;
        uint32_t y;
    };

    struct OIV_Exception_Args
    {
        int errorCode;
        const wchar_t* description;
        const wchar_t* systemErrorMessage;
        const wchar_t* callstack;
        const wchar_t* functionName;
    };

    struct OIV_CMD_RegisterCallbacks_Request
    {
        void(*OnException) (OIV_Exception_Args, void*);
		void* userPointer;
    };


    struct OIV_CMD_TexelInfo_Response
    {
        OIV_TexelFormat type;
        uint8_t size;
        uint8_t buffer[32];
    };

    enum OIV_ConvertFormat_Flags
    {
          OIV_CF_None
        , OIV_CF_RAINBOW_NORMALIZE
    };

    struct OIV_CMD_ConvertFormat_Request
    {
        ImageHandle handle;
        OIV_ConvertFormat_Flags flags;
        OIV_TexelFormat format;
    };

    struct OIV_CMD_ConvertFormat_Response
    {
        ImageHandle handle;
    };


    struct OIV_CMD_GetPixels_Request
    {
        ImageHandle handle;
    };

    struct OIV_CMD_GetPixels_Response
    {
        const std::byte* pixelBuffer;
        uint32_t width;
        uint32_t height;
        uint32_t rowPitch;
        OIV_TexelFormat texelFormat;
    };
    


    struct OIV_CMD_ColorExposure_Request
    {
        double exposure;
        double offset;
        double gamma;
        double saturation;
        double contrast;
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


    enum OIV_PROP_CreateText_Mode
    {
          CTM_None = 0 << 0
        , CTM_AntiAliased = 1 << 0
        , CTM_SubpixelAntiAliased = 1 << 1
    };

    struct OIV_CMD_CreateText_Request
    {
        const OIVCHAR* text;
        const OIVCHAR* fontPath;
        uint16_t fontSize;
        uint32_t backgroundColor;
        uint8_t outlineWidth;
        uint8_t outlineColor;
        uint16_t DPIx;
        uint16_t DPIy;
        OIV_PROP_CreateText_Mode renderMode;
    };

    

    struct OIV_CMD_CreateText_Response
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
        uint32_t rowPitch;
        OIV_TexelFormat texelFormat;
        std::byte* buffer;
        OIV_AxisAlignedFlip transformation;
    };


    struct OIV_CMD_LoadRaw_Response
    {
        double loadTime;
        ImageHandle handle;
    };


    /*enum OIV_PROP_Scale_Mode
    {
          SM_None = 0 << 0
        , SM_FitToWindow = 1 << 0
        , SM_OriginalSize = 1 << 1
        , SM_LockScale = 1 << 2
        , SM_KeepAspectRatio = 1 << 3
    };*/


    enum OIV_PROP_TransparencyMode
    {
          TM_Light 
        , TM_Medium
        , TM_Dark
        , TM_Darker
        , TM_Count
    };



    struct OIV_CMD_GetKnownFileTypes_Response
    {
        size_t bufferSize;
        char*  knownFileTypes;
    };

    struct CmdSetClientSizeRequest
    {
        uint16_t width;
        uint16_t height;
    };

    struct OIV_Tranform
    {
        OIV_AxisAlignedRotation rotation;
        OIV_AxisAlignedFlip flip;
    };


    struct OIV_CMD_AxisAlignedTransform_Request
    {
        OIV_Tranform transform;
        ImageHandle handle;
    };

    struct OIV_CMD_AxisAlignedTransform_Response
    {
        ImageHandle handle;
    };

    
    struct CmdRequestTexelGrid
    {
        double gridSize;
        OIV_PROP_TransparencyMode transparencyMode;
        bool generateMipmaps;
    };

    enum OIV_Filter_type
    {
          FT_None
        , FT_Linear
        , FT_Lanczos3
        , FT_Count
    };

    /*struct OIV_CMD_Filter_Request
    {
        OIV_Filter_type filterType;
    };*/

    /*struct CmdDataZoom
    {
        double amount;
    };*/

    

  /*  struct CmdDataPan
    {
        double x;
        double y;
    };*/

    
    enum OIV_CMD_LoadFile_Flags
    {
          None = 0 << 1
        , OnlyRegisteredExtension = 1 << 0
        , Load_Exif_Data = 1 << 1
        , Load_Sub_Images = 1 << 2
        , Load_Main_Image = 1 << 3
    };

    struct OIV_CMD_LoadFile_Request
    {
        static constexpr uint8_t MAX_EXTENSION_SIZE = 16;
        void* buffer;
        std::size_t length;
        char extension[MAX_EXTENSION_SIZE];
        OIV_CMD_LoadFile_Flags flags;
        
    };


    struct OIV_CMD_LoadFile_Response
    {
        double loadTime;
        uint32_t width;
        uint32_t height;
        uint8_t bpp;
        uint32_t sizeInMemory;
        ImageHandle handle;
        uint32_t numSubImages;

        //user pointer for subimages
        OIV_CMD_LoadFile_Response* subImages;

    };

    enum OIV_Image_Render_mode
    {
          IRM_Default
        , IRM_MainImage
        , IRM_Overlay
    };


    struct OIV_CMD_ImageProperties_Request
    {
        ImageHandle imageHandle;
        LLUtils::PointF64 position;
        LLUtils::PointF64 scale;
        OIV_Image_Render_mode imageRenderMode;
        double opacity;
        OIV_Filter_type filterType;
    };



    struct OIV_CMD_UnloadFile_Request
    {
        ImageHandle handle;
    };


    struct OIV_CMD_GetSubImages_Request
    {
        ImageHandle handle;
        ImageHandle *childrenArray;
        uint32_t arraySize;
    };

    struct OIV_CMD_GetSubImages_Response
    {
        uint32_t copiedElements;
    };

    ////
    enum OIV_CMD_DisplayImage_Flags
    {
          DF_None                       = 0 << 0
        , DF_ApplyExifTransformation    = 1 << 0
        , DF_RefreshRenderer            = 1 << 1
        , DF_AutoRasterize              = 1 << 2 // don't rasterize to a compabile image
        , DF_Hide                       = 1 << 3
        , DF_UseFixedDisplayHandle      = 1 << 4 

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

    struct OIV_CMD_QueryImageInfo_Request
    {
        ImageHandle handle;
    };


    struct OIV_CMD_QueryImageInfo_Response
    {
        uint32_t width;
        uint32_t height;
        uint32_t rowPitchInBytes;
        uint32_t bitsPerPixel;
        uint32_t NumSubImages;
        OIV_TexelFormat texelFormat;
    };

#pragma pack() 

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
using OIVString = std::basic_string<OIVCHAR>;
using OIVStringStream = std::basic_stringstream<OIVCHAR>;
#endif




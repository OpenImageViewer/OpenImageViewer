#pragma warning(disable : 4275 ) // disables warning 4275, pop and push from exceptions
#pragma warning(disable : 4251 ) // disables warning 4251, the annoying warning which isn't needed here...
#include "oiv.h"
#include "External\easyexif\exif.h"
#include "Interfaces\IRenderer.h"
#include "NullRenderer.h"
#include <ImageLoader.h>
#include <ImageUtil.h>
#include "Configuration.h"


#if OIV_BUILD_RENDERER_D3D11 == 1
#include "../OIVD3D11Renderer/Include/OIVD3D11RendererFactory.h"
#endif


#if OIV_BUILD_RENDERER_GL == 1
#include "../OIVGLRenderer/OIVGLRendererFactory.h"
#endif



namespace OIV
{

    #pragma region ZoomScrollStateListener
    LLUtils::PointI32 OIV::GetImageSize()
    {
        using namespace LLUtils;
        return GetDisplayImage() ? PointI32(
                static_cast<PointI32::point_type>(GetDisplayImage()->GetWidth()),
                static_cast<PointI32::point_type>(GetDisplayImage()->GetHeight()))
            : PointI32::Zero;
    }

    LLUtils::PointI32 OIV::GetClientSize()
    {
        return fClientSize;
    }
    
    void OIV::NotifyDirty()
    {
        const bool AutoRefreshWhenDirty = false;
        fIsViewDirty = true;
        if (AutoRefreshWhenDirty == true)
            Refresh();
    }

    
#pragma endregion 
   
 
    void OIV::RefreshRenderer()
    {
        UpdateGpuParams();
        fRenderer->Redraw();
    }

    IMCodec::ImageSharedPtr OIV::GetDisplayImage() const
    {
        return fDisplayedImage;
    }

    void OIV::UpdateGpuParams()
    {
        LLUtils::PointF64 uvScaleFixed = fScrollState.GetARFixedUVScale();
        LLUtils::PointF64 uvOffset = fScrollState.GetOffset();
        
        if (GetDisplayImage() == nullptr)
        {
            // Place image out of the window space.
            uvOffset = LLUtils::PointF64::One;
            uvScaleFixed = LLUtils::PointF64::One;
        }
        
        fViewParams.showGrid = fShowGrid;
        fViewParams.uViewportSize = GetClientSize();
        fViewParams.uvOffset = uvOffset;
        fViewParams.uvscale = uvScaleFixed;
        
        fRenderer->SetViewParams(fViewParams);
    }

    bool OIV::IsImageDisplayed() const
    {
        return GetDisplayImage() != nullptr;
    }

    OIV_AxisAlignedRTransform OIV::ResolveExifRotation(unsigned short exifRotation) const
    {
        OIV_AxisAlignedRTransform rotation;
            switch (exifRotation)
            {
            case 3:
                rotation = AAT_Rotate180;
                break;
            case 6:
                rotation = AAT_Rotate90CW;
                break;
            case 8:
                rotation = AAT_Rotate90CCW;
                break;
            default:
                rotation = AAT_None;
            }
            return rotation;
    }

    IRendererSharedPtr OIV::CreateBestRenderer()
    {

#ifdef _MSC_VER 
     // Prefer Direct3D11 for windows.
    #if OIV_BUILD_RENDERER_D3D11 == 1
            return D3D11RendererFactory::Create();
    // Prefer Direct3D11 for windows.
    #elif  OIV_BUILD_RENDERER_GL == 1
        return GLRendererFactory::Create();
    #elif OIV_ALLOW_NULL_RENDERER == 1
        return IRendererSharedPtr(new NullRenderer());
    #else
        #error No valid renderers detected.
    #endif

#else // _MSC_VER
// If no windows choose GL renderer
    #if OIV_BUILD_RENDERER_GL == 1
        return GLRendererFactory::Create();
    #elif OIV_ALLOW_NULL_RENDERER == 1
        return IRendererSharedPtr(new NullRenderer());
    #else
        #error No valid renderers detected.
    #endif
#endif

        throw std::runtime_error("wrong build configuration");

    }


#pragma region IPictureViewer implementation
    // IPictureViewr implementation
    ResultCode OIV::LoadFile(void* buffer, std::size_t size, char* extension, OIV_CMD_LoadFile_Flags flags, ImageHandle& handle)
    {

        if (buffer == nullptr || size == 0)
            return RC_InvalidParameters;

        using namespace IMCodec;
        ImageSharedPtr image = ImageSharedPtr(fImageLoader.Load(static_cast<uint8_t*>(buffer), size, extension, (flags & OIV_CMD_LoadFile_Flags::OnlyRegisteredExtension) != 0));

        if (image != nullptr)
        {
            if (flags & OIV_CMD_LoadFile_Flags::Load_Exif_Data)
            {
                easyexif::EXIFInfo exifInfo;
                if (exifInfo.parseFrom(static_cast<const unsigned char*>(buffer), static_cast<unsigned int>(size)) == PARSE_EXIF_SUCCESS)
                {
                    const_cast<ImageData&>(image->GetData()).exifOrientation = exifInfo.Orientation;
                }
            }
                
            handle = fImageManager.AddImage(image);

            return RC_Success;
        }
        else
        {
            return RC_FileNotSupported;
        }
    }

    ResultCode OIV::LoadRaw(const OIV_CMD_LoadRaw_Request& loadRawRequest, int16_t& handle) 
    {
        using namespace IMCodec;
        
        ImageData data;
        data.LoadTime = 0;
        ImageProperies props;
        props.Height = loadRawRequest.height;
        props.Width = loadRawRequest.width;
        props.NumSubImages = 0;
        props.TexelFormatStorage = static_cast<IMCodec::TexelFormat>(loadRawRequest.texelFormat);
        props.TexelFormatDecompressed = static_cast<IMCodec::TexelFormat>(loadRawRequest.texelFormat);
        props.RowPitchInBytes = loadRawRequest.width * IMCodec::GetTexelFormatSize(props.TexelFormatDecompressed) / 8;

        const std::size_t buferSize = props.RowPitchInBytes * props.Height;
        props.ImageBuffer = new uint8_t[buferSize];


        memcpy(props.ImageBuffer, loadRawRequest.buffer, buferSize);

        ImageSharedPtr image = ImageSharedPtr(new Image(props, data));
        image = IMUtil::ImageUtil::Transform(
            static_cast<IMUtil::AxisAlignedRTransform>(loadRawRequest.transformation), image);

        handle = fImageManager.AddImage(image);
        return RC_Success;

        
    }

    IMCodec::ImageSharedPtr OIV::ApplyExifRotation(IMCodec::ImageSharedPtr image) const
    {
        return IMUtil::ImageUtil::Transform(
            static_cast<IMUtil::AxisAlignedRTransform>(ResolveExifRotation(image->GetData().exifOrientation))
            , image);
    }

    ResultCode OIV::DisplayFile(const OIV_CMD_DisplayImage_Request& display_request)
    {
        ResultCode result = RC_Success;

        if (display_request.handle == ImageNullHandle)
        {
            fDisplayedImage.reset();
            return result;
        }

        IMCodec::ImageSharedPtr image = fImageManager.GetImage(display_request.handle);
        if (image != nullptr)
        {
            OIV_CMD_DisplayImage_Flags display_flags = display_request.displayFlags;

            const bool applyExif = (display_flags & OIV_CMD_DisplayImage_Flags::DF_ApplyExifTransformation) != 0;
            if (applyExif)
                image = ApplyExifRotation(image);
            

            // Texel format supported by the renderer is currently RGBA.
            // support for other texel formats may save conversion.
            const IMCodec::TexelFormat targetTexelFormat = IMCodec::TexelFormat::TF_I_R8_G8_B8_A8;

            switch (image->GetImageType())
            {
            case IMCodec::TF_F_X16:
                image = IMUtil::ImageUtil::Normalize<half_float::half>(image, targetTexelFormat);
                break;

            case IMCodec::TF_F_X24:
                throw std::logic_error("not implemented");
                image = IMUtil::ImageUtil::Normalize<half_float::half>(image, targetTexelFormat);
                break;

            case IMCodec::TF_F_X32:
                image = IMUtil::ImageUtil::Normalize<float>(image, targetTexelFormat,static_cast<IMUtil::ImageUtil::NormalizeMode>(display_request.normalizeMode));
                break;
            case IMCodec::TF_I_X8:
                image = IMUtil::ImageUtil::Normalize<int8_t>(image, targetTexelFormat, static_cast<IMUtil::ImageUtil::NormalizeMode>(display_request.normalizeMode));
                break;

            default:
                image = IMUtil::ImageUtil::Convert(image, targetTexelFormat);
            }


            if (image != nullptr)
            {
                if (fRenderer->SetImage(image) == RC_Success)
                {
                    fDisplayedImage = image;

                    const bool resetScrollState = (display_flags & OIV_CMD_DisplayImage_Flags::DF_ResetScrollState) != 0;
                    const bool refreshRenderer = (display_flags & OIV_CMD_DisplayImage_Flags::DF_RefreshRenderer) != 0;

                    //if (resetScrollState)
                        //fScrollState.Reset(resetScrollState);

                    if (refreshRenderer)
                        RefreshRenderer();
                }
                else
                {
                    result = RC_RenderError;
                }
            }
            else
            {
                result = RC_PixelFormatConversionFailed;
            }
        }
        else
        {
            result = RC_InvalidImageHandle;
        }
     
        return result;
    }

    ResultCode OIV::SetSelectionRect(const OIV_CMD_SetSelectionRect_Request& selectionRect)
    {
        fRenderer->SetSelectionRect({ { selectionRect.rect.x0 ,selectionRect.rect.y0 },{ selectionRect.rect.x1 ,selectionRect.rect.y1 } });
        return RC_Success;
    }

    ResultCode OIV::WindowToImage(const OIV_CMD_WindowToImage_Request& request, OIV_CMD_WindowToImage_Response& response)
    {
        if (fDisplayedImage != nullptr)
        {
            LLUtils::PointF64 p0 = fScrollState.ClientPosToTexel({ request.rect.x0, request.rect.y0 });
            LLUtils::PointF64 p1 = fScrollState.ClientPosToTexel({ request.rect.x1, request.rect.y1 });

            response.rect.x0 = p0.x;
            response.rect.y0 = p0.y;
            response.rect.x1 = p1.x;
            response.rect.y1 = p1.y;
            return RC_Success;
        }

        return RC_InvalidParameters;
        
    }

    ResultCode OIV::ConverFormat(const OIV_CMD_ConvertFormat_Request& req)
    {
        using namespace IMCodec;
        ResultCode result = RC_Success;
        if (req.handle > 0 )
        {
            ImageSharedPtr original = fImageManager.GetImage(req.handle);
            if (original != nullptr)
            {
                ImageSharedPtr converted = IMUtil::ImageUtil::Convert(original, static_cast<TexelFormat>(req.format));
                if (converted != nullptr)
                    fImageManager.ReplaceImage(req.handle, converted);
                else
                    result = ResultCode::RC_BadConversion;
            }
            else
                result = ResultCode::RC_ImageNotFound;
            
        }
        return result;
    }

    ResultCode OIV::GetPixels(const OIV_CMD_GetPixels_Request & req,  OIV_CMD_GetPixels_Response & res)
    {
        IMCodec::ImageSharedPtr image = fImageManager.GetImage(req.handle);

        if (image != nullptr)
        {
            res.width = image->GetWidth();
            res.height = image->GetHeight();
            res.rowPitch = image->GetRowPitchInBytes();
            res.texelFormat = static_cast<OIV_TexelFormat>( image->GetImageType());
            res.pixelBuffer = image->GetConstBuffer();
            return RC_Success;
        }

        return RC_InvalidHandle;
        
    }

    ResultCode OIV::CropImage(const OIV_CMD_CropImage_Request& request, OIV_CMD_CropImage_Response& response)
    {
        ResultCode result = RC_Success;
        IMCodec::ImageSharedPtr imageToCrop;
        if (request.imageHandle == ImageHandleDisplayed)
        {
            if (fDisplayedImage != nullptr)
                imageToCrop = fDisplayedImage;
        }
        else
        {
            imageToCrop = fImageManager.GetImage(request.imageHandle);
        }

        if (imageToCrop == nullptr)
        {
            result = RC_ImageNotFound;
        }
        else
        {
            LLUtils::RectI32 imageRect = { { 0,0 } ,{ static_cast<int32_t> (imageToCrop->GetWidth())
                , static_cast<int32_t> (imageToCrop->GetHeight()) } };

            LLUtils::RectI32 subImageRect = { { request.rect.x0,request.rect.y0 },{ request.rect.x1,request.rect.y1 } };
            LLUtils::RectI32 cuttedRect = subImageRect.Intersection(imageRect);
            IMCodec::ImageSharedPtr subImage =
                IMUtil::ImageUtil::GetSubImage(fDisplayedImage, cuttedRect);

            if (subImage != nullptr)
            {
                ImageHandle handle = fImageManager.AddImage(subImage);
                response.imageHandle = handle;
                result = RC_Success;
            }
            else
            {
                result = RC_UknownError;
            }
        }
        return result;
    }

    ResultCode OIV::SetColorExposure(const OIV_CMD_ColorExposure_Request& exposure)
    {
        fRenderer->SetExposure(exposure);
        return RC_Success;
    }

    ResultCode OIV::Zoom(double zoom)
    {
        fScrollState.SetZoom(zoom);
        return ResultCode::RC_Success;
    }

    ResultCode OIV::Pan(double x, double y)
    {
        fScrollState.SetOffset({ x, y });
        return ResultCode::RC_Success;
    }

    ResultCode OIV::UnloadFile(const ImageHandle handle)
    {
        return fImageManager.RemoveImage(handle) == true ? RC_Success : RC_FileNotFound;
    }

    int OIV::Init()
    {
        fRenderer = CreateBestRenderer();
        fRenderer->Init(fParent);
        return 0;
    }


    int OIV::SetParent(std::size_t handle)
    {
        fParent = handle;
        return 0;
    }
    int OIV::Refresh()
    {
        if (fIsRefresing == false)
        {
            fIsRefresing = true;
            fScrollState.Refresh();
            RefreshRenderer();
            fIsRefresing = false;
        }

        return 0;
    }

    IMCodec::Image* OIV::GetImage(ImageHandle handle)
    {
        return fImageManager.GetImage(handle).get();
    }

    int OIV::SetFilterLevel(OIV_Filter_type filter_level)
    {
        if (filter_level >= FT_None && filter_level < FT_Count)
        {
            fRenderer->SetFilterLevel(filter_level);
            return RC_Success;
        }

        return RC_WrongParameters;
    }

    int OIV::GetFileInformation(QryFileInformation& information)
    {

        //TODO: restore implementation and add image handle
        //if (IsImageLoaded())
        {

          /*  information.bitsPerPixel = GetOpenedImage()->GetBitsPerTexel();
            information.height = GetOpenedImage()->GetHeight();
            information.width = GetOpenedImage()->GetWidth();
            information.numMipMaps = 0;
            information.rowPitchInBytes = GetOpenedImage()->GetRowPitchInBytes();
            information.hasTransparency = 1;
            information.imageDataSize = 0;
            information.numChannels = 0;
*/
            return RC_Success;
        }
  /*      else
        {
            return 1;
        }*/
    }

    int OIV::GetTexelAtMousePos(int mouseX, int mouseY, double& texelX, double& texelY)
    {
        LLUtils::PointF64 texelPos = this->fScrollState.ClientPosToTexel({ mouseX, mouseY });
        texelX = texelPos.x;
        texelY = texelPos.y;
        return RC_Success;
    }

    int OIV::SetTexelGrid(double gridSize)
    {
        fShowGrid = gridSize > 0.0;
        return RC_Success;
    }

    int OIV::GetNumTexelsInCanvas(double &x, double &y)
    {
        LLUtils::PointF64 canvasSize = fScrollState.GetNumTexelsInCanvas();
        x = canvasSize.x;
        y = canvasSize.y;
        return RC_Success;
    }

    int OIV::SetClientSize(uint16_t width, uint16_t height)
    {
        fClientSize = { width, height };
        return 0;
    }

    ResultCode OIV::AxisAlignTrasnform(const OIV_CMD_AxisAlignedTransform_Request& request)
    {
        
        if (request.handle == ImageHandleDisplayed && GetDisplayImage() != nullptr)
        {
            IMCodec::ImageSharedPtr& image = fDisplayedImage;
            
            image = IMUtil::ImageUtil::Transform(static_cast<IMUtil::AxisAlignedRTransform>(request.transform), image);
            if (image != nullptr && fRenderer->SetImage(image) == RC_Success)
            {
//                fScrollState.Reset(true);
                return RC_Success;
            }
        }
        else
        {
            IMCodec::ImageSharedPtr image = fImageManager.GetImage(request.handle);
            if (image != nullptr)
            {
                image = IMUtil::ImageUtil::Transform(static_cast<IMUtil::AxisAlignedRTransform>(request.transform), image);
                fImageManager.ReplaceImage(request.handle, image);
            }
        }
        return RC_UknownError;
    }
    ResultCode OIV::SetZoomScrollState(const OIV_CMD_ZoomScrollState_Request * zoom_scroll_state)
    {

        return ResultCode::RC_Success;
        
    }
#pragma endregion

}

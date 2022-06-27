#include "oiv.h"
#include <exif.h>
#include "Interfaces/IRenderer.h"
#include "NullRenderer.h"
#include <ImageLoader.h>
#include "ImageUtil.h"
#include "Configuration.h"
#include "FreeTypeHelper.h"
#include <functions.h>
#include <Version.h>
#include "Interfaces/IRendererDefs.h"


#if OIV_BUILD_RENDERER_D3D11 == 1
#include <OIVD3D11RendererFactory.h>
#endif


#if OIV_BUILD_RENDERER_GL == 1
//#include <OIVGLRendererFactory.h>
#endif

namespace OIV
{
    IRenderer* OIV::GetRenderer() 
    {
        return fRenderer.get();
    }

    LLUtils::PointI32 OIV::GetClientSize() const
    {
        return fClientSize;
    }
  
    void OIV::RefreshRenderer()
    {
        UpdateGpuParams();
        fRenderer->Redraw();
    }

    void OIV::UpdateGpuParams()
    {
        fViewParams.showGrid = fShowGrid;
        fViewParams.uTransparencyColor1 = transparencyCheckerShades[static_cast<int>(fTransparencyShade)].color1;
        fViewParams.uTransparencyColor2 = transparencyCheckerShades[static_cast<int>(fTransparencyShade)].color2;
        fViewParams.uViewportSize = GetClientSize();
        fRenderer->SetViewParams(fViewParams);
    }

    OIV_AxisAlignedRotation OIV::ResolveExifRotation(unsigned short exifRotation) const
    {
        OIV_AxisAlignedRotation rotation;
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

// use Direct3D11 for window and opengl for every other platfotm.
#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32
    #if OIV_BUILD_RENDERER_D3D11 == 1
    // Prefer Direct3D11 for windows.
        return D3D11RendererFactory::Create();
    #elif  OIV_BUILD_RENDERER_GL == 1
        return GLRendererFactory::Create();
    #elif OIV_ALLOW_NULL_RENDERER == 1
        return IRendererSharedPtr(new NullRenderer());
    #else
        #error No valid renderers detected.
    #endif
#else
// If no windows choose GL renderer
    #if OIV_BUILD_RENDERER_GL == 1
        return GLRendererFactory::Create();
    #elif OIV_ALLOW_NULL_RENDERER == 1
        return IRendererSharedPtr(new NullRenderer());
    #else
        #error No valid renderers detected.
    #endif
#endif

        LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters, "Bad build configuration");
    }

    IMCodec::ImageSharedPtr OIV::Resample(IMCodec::ImageSharedPtr sourceImage, LLUtils::PointI32 targetSize)
    {

        const uint32_t width = targetSize.x;
        const uint32_t height = targetSize.y;


        using namespace IMCodec;
        //Create target downscaled image.
        ImageItemSharedPtr imageItem = std::make_shared<ImageItem>();
        ImageDescriptor& desc = imageItem->descriptor;
        desc.height = height;
        desc.width = width;
        desc.rowPitchInBytes = width * sourceImage->GetBytesPerTexel();
        desc.texelFormatDecompressed = sourceImage->GetTexelFormat();
        desc.texelFormatStorage = sourceImage->GetOriginalTexelFormat();
        imageItem->data.Allocate(width * height * sourceImage->GetBytesPerTexel());
        
        ImageSharedPtr resampled = std::make_shared<IMCodec::Image>(imageItem,ImageItemType::Unknown);
        ResamplerParams params;
        params.sourceBuffer = reinterpret_cast<const uint32_t*>(sourceImage->GetBufferAt(0, 0));
        params.sourceWidth = sourceImage->GetWidth();
        params.sourceHeight = sourceImage->GetHeight();
        params.targetBuffer = const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(resampled->GetBufferAt(0, 0)));
        params.targetWidth = width;
        params.targetHeight = height;

        fResampler.Resample(params);

        return resampled;
    }


#pragma region IPictureViewer implementation
    // IPictureViewr implementation
    ResultCode OIV::LoadFile(void* buffer, std::size_t size, char* extension, OIV_CMD_LoadFile_Flags flags, ImageHandle& handle)
    {

        return ResultCode::RC_NotImplemented;

  //      if (buffer == nullptr || size == 0)
  //          return RC_InvalidParameters;

		//ResultCode result = RC_FileNotSupported;
  //      using namespace IMCodec;
  //      ImageSharedPtr image;
  //      ImageLoaderFlags loadFlags = ((flags & OIV_CMD_LoadFile_Flags::OnlyRegisteredExtension) != 0) ? ImageLoaderFlags::OnlyRegisteredExtension : ImageLoaderFlags::None;
  //      ImageResult loadResult = fImageLoader.Load(static_cast<const std::byte*>(buffer), size, extension,ImageLoadFlags::None, loadFlags, image);

		//if (loadResult ==  ImageResult::Success)
		//{
		//	if (image != nullptr)
		//	{
		//		int exifOrientation = 0;
		//		easyexif::EXIFInfo exifInfo;
		//		if (flags & OIV_CMD_LoadFile_Flags::Load_Exif_Data
		//			&& exifInfo.parseFrom(static_cast<const unsigned char*>(buffer), static_cast<unsigned int>(size)) == PARSE_EXIF_SUCCESS)
		//			exifOrientation = exifInfo.Orientation;


		//		if (exifOrientation != 0)
		//		{
  //                  
		//			const_cast<ItemMetaData&>(image->GetMetaData()).exifData.orientation = exifOrientation;

		//			// I see no use of using the original image, discard source image and use the image with exif rotation applied. 
		//			// If needed, responsibility for exif rotation can be transferred to the user by returning MetaData.exifOrientation.
		//			image = ApplyExifRotation(image);

		//		}
		//		handle = fImageManager.AddImage(image);

		//		for (size_t i = 0; i < image->GetNumSubImages(); i++)
		//		{

		//			if (exifOrientation != 0)
  //                      image->SetSubImage(i, ApplyExifRotation(image->GetSubImage(i)));

		//			fImageManager.AddChildImage(image->GetSubImage(i), handle);
		//		}
		//		result = RC_Success;
		//	}
		//}
		//return result;
    }

    ResultCode OIV::LoadRaw(const OIV_CMD_LoadRaw_Request& loadRawRequest, int16_t& handle) 
    {
        using namespace IMCodec;
        ImageItemSharedPtr imageItem = std::make_shared<ImageItem>();
        ImageDescriptor& props = imageItem->descriptor;
        props.height = loadRawRequest.height;
        props.width = loadRawRequest.width;
        props.texelFormatStorage = static_cast<IMCodec::TexelFormat>(loadRawRequest.texelFormat);
        props.texelFormatDecompressed = static_cast<IMCodec::TexelFormat>(loadRawRequest.texelFormat);
        props.rowPitchInBytes = loadRawRequest.rowPitch;
        const size_t bufferSize = props.rowPitchInBytes * props.height;
        imageItem->data.Allocate(bufferSize);
        imageItem->data.Write(loadRawRequest.buffer, 0, bufferSize);

        IMUtil::OIV_AxisAlignedTransform transform{};
        //transform.rotation = static_cast<IMUtil::OIV_AxisAlignedRotation>(loadRawRequest.transformation);
        transform.flip = static_cast<IMUtil::OIV_AxisAlignedFlip>(loadRawRequest.transformation);

        ImageSharedPtr image = std::make_shared<Image>(imageItem, ImageItemType::Unknown);
        image = IMUtil::ImageUtil::Transform(transform, image);

        handle = fImageManager.AddImage(image);
        return RC_Success;

        
    }

    IMCodec::ImageSharedPtr OIV::ApplyExifRotation(IMCodec::ImageSharedPtr image) const
    {
        IMUtil::OIV_AxisAlignedTransform transform;
        transform.rotation = static_cast<IMUtil::OIV_AxisAlignedRotation>(ResolveExifRotation(image->GetMetaData().exifData.orientation));
        transform.flip = IMUtil::OIV_AxisAlignedFlip::None;

        return IMUtil::ImageUtil::Transform(transform, image);
    }


  


    ResultCode OIV::CreateText(const OIV_CMD_CreateText_Request &request, OIV_CMD_CreateText_Response &response)
    {
        return ResultCode::RC_NotImplemented;
        
//    #if OIV_BUILD_FREETYPE == 1
//
//        OIVString text = request.text;
//        OIVString fontPath = request.fontPath;
//
//        //std::string u8Text = LLUtils::StringUtility::ToUTF8<OIVCHAR>(text);
//        //std::string u8FontPath = LLUtils::StringUtility::ToUTF8<OIVCHAR>(fontPath);
//        using namespace FreeType;
//        TextCreateParams createParams = {};
//        *reinterpret_cast<LLUtils::Color*>(&createParams.backgroundColor) = *reinterpret_cast<const LLUtils::Color*>(&request.backgroundColor);
//        createParams.fontPath = fontPath;
//        createParams.fontSize = request.fontSize;
//        createParams.outlineColor = { 0,0,0,255 };// request.outlineColor;
//        createParams.outlineWidth = request.outlineWidth;
//        createParams.text = text;
//        createParams.renderMode = static_cast<RenderMode>(request.renderMode);
//        createParams.DPIx = request.DPIx == 0 ? 96 : request.DPIx;
//        createParams.DPIy = request.DPIy == 0 ? 96 : request.DPIy;
//        createParams.flags = TextCreateFlags::Bidirectional | TextCreateFlags::UseMetaText;
//
//
//        IMCodec::ImageSharedPtr imageText = FreeTypeHelper::CreateRGBAText(createParams);
//
//        if (imageText != nullptr)
//        {
//            if (imageText->GetOriginalTexelFormat() != IMCodec::TexelFormat::I_B8_G8_R8_A8 && imageText->GetOriginalTexelFormat() != IMCodec::TexelFormat::I_R8_G8_B8_A8)
//                imageText = IMUtil::ImageUtil::Convert(imageText, IMCodec::TexelFormat::I_B8_G8_R8_A8);
//
//            ImageHandle handle = fImageManager.AddImage(imageText);
//            response.imageHandle = handle;
//            UploadImageToRenderer(handle);
//            return RC_Success;
//        }
//        else
//        {
//            return RC_InvalidParameters;
//        }
//
//
//#else
//        return RC_NotImplemented;
//#endif
    }

    ResultCode OIV::SetSelectionRect(const OIV_CMD_SetSelectionRect_Request& selectionRect)
    {
        fRenderer->SetSelectionRect({ { selectionRect.rect.x0 ,selectionRect.rect.y0 },{ selectionRect.rect.x1 ,selectionRect.rect.y1 } });
        return RC_Success;
    }

    ResultCode OIV::ConverFormat(const OIV_CMD_ConvertFormat_Request& req, OIV_CMD_ConvertFormat_Response& res)
    {
        using namespace IMCodec;
        ResultCode result = RC_Success;
        if (req.handle > 0 )
        {
            ImageSharedPtr original = fImageManager.GetImage(req.handle);
            if (original != nullptr)
            {
                bool rainbow = (req.flags & OIV_CF_RAINBOW_NORMALIZE) != 0;
                
                ImageSharedPtr converted = IMUtil::ImageUtil::ConvertImageWithNormalization(original, static_cast<TexelFormat>(req.format), rainbow);
                if (converted != nullptr)
                    res.handle = fImageManager.AddImage(converted);
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
            res.texelFormat = static_cast<OIV_TexelFormat>( image->GetTexelFormat());
            res.pixelBuffer = image->GetBuffer();
            return RC_Success;
        }

        return RC_InvalidHandle;
        
    }

    ResultCode OIV::SetBackgroundColor(int index, LLUtils::Color backgroundColor)
    {
        fRenderer->SetBackgroundColor(index, backgroundColor);

        return ResultCode::RC_Success;
    }

    ResultCode OIV::AddRenderable(IRenderable* renderable)
    {
        if (fRenderer != nullptr)
            fRenderer->AddRenderable(renderable);
        else
            fPendingRenderables.push_back(renderable);

        return ResultCode::RC_Success;
    }
    ResultCode OIV::RemoveRenderable(IRenderable* renderable)
    {
        if (fRenderer != nullptr)
            fRenderer->RemoveRenderable(renderable);
        else
            fPendingRenderables.erase(std::find(fPendingRenderables.begin(), fPendingRenderables.end(), renderable));

        return ResultCode::RC_Success;
    }

    ResultCode OIV::CropImage(const OIV_CMD_CropImage_Request& request, OIV_CMD_CropImage_Response& response)
    {
        ResultCode result = RC_Success;
        IMCodec::ImageSharedPtr imageToCrop = GetImage(request.imageHandle);
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
                IMUtil::ImageUtil::GetSubImage(imageToCrop, cuttedRect);

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

    ResultCode OIV::GetTexelInfo(const OIV_CMD_TexelInfo_Request& texel_request, OIV_CMD_TexelInfo_Response& texelresponse)
    {
        
        IMCodec::ImageSharedPtr  image = GetImage(texel_request.handle);
        if (image != nullptr)
        {
            if (texel_request.x >= 0
                && texel_request.x < image->GetWidth()
                && texel_request.y >= 0
                && texel_request.y < image->GetHeight())
            {
                texelresponse.type = (OIV_TexelFormat)image->GetTexelFormat();
                OIV_Util_GetBPPFromTexelFormat(texelresponse.type, &texelresponse.size);
                memcpy_s(texelresponse.buffer, OIV_CMD_TexelInfo_Buffer_Size, image->GetBufferAt(texel_request.x, texel_request.y), texelresponse.size / CHAR_BIT);
                    
                return RC_Success;
            }
            else return RC_UknownError;
        }

        return RC_ImageNotFound;
        
    }

    //ResultCode OIV::SetImageProperties(const OIV_CMD_ImageProperties_Request& imageProperties)
    //{
    //    if (imageProperties.opacity > 0.0)
    //    {
    //        UploadImageToRenderer(imageProperties.imageHandle); // if not already uploaded
    //    }
    //    else
    //    {
    //        //Don't remove from renderer when opacity is 0, cache image in the GPU until removed.
    //        //RemoveImageFromRenderer(imageProperties.imageHandle);
    //    }
    //    if (fImagesUploadToRenderer.contains(imageProperties.imageHandle))
    //        fRenderer->SetImageProperties(imageProperties);
    //    
    //    return RC_Success;
    //}

    ResultCode OIV::GetKnownFileTypes(OIV_CMD_GetKnownFileTypes_Response& res)
    {
        return RC_NotImplemented; 
     /*   std::wstring knownFileTypes = fImageLoader.GetKnownFileTypes();
        std::string knownFileTypesAnsi =  LLUtils::StringUtility::ConvertString<std::string>(knownFileTypes);
        res.bufferSize = knownFileTypesAnsi.size() + 1;
        if (res.knownFileTypes != nullptr)
            memcpy(res.knownFileTypes, knownFileTypesAnsi.data(), knownFileTypesAnsi.size() + 1);
        
        return RC_Success;*/
    }

    ResultCode OIV::RegisterCallbacks(const OIV_CMD_RegisterCallbacks_Request& callbacks)
    {
        fCallBacks = callbacks;
        return RC_Success;
    }

    ResultCode OIV::GetSubImages(const OIV_CMD_GetSubImages_Request& req, OIV_CMD_GetSubImages_Response & res)
    {
        return ResultCode::RC_NotImplemented;
        // ImageManager::VecImageHandles children = fImageManager.GetChildrenOf(req.handle);

        // //Copy no more then res.sizeOfArray elements-
        // res.copiedElements = static_cast<uint32_t>(std::min<size_t>(req.arraySize, children.size()));
        // memcpy(req.childrenArray, children.data(), res.copiedElements * sizeof(ImageHandle));

        //return ResultCode::RC_Success;
    }

    ResultCode OIV::UnloadFile(const ImageHandle handle)
    {
        return ResultCode::RC_NotImplemented;

        /*ResultCode result = RC_Success;
        RemoveImageFromRenderer(handle);
        fImageManager.RemoveImage(handle);

        return result;*/
    }

    int OIV::Init()
    {
        static_assert(OIV_TexelFormat::TF_COUNT == static_cast<OIV_TexelFormat>( IMCodec::TexelFormat::COUNT), "Wrong array size");

        LLUtils::Exception::OnException.Add([this](LLUtils::Exception::EventArgs args)
        {
            if (fCallBacks.OnException != nullptr)
            {
                auto formattedcallStack = LLUtils::Exception::FormatStackTrace(args.stackTrace);
                OIV_Exception_Args localArgs = { };
                localArgs.errorCode = static_cast<int>(args.errorCode);
                localArgs.callstack = formattedcallStack.c_str();
                localArgs.description = args.description.c_str();
                localArgs.systemErrorMessage = args.systemErrorMessage.c_str();
                localArgs.functionName = args.functionName.c_str();
                fCallBacks.OnException(localArgs, fCallBacks.userPointer);
            }
        }
        );

        fRenderer = CreateBestRenderer();
        for (const auto renderable : fPendingRenderables)
            fRenderer->AddRenderable(renderable);
            
        fPendingRenderables.clear();
        

        OIV_RendererInitializationParams params = {};

        auto GetVersionAsString = []
        {
            constexpr auto dot = OIV_TEXT(".");
            OIVStringStream ss;
            ss << OIV_VERSION_MAJOR << dot << OIV_VERSION_MINOR << dot << OIV_VERSION_BUILD << dot << OIV_VERSION_REVISION;
            return ss.str();

        };

        //TODO: add renderer properties to get a unique renderer name instead of hard coded "D3D11"
		OIVString appDataPath = LLUtils::StringUtility::ConvertString<OIVString>(LLUtils::PlatformUtility::GetAppDataFolder()) + 
			+ OIV_TEXT("/OIV/") + GetVersionAsString() + OIV_TEXT("/Renderer/D3D11/.");
        params.container = fParent;
        
        params.dataPath = appDataPath.c_str();
        fRenderer->Init(params);
        return 0;
    }


    int OIV::SetParent(std::size_t handle)
    {
        fParent = handle;
        return 0;
    }
    int OIV::Refresh()
    {
        RefreshRenderer();
        return 0;
    }

    IMCodec::ImageSharedPtr OIV::GetImage(ImageHandle handle) const
    {
        return fImageManager.GetImage(handle);
    }

    ResultCode OIV::GetFileInformation(ImageHandle handle, OIV_CMD_QueryImageInfo_Response& info)
    {
        using namespace  IMCodec;
        ImageSharedPtr image = GetImage(handle);

        if (image != nullptr)
        {
            info.width = image->GetWidth();
            info.height = image->GetHeight();
            info.rowPitchInBytes = image->GetRowPitchInBytes();
            info.bitsPerPixel = image->GetBitsPerTexel();
            info.NumSubImages = image->GetNumSubImages();
            info.texelFormat = static_cast<OIV_TexelFormat>(image->GetTexelFormat());
            return RC_Success;
        }

        return RC_InvalidImageHandle;
    }

    int OIV::SetTexelGrid(const CmdRequestTexelGrid& viewParams)
    {
        fShowGrid = viewParams.gridSize > 0.0;
        fTransparencyShade = viewParams.transparencyMode;
        return RC_Success;
    }

  
    int OIV::SetClientSize(uint16_t width, uint16_t height)
    {
        fClientSize = { width, height };
        return 0;
    }

    ResultCode OIV::AxisAlignTrasnform(const OIV_CMD_AxisAlignedTransform_Request& request, OIV_CMD_AxisAlignedTransform_Response& response)
    {
        IMCodec::ImageSharedPtr image = fImageManager.GetImage(request.handle);
        if (image != nullptr)
        {
            IMUtil::OIV_AxisAlignedTransform transform;
            transform.rotation = static_cast<IMUtil::OIV_AxisAlignedRotation>(request.transform.rotation);
            transform.flip = static_cast<IMUtil::OIV_AxisAlignedFlip>(request.transform.flip);
            image = IMUtil::ImageUtil::Transform(transform,  image);
            response.handle = fImageManager.AddImage(image);
            return RC_Success;
        }

        return RC_UknownError;
    }

    ResultCode OIV::ResampleImage(const OIV_CMD_Resample_Request& resampleRequest, ImageHandle& handle)
    {
        using namespace IMCodec;
        //resample the displayed image.
        ImageSharedPtr original = fImageManager.GetImage(resampleRequest.imageHandle);
        ImageSharedPtr resmapled = Resample(original, resampleRequest.size);
        handle = fImageManager.AddImage(resmapled);
        return RC_Success;
    }

  
#pragma endregion

}

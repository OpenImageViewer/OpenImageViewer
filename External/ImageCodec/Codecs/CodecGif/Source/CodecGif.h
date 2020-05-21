#pragma once

#include <assert.h>
#include <IImagePlugin.h>
#include <gif_lib.h>
namespace IMCodec
{

    struct GifReadContext
    {
        const uint8_t* buffer;
        size_t bufferSize;
        size_t pos;
    };

 
    int ReadGifBuffer(GifFileType * gifType, GifByteType * byteType, int length)
    {
        GifReadContext* context = (GifReadContext*)(gifType->UserData);
        // dont overflow buffer;
        size_t bytesToRead = std::min<size_t>(length, context->bufferSize - context->pos);
        if (bytesToRead != length)
        {
            //TODO: warning
        }
        memcpy(byteType, context->buffer + context->pos, bytesToRead);
        context->pos += bytesToRead;
        return static_cast<int>(bytesToRead);
    }

    class CodecGif : public IImagePlugin
    {
    private:
        PluginProperties mPluginProperties = { "GifLib image codec","gif" };
    public:
        PluginProperties& GetPluginProperties() override
        {
            return mPluginProperties;
        }

        //Base abstract methods
        bool LoadImage(const uint8_t* buffer, std::size_t size, ImageDescriptor& out_properties) override
        {
            int e;
            GifReadContext context = {buffer, size, 0};

            GifFileType* gif = DGifOpen((void*)&context,&ReadGifBuffer, &e);
            
            if (gif != nullptr && DGifSlurp(gif) == GIF_OK)
            {
                SavedImage* firstImage = gif->SavedImages;
                GraphicsControlBlock gcb;

                int transparentColor = -1;

                for (int i = 0; i < firstImage->ExtensionBlockCount; i++)
                {
                    const ExtensionBlock& eb = firstImage->ExtensionBlocks[i];
                    switch (eb.Function)
                    {
                        case CONTINUE_EXT_FUNC_CODE    :    /* continuation subblock */
                            break;
                        case COMMENT_EXT_FUNC_CODE     :    /* comment */
                            break;
                        case GRAPHICS_EXT_FUNC_CODE    :    /* graphics control (GIF89) */
                            if (DGifExtensionToGCB(eb.ByteCount, eb.Bytes, &gcb) == GIF_OK)
                            {
                                transparentColor = gcb.TransparentColor;
                            }
                            break;
                        case PLAINTEXT_EXT_FUNC_CODE   :    /* plaintext */
                            break;
                        case APPLICATION_EXT_FUNC_CODE :    /* application block */
                            break;
                    }
                    
                }

                out_properties.fProperties.Width = gif->SavedImages->ImageDesc.Width;
                out_properties.fProperties.Height = gif->SavedImages->ImageDesc.Height;
                out_properties.fProperties.NumSubImages = 0;
                out_properties.fProperties.TexelFormatDecompressed = IMCodec::TexelFormat::I_R8_G8_B8_A8;
                out_properties.fProperties.TexelFormatStorage = IMCodec::TexelFormat::I_X8;
                out_properties.fProperties.RowPitchInBytes = out_properties.fProperties.Width * 4;
                out_properties.fData.Allocate(out_properties.fProperties.Height * out_properties.fProperties.RowPitchInBytes);
                for (uint32_t y = 0 ; y < out_properties.fProperties.Height ; y++)
                    for (uint32_t x = 0; x < out_properties.fProperties.Width; x++)
                    {
                        const uint32_t bufPos = (y * out_properties.fProperties.Width) + x;
                        int idx = gif->SavedImages->RasterBits[bufPos];
                        unsigned long alpha = idx == transparentColor ? 0 : 0xff;
                        GifColorType colorType = gif->SColorMap->Colors[idx];
                        const uint32_t RGBACOLOR = (alpha << 24) | (colorType.Blue << 16) | (colorType.Green << 8) | colorType.Red;
                        ((uint32_t*)out_properties.fData.data())[bufPos] = RGBACOLOR;
                        
                    }

                DGifCloseFile(gif, &e);
                return true;
            }
            return false;
        }
    };
}

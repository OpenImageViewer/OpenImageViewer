#pragma once

#include <IImagePlugin.h>
#include "nv_dds.h"
#include "s3tc.h"

namespace IMCodec
{
    class CodecDDS : public IImagePlugin
    {
    private:
            PluginProperties mPluginProperties;
    public:

        CodecDDS() : mPluginProperties({ "NV_DDS image codec","dds" , true})
        {
            
        }
        
        PluginProperties& GetPluginProperties() override
        {
            return mPluginProperties;
        }


        void LoadSurface(bool isCompressed , unsigned storageFormat, const nv_dds::CSurface& surface , TexelFormat format, ImageDescriptor& out_properties)
        {

            LLUtils::Buffer decompressedBuffer;

            const uint32_t decompressedTexelSize = GetTexelFormatSize(format) / 8;
            const uint32_t decompressedImageSize = decompressedTexelSize * surface.get_width() * surface.get_height();

            if (isCompressed)
            {
                //Not sure why need to add extra padding here.
                constexpr int padding = 4;
                decompressedBuffer.Allocate(decompressedTexelSize * (surface.get_width() + padding) * surface.get_height());

                switch (storageFormat)
                {
                case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
                    BlockDecompressImageDXT1(surface.get_width(), surface.get_height(), static_cast<uint8_t*>(surface), reinterpret_cast<unsigned long*>(decompressedBuffer.data()));
                    break;
                case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                    BlockDecompressImageDXT5(surface.get_width(), surface.get_height(), static_cast<uint8_t*>(surface), reinterpret_cast<unsigned long*>(decompressedBuffer.data()));
                    break;
                    //TODO: add implementation for decompressing DXT3 
                case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
                default:
                    LL_EXCEPTION_NOT_IMPLEMENT(std::string(" DDS format: ") + std::to_string(storageFormat) + " is not implemented");
                    break;
                }
            }
            else
            {
                //Not compressed, copy as is.
                decompressedBuffer.Allocate(surface.get_size());
                decompressedBuffer.Write(reinterpret_cast<std::byte*>(static_cast<uint8_t*>(surface)), 0, surface.get_size());
            }




            out_properties.fProperties.Width = surface.get_width();
            out_properties.fProperties.Height = surface.get_height();
            out_properties.fProperties.TexelFormatDecompressed = format;
            out_properties.fData = std::move(decompressedBuffer);

            //TODO: chech if need to extract row pitch from DDS.
            out_properties.fProperties.RowPitchInBytes = surface.get_width() * decompressedTexelSize;
        }

        virtual bool LoadImages(const uint8_t* buffer, std::size_t size, std::vector<ImageDescriptor>& out_vec_properties)
        {
            using namespace std;
            using namespace nv_dds;
            bool success = false;

            class membuf : public std::streambuf
            {
            public:
                membuf(char* begin, char* end) {
                    this->setg(begin, begin, end);
                }

            };


            uint8_t* buf = const_cast<uint8_t*>(buffer);
            membuf sbuf(reinterpret_cast<char*>(buf), reinterpret_cast<char*>(buf + size));

            CDDSImage image;
            try
            {
                
                std::istream tmpBuf(&sbuf);
                image.load(tmpBuf, false);
                out_vec_properties.resize(image.get_num_mipmaps() + 1);

                unsigned format = image.get_format();
                TexelFormat texelFormat;
                switch (format)
                {
                case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
                    texelFormat = TexelFormat::I_B8_G8_R8;
                    break;
                case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                    texelFormat = TexelFormat::I_A8_B8_G8_R8;
                    break;
                case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
                    texelFormat = TexelFormat::I_A8_B8_G8_R8;
                    break;
                case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                    texelFormat = TexelFormat::I_A8_B8_G8_R8;
                    break;
                case GL_BGRA_EXT:
                    texelFormat = TexelFormat::I_B8_G8_R8_A8;
                    break;
                
                case GL_BGR_EXT:
                    texelFormat = TexelFormat::I_B8_G8_R8;
                    break;
                case GL_RGB:
                    texelFormat = TexelFormat::I_R8_G8_B8;
                    break;
                
                case GL_RGBA:
                    texelFormat = TexelFormat::I_R8_G8_B8_A8;
                    break;
                default:
                    texelFormat = TexelFormat::UNKNOWN;
                    break;
                }


                LoadSurface(image.is_compressed(), format, image.get_surface(), texelFormat, out_vec_properties[0]);
                
                out_vec_properties[0].fProperties.NumSubImages = static_cast<uint32_t>(std::max<int>(0, image.get_num_mipmaps()));
                out_vec_properties[0].fProperties.TexelFormatDecompressed = texelFormat;
                

                
                for (unsigned int i = 0; i < image.get_num_mipmaps(); i++)
                {
                    ImageDescriptor& out_properties = out_vec_properties[i + 1];
                    memset(&out_properties, 0, sizeof(ImageDescriptor));
                    const nv_dds::CSurface& surface = image.get_mipmap(i);

                    LoadSurface(image.is_compressed(), format, surface, texelFormat, out_properties);
                    out_properties.fProperties.TexelFormatDecompressed = texelFormat;
                }
                
                success = true;

            }

            catch (...)
            {

            }

            return success;
        }


        //Base abstract methods
        bool LoadImage(const uint8_t* buffer, std::size_t size, ImageDescriptor& out_properties) override
        {
            using namespace std;
            using namespace nv_dds;
            bool success = false;
            
            class membuf : public std::streambuf
            {
            public:
                membuf(char* begin, char* end) {
                    this->setg(begin, begin, end);
                }
            };

            uint8_t* buf = const_cast<uint8_t*>(buffer);
            membuf sbuf(reinterpret_cast<char*>(buf), reinterpret_cast<char*>(buf + size));

            CDDSImage image;
            try
            {
                istream bufStream = istream(&sbuf);
                image.load(bufStream, false);
                out_properties.fProperties.Width = image.get_width();
                out_properties.fProperties.Height = image.get_height();
                out_properties.fProperties.NumSubImages = image.get_num_mipmaps();
                out_properties.fData.Allocate(image.get_size());
                out_properties.fData.Write(reinterpret_cast<std::byte*>(static_cast<uint8_t*>(image)), 0, image.get_size());

                unsigned format = image.get_format();
                switch (format)
                {
                case GL_BGRA_EXT:
                    out_properties.fProperties.TexelFormatDecompressed = TexelFormat::I_B8_G8_R8_A8;
                    break;
                case GL_BGR_EXT:
                    out_properties.fProperties.TexelFormatDecompressed = TexelFormat::I_B8_G8_R8;
                    break;
                case GL_RGB:
                    out_properties.fProperties.TexelFormatDecompressed = TexelFormat::I_R8_G8_B8;
                    break;
                case GL_RGBA:
                    out_properties.fProperties.TexelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;
                    break;
                }
                //TODO: chech if need to extract row pitch from DDS.
                out_properties.fProperties.RowPitchInBytes = image.get_width() * GetTexelFormatSize(out_properties.fProperties.TexelFormatDecompressed) / 8;
                success = true;

            }

            catch(...)
            {
              
            }

            return success;
        }
    };
}
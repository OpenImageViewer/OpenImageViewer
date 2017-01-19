#pragma once

#include <IImagePlugin.h>
#include "nv_dds.h"

namespace IMCodec
{
    class CodecDDS : public IImagePlugin
    {
    private:
            PluginProperties mPluginProperties;
    public:

        CodecDDS() : mPluginProperties({ "NV_DDS image codec","dds" })
        {
            
        }
        
        PluginProperties& GetPluginProperties() override
        {
            return mPluginProperties;
        }

        //Base abstract methods
        bool LoadImage(const uint8_t* buffer, std::size_t size, ImageProperies& out_properties) override
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
            //image.load("D:\\pine04.dds",false);
            image.load(istream(&sbuf) , false);
            unsigned format = image.get_format();

            
            out_properties.Width = image.get_width();
            out_properties.Height = image.get_height();
            out_properties.NumSubImages = image.get_num_mipmaps();
            out_properties.ImageBuffer = new uint8_t[image.get_size()]; 
            memcpy(out_properties.ImageBuffer, static_cast<uint8_t*>(image), image.get_size());
            switch (format)
            {
                case GL_BGRA_EXT:
                    out_properties.TexelFormatDecompressed = TF_I_B8_G8_R8_A8;
                    break;
                case GL_BGR_EXT:
                    out_properties.TexelFormatDecompressed = TF_I_B8_G8_R8;
                    break;
                case GL_RGB:
                    out_properties.TexelFormatDecompressed = TF_I_R8_G8_B8;
                    break;
                case GL_RGBA:
                    out_properties.TexelFormatDecompressed = TF_I_R8_G8_B8_A8;
                    break;
            }
            //TODO: chech if need to extract row pitch from DDS.
            out_properties.RowPitchInBytes = image.get_width() * GetTexelFormatSize(out_properties.TexelFormatDecompressed) / 8;
            success = true;
            
            return success;
        }
    };
}

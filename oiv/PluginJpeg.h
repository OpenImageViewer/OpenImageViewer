#pragma once
#pragma once
#include "ImagePlugin.h"
#include <turbojpeg.h>
#include <jpeglib.h>
namespace OIV
{
    class PluginJpeg : public ImagePlugin
    {
    private:
        static tjhandle ftjHandle;

    public:
        virtual char* GetHint() override
        {
            return "jpg;jpeg";
        }

        //virtual bool LoadImage(std::string filePath, ImageProperies& out_properties) override
        //{
        //    bool success = false;

        //    std::ifstream file(filePath, std::ifstream::ate | std::ifstream::binary);

        //    if (file.is_open())
        //    {
        //        size_t fileSize = file.tellg();
        //        file.seekg(0, std::ios::beg);

        //        unsigned char* byteArray = new unsigned char[fileSize];
        //        file.read((char*)byteArray, fileSize);

        //        int width = 0;
        //        int height = 0;

        //        int res = 0;
        //        int bytesPerPixel = 3;
        //        int result;
        //        jpeg_decompress_struct cinfo = { 0 };
        //        
        //        jpeg_create_decompress(&cinfo);
        //        jpeg_mem_src(&cinfo, byteArray, fileSize);
        //        
        //        result = jpeg_read_header(&cinfo, FALSE);
        //        jpeg_start_decompress(&cinfo);
        //        width = cinfo.output_width;
        //        height = cinfo.output_height;
        //        int pixel_size = cinfo.output_components;


        //        int bmp_size = width * height * pixel_size;
        //        unsigned char* bmp_buffer = (unsigned char*)malloc(bmp_size);

        //        // The row_stride is the total number of bytes it takes to store an
        //        // entire scanline (row). 
        //        int row_stride = width * pixel_size;

        //        while (cinfo.output_scanline < cinfo.output_height) {
        //            unsigned char *buffer_array[1];
        //            buffer_array[0] = bmp_buffer + \
        //                (cinfo.output_scanline) * row_stride;

        //            jpeg_read_scanlines(&cinfo, buffer_array, 1);

        //        }
        //        


        //        // Once done reading *all* scanlines, release all internal buffers,
        //        // etc by calling jpeg_finish_decompress. This lets you go back and
        //        // reuse the same cinfo object with the same settings, if you
        //        // want to decompress several jpegs in a row.
        //        //
        //        // If you didn't read all the scanlines, but want to stop early,
        //        // you instead need to call jpeg_abort_decompress(&cinfo)
        //        jpeg_finish_decompress(&cinfo);

        //        // At this point, optionally go back and either load a new jpg into
        //        // the jpg_buffer, or define a new jpeg_mem_src, and then start 
        //        // another decompress operation.

        //        // Once you're really really done, destroy the object to free everything
        //        jpeg_destroy_decompress(&cinfo);
        //        // And free the input buffer
        //        free(byteArray);
        //      
        //       /*         unsigned char* buffer = new unsigned char[width * height * bytesPerPixel];


        //                if (tjDecompress(ftjHandle, byteArray, fileSize, buffer, width, width * bytesPerPixel, height, bytesPerPixel, 0) != -1)
        //                {
        //                    out_properties.ImageBuffer = (void*)buffer;
        //                    out_properties.BitsPerTexel = bytesPerPixel * 8;
        //                    out_properties.Format = IF_BGR;
        //                    out_properties.Width = width;
        //                    out_properties.Height = height;
        //                    out_properties.RowPitchInBytes = bytesPerPixel * width;
        //                    out_properties.NumSubImages = 0;
        //                    out_properties.Type = IT_BITMAP;


        //                    success = true;
        //                }*/
        //      
        //    }

        //    return success;
        //}

        virtual bool LoadImage(std::string filePath, ImageProperies& out_properties) override
        {
            bool success = false;
            
            std::ifstream file(filePath, std::ifstream::ate | std::ifstream::binary);

            if (file.is_open())
            {
                size_t fileSize = file.tellg();
                file.seekg(0, std::ios::beg);

                unsigned char* byteArray = new unsigned char[fileSize];
                file.read((char*)byteArray, fileSize);

                int width = 0;
                int height = 0;

                int bytesPerPixel = 3;
                int subsamp;
                if (tjDecompressHeader2(ftjHandle, byteArray, fileSize, &width, &height, &subsamp) != -1)
                {
                    unsigned char* buffer = new unsigned char[width * height * bytesPerPixel];

                    if (tjDecompress2(ftjHandle, byteArray, fileSize, buffer, width, width * bytesPerPixel, height, TJPF_RGB, 0) != -1)
                    {
                        out_properties.ImageBuffer = (void*)buffer;
                        out_properties.BitsPerTexel = bytesPerPixel * 8;
                        out_properties.Type = IT_BYTE_BGR;
                        out_properties.Width = width;
                        out_properties.Height = height;
                        out_properties.RowPitchInBytes = bytesPerPixel * width;
                        out_properties.NumSubImages = 0;

                        success = true;
                    }
                }
                delete[]byteArray;
            }
            
            return success;

        }

    };
    tjhandle PluginJpeg::ftjHandle = tjInitDecompress();
}
#pragma once
#include <fstream>
#include <PlatformUtility.h>

struct BitmapBuffer
{
    const std::byte* buffer;
    uint8_t bitsPerPixel;
    uint32_t width;
    uint32_t height;
    uint32_t rowPitch;
};

class Bitmap;
using BitmapShaderPtr = std::shared_ptr< Bitmap>;

class Bitmap
{
    struct {
        BITMAPINFOHEADER bmiHeader;
        RGBQUAD bmiColors[256];
    } fBitmapInfo = {};

public:
    Bitmap(HBITMAP bitmap) : fBitmap(bitmap)
    {


    }



    const BITMAPINFOHEADER& GetBitmapHeader()
    {
        if (fBitmapInfo.bmiHeader.biSize == 0)
        {
            fBitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

            HDC hDC = GetDC(nullptr);

            GetDIBits(hDC, fBitmap, 0, 1, nullptr, (BITMAPINFO *)&fBitmapInfo,
                DIB_RGB_COLORS);
            ReleaseDC(nullptr, hDC);
        }
        return fBitmapInfo.bmiHeader;
    }



    static BitmapShaderPtr FromMemory(const BitmapBuffer& bitmapBuffer)
    {

        const int height = bitmapBuffer.height;
        const int width = bitmapBuffer.width;
        const int bpp = bitmapBuffer.bitsPerPixel;
        const int rowPitch = bitmapBuffer.rowPitch;

        BITMAPINFO bi;
        bi.bmiHeader = {};
        bi.bmiHeader.biBitCount = bpp;
        bi.bmiHeader.biClrImportant = 0;
        bi.bmiHeader.biClrUsed = 0;
        bi.bmiHeader.biCompression = 0;
        bi.bmiHeader.biHeight = height;
        bi.bmiHeader.biWidth = width;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biSize = 40;
        bi.bmiHeader.biSizeImage = rowPitch * height;

        bi.bmiHeader.biXPelsPerMeter = 11806;
        bi.bmiHeader.biYPelsPerMeter = 11806;


        bi.bmiColors[0].rgbBlue = 0;
        bi.bmiColors[0].rgbGreen = 0;
        bi.bmiColors[0].rgbRed = 0;
        bi.bmiColors[0].rgbReserved = 0;



        const std::byte* pPixels = (bitmapBuffer.buffer);

        char* ppvBits;

        HBITMAP hBitmap = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&ppvBits, NULL, 0);
        int res = SetDIBits(NULL, hBitmap, 0, height, pPixels, &bi, DIB_RGB_COLORS);
        return std::make_shared<BitmapShaderPtr::element_type>(hBitmap);
    }


    static BitmapShaderPtr FromMemory(const std::wstring& filePath)
    {

        std::ifstream is;
        is.open(filePath, std::ios::binary);
        is.seekg(0, std::ios::end);
        size_t length = is.tellg();
        is.seekg(0, std::ios::beg);
        char* pBuffer = new char[length];
        is.read(pBuffer, length);
        is.close();



        const int height = 816;
        const int width = 822;
        const int bpp = 32;
        const int rowPitch = 3288;

        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++)
            {
                size_t start = y * rowPitch + x * 4;
                //flip Red and blue channles.
                std::swap(pBuffer[start + 0], pBuffer[start + 2]);
            }

        char* flipped = new char[length];

        for (int y = 0; y < height; y++)
            memcpy(flipped + rowPitch * y, pBuffer + rowPitch * (height - y - 1), rowPitch);

        delete pBuffer;
        pBuffer = flipped;


        BITMAPINFO bi;
        bi.bmiHeader = {};
        bi.bmiHeader.biBitCount = bpp;
        bi.bmiHeader.biClrImportant = 0;
        bi.bmiHeader.biClrUsed = 0;
        bi.bmiHeader.biCompression = 0;
        bi.bmiHeader.biHeight = height;
        bi.bmiHeader.biWidth = width;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biSize = 40;
        bi.bmiHeader.biSizeImage = rowPitch * height;

        bi.bmiHeader.biXPelsPerMeter = 11806;
        bi.bmiHeader.biYPelsPerMeter = 11806;


        bi.bmiColors[0].rgbBlue = 0;
        bi.bmiColors[0].rgbGreen = 0;
        bi.bmiColors[0].rgbRed = 0;
        bi.bmiColors[0].rgbReserved = 0;



        char* pPixels = (pBuffer);

        char* ppvBits;

        HBITMAP hBitmap = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&ppvBits, NULL, 0);
        int res = SetDIBits(NULL, hBitmap, 0, height, pPixels, &bi, DIB_RGB_COLORS);
        //std::string lastError = LLUtils::PlatformUtility::GetLastErrorAsString<char>();

        delete pBuffer;

        return std::make_shared<BitmapShaderPtr::element_type>(hBitmap);
    }

    static BitmapShaderPtr FromFileAnyFormat(const std::wstring& filePath)
    {
        std::ifstream is;
        is.open(filePath, std::ios::binary);
        is.seekg(0, std::ios::end);
        size_t length = is.tellg();
        is.seekg(0, std::ios::beg);
        char* pBuffer = new char[length];
        is.read(pBuffer, length);
        is.close();
        
         HBITMAP bmp = (HBITMAP) LoadImage(GetModuleHandle(nullptr), filePath.c_str() /*L"D:\\Cursor\\arrow-C.png"*/, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
         return std::make_shared<BitmapShaderPtr::element_type>(bmp);
        /*tagBITMAPFILEHEADER bfh = *(tagBITMAPFILEHEADER*)pBuffer;
        tagBITMAPINFOHEADER bih = *(tagBITMAPINFOHEADER*)(pBuffer + sizeof(tagBITMAPFILEHEADER));
        RGBQUAD             rgb = *(RGBQUAD*)(pBuffer + sizeof(tagBITMAPFILEHEADER) + sizeof(tagBITMAPINFOHEADER));

        BITMAPINFO bi;
        bi.bmiColors[0] = rgb;
        bi.bmiHeader = bih;

        char* pPixels = (pBuffer + bfh.bfOffBits);

        char* ppvBits;

        HBITMAP hBitmap = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&ppvBits, NULL, 0);

        int res = SetDIBits(NULL, hBitmap, 0, bih.biHeight, pPixels, &bi, DIB_RGB_COLORS);

        return std::make_shared<BitmapShaderPtr::element_type>(hBitmap);*/
    }

    static BitmapShaderPtr FromFile(const std::wstring& filePath)
    {
        std::ifstream is;
        is.open(filePath, std::ios::binary);
        is.seekg(0, std::ios::end);
        size_t length = is.tellg();
        is.seekg(0, std::ios::beg);
        char* pBuffer = new char[length];
        is.read(pBuffer, length);
        is.close();

        tagBITMAPFILEHEADER bfh = *(tagBITMAPFILEHEADER*)pBuffer;
        tagBITMAPINFOHEADER bih = *(tagBITMAPINFOHEADER*)(pBuffer + sizeof(tagBITMAPFILEHEADER));
        RGBQUAD             rgb = *(RGBQUAD*)(pBuffer + sizeof(tagBITMAPFILEHEADER) + sizeof(tagBITMAPINFOHEADER));

        BITMAPINFO bi;
        bi.bmiColors[0] = rgb;
        bi.bmiHeader = bih;

        char* pPixels = (pBuffer + bfh.bfOffBits);

        char* ppvBits;

        HBITMAP hBitmap = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&ppvBits, NULL, 0);

        int res = SetDIBits(NULL, hBitmap, 0, bih.biHeight, pPixels, &bi, DIB_RGB_COLORS);

        return std::make_shared<BitmapShaderPtr::element_type>(hBitmap);
    }

    HBITMAP GetHBitmap() const { return fBitmap; }

private:
    HBITMAP fBitmap;
};
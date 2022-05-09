#pragma once
#include <fstream>
#include <LLUtils/PlatformUtility.h>
#include <Win32/BitmapHelper.h>

class ImageList
{
public:
    ImageList()
    {
        LOGFONT font{};
        font.lfHeight = 20;
        font.lfWeight = FW_NORMAL;
        font.lfQuality = CLEARTYPE_QUALITY;
        wcscpy_s(font.lfFaceName, L"Segoe UI");
        fFont = CreateFontIndirect(&font);

        fGrayBrush = CreateSolidBrush(RGB(245, 249, 213));
        fLightgrayBrush = CreateSolidBrush(RGB(224, 249, 213));
        fBlueBrush = CreateSolidBrush(RGB(0, 0, 200));
        fPen = CreatePen(PS_DASHDOTDOT, lineWidth, RGB(0, 0, 0));
     }

    ~ImageList()
    {
        if (fPen)
            DeleteObject(fPen);
        if (fGrayBrush)
            DeleteObject(fGrayBrush);
        if (fLightgrayBrush)
            DeleteObject(fLightgrayBrush);
        if (fFont)
            DeleteObject(fFont);

    }
    struct ImageSelectionChangeArgs
    {
        int imageIndex = -1;
    };
    using ImageSelectionChangEvent = LLUtils::Event<void(const ImageSelectionChangeArgs&)>;

    ImageSelectionChangEvent ImageSelectionChanged;
 

    struct ImageDesc
    {
        uint32_t index;
        std::wstring title;
        ::Win32::BitmapSharedPtr bitmap;
        ::Win32::BitmapSharedPtr mask;
    };

    struct RGBAImageDesc
    {
        std::byte* buffer;
        uint32_t width;
        uint32_t height;
    };

public:
    void SetTarget(HWND hwnd)
    {
        fTargetWindow = hwnd;
    }

    int GetSelected() const
    {
        return fSelected;
    }

    void Clear()
    {
        fImages.clear();
    }

    void SetSelected(int selected)
    {
        if (selected != fSelected)
        {
            fSelected = selected;
            ImageSelectionChangeArgs args;
            args.imageIndex = selected;
            ImageSelectionChanged.Raise(args);
            InvalidateRect(fTargetWindow, nullptr, FALSE);
        }
    }


    void MouseClick([[maybe_unused]] int xPos, int yPos)
    {
        int selected =  yPos / fEntryHeight + fPos;
        if (selected < static_cast<int>(fImages.size()))
            SetSelected(selected);
    }

    void SetPos(int pos)
    {
        fPos = pos;
    }

    size_t GetNumberOfDisplayedElements()
    {
        HWND hwnd = fTargetWindow;
        RECT rect;
        GetClientRect(hwnd, &rect);
        int height = rect.bottom - rect.top;

        const size_t numberOfMaxVisibleElements = height / fEntryHeight;
        return (std::min)(numberOfMaxVisibleElements, GetNumberOfElements());
    }

    size_t GetNumberOfElements()
    {
        return fImages.size();
    }


    void Draw()
    {
        RECT rect;
        HWND hwnd = fTargetWindow;
        GetClientRect(hwnd, &rect);
        const int imageDestWidth = 64;
        const int imageDestHeight = 64;
        const int entrywidth = rect.right - rect.left;
        int imagePos = 30;
     
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        HDC hdc = ps.hdc;// GetDC(hwnd);//  BeginPaint(GetHandleClient(), &ps);
        HDC hdcMem = CreateCompatibleDC(nullptr);
        SelectObject(hdc, fPen);

        SelectObject(hdc, fFont);


        int currentEntry = 0;
        int x = 0;
        int y = fPos * -100;
        for (const ImageDesc& imageDesc : fImages)
        {
            RECT r;
            r.top = y;
            r.bottom = y + fEntryHeight;
            r.left = 0;
            r.right = entrywidth;

            if (currentEntry == fSelected)
                FillRect(hdc, &r, fBlueBrush);
            else
                FillRect(hdc, &r, currentEntry % 2 == 0 ? fGrayBrush : fLightgrayBrush);

            MoveToEx(hdc, x, y + fEntryHeight - lineWidth, nullptr);
            
            int textpos = 5 + y;

            LineTo(hdc, entrywidth, y + fEntryHeight - lineWidth);
            {
                RECT r1 = { 0, textpos ,0, textpos + 24 };
                std::wstring text = imageDesc.title;
                DrawText(hdc, text.c_str(), static_cast<int>(text.length()), &r1, DT_CALCRECT);
                SetBkMode(hdc, TRANSPARENT);
                int offset = (entrywidth - (r1.right - r1.left)) / 2;
                r1.right += offset;
                r1.left += offset;
                DrawText(hdc, text.c_str(), static_cast<int>(text.length()), &r1,DT_CENTER);
            }
            
            SetStretchBltMode(hdc, STRETCH_HALFTONE);

            

            int finalWidth = std::min<int>(imageDestWidth, imageDesc.bitmap->GetBitmapHeader().biWidth);
            int finalHeight = std::min<int>(imageDestHeight, imageDesc.bitmap->GetBitmapHeader().biHeight);


            int imageFinalLocalYPos = finalHeight < imageDestHeight ? (fEntryHeight - finalHeight) / 2 : imagePos;

            BLENDFUNCTION bf{};      // structure for alpha blending 
            bf.BlendOp = AC_SRC_OVER;
            bf.BlendFlags = 0;
            bf.SourceConstantAlpha = 0xef;  // half of 0xff = 50% transparency 
            bf.AlphaFormat = 0;             // ignore source alpha channel 

            HBITMAP currentMask = imageDesc.mask->GetHBitmap();
            HBITMAP currentBitmap = imageDesc.bitmap->GetHBitmap();
            HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, currentMask);
            BitBlt(hdc, (entrywidth - finalWidth) / 2, y + imageFinalLocalYPos, finalWidth, finalHeight, hdcMem, 0, 0, SRCPAINT);

            hbmOld = (HBITMAP)SelectObject(hdcMem, currentBitmap); 

            BitBlt(hdc, (entrywidth - finalWidth) / 2, y + imageFinalLocalYPos, finalWidth, finalHeight, hdcMem, 0, 0, SRCAND);
            
            
            currentEntry++;
            y += fEntryHeight;
            SelectObject(hdcMem, hbmOld);
        }

        
        DeleteDC(hdcMem);
        //ReleaseDC(hwnd, hdc);
        EndPaint(hwnd, &ps);
    }

    void SetImage(const ImageDesc& imageDesc)
    {
        fImages.resize(imageDesc.index + 1);
        fImages[imageDesc.index] = imageDesc;
        fImages[imageDesc.index].bitmap = fImages[imageDesc.index].bitmap->resize(64,64,255);
        fImages[imageDesc.index].mask = fImages[imageDesc.index].mask->resize(64, 64, 0);
        InvalidateRect(this->fTargetWindow, nullptr, TRUE);
    }

   static  HBITMAP HBitmapFromMemory()
    {
        std::ifstream is;
        is.open("d:\\ImagesCopy\\loading.bmp", std::ios::binary);
        is.seekg(0, std::ios::end);
        size_t length = is.tellg();
        is.seekg(0, std::ios::beg);
        auto pBuffer = std::make_unique<char[]>(length);
        is.read(pBuffer.get(), length);
        is.close();

        tagBITMAPFILEHEADER bfh = *(tagBITMAPFILEHEADER*)pBuffer.get();
        tagBITMAPINFOHEADER bih = *(tagBITMAPINFOHEADER*)(pBuffer.get() + sizeof(tagBITMAPFILEHEADER));
        RGBQUAD             rgb = *(RGBQUAD*)(pBuffer.get() + sizeof(tagBITMAPFILEHEADER) + sizeof(tagBITMAPINFOHEADER));

        BITMAPINFO bi;
        bi.bmiColors[0] = rgb;
        bi.bmiHeader = bih;

        char* pPixels = (pBuffer.get() + bfh.bfOffBits);
        char* ppvBits;


        const uint32_t width = 400;
        const uint32_t height = 240;

        tagBITMAPINFOHEADER bihRGBA = {};
        bihRGBA.biSize = sizeof(bihRGBA);
        bihRGBA.biWidth = width;
        bihRGBA.biHeight = height;
        bihRGBA.biPlanes = 1;
        bihRGBA.biBitCount = 32;
        bihRGBA.biSizeImage = width * height * 32;
        bihRGBA.biXPelsPerMeter = 11808;
        bihRGBA.biYPelsPerMeter = 11808;

        BITMAPINFO bi32;
        bi32.bmiColors[0] = rgb;
        bi32.bmiHeader = bihRGBA;

        HBITMAP hBitmap = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&ppvBits, NULL, 0);
        SetDIBits(NULL, hBitmap, 0, bih.biHeight, pPixels, &bi, DIB_RGB_COLORS);

        auto pixels32 = std::make_unique<uint8_t[]>(width * height * 4);

        uint32_t rowPitch = LLUtils::Utility::Align<uint32_t>(bih.biWidth * (bih.biBitCount / 8), 4);

        uint8_t* sourceLine = (uint8_t*)pPixels;
        uint8_t* destLine = (uint8_t*)pixels32.get();

        LONG currentPixelInLine = 0;
        for (LONG line = 0; line < bih.biHeight; line++)
        {
            currentPixelInLine = 0;
            while (currentPixelInLine < bih.biWidth)
            {
                uint8_t* sourcePIxel = sourceLine + (currentPixelInLine * 3);

                ((uint32_t*)destLine)[currentPixelInLine] = (0x55 << 24) | (sourcePIxel[2] << 16) | (sourcePIxel[1] << 8) | sourcePIxel[0];
                //((uint32_t*)destLine)[currentPixelInLine] = 0xffffffff;
                currentPixelInLine++;
            }
            sourceLine += rowPitch;
            destLine += width * 4;
        }

        HBITMAP hBitmap32 = CreateDIBSection(NULL, &bi32, DIB_RGB_COLORS, (void**)&ppvBits, NULL, 0);
        SetDIBits(NULL, hBitmap32, 0, bihRGBA.biHeight, pixels32.get(), &bi32, DIB_RGB_COLORS);

        //GetObject(hBitmap, sizeof(BITMAP), &cBitmap);
        //return hBitmap32;
        return hBitmap;
    }

private:
    static constexpr int lineWidth = 2;
    HFONT fFont{};
    HBRUSH fGrayBrush{};
    HBRUSH fLightgrayBrush{};
    HBRUSH fBlueBrush{};
    HPEN fPen{};
    uint32_t fEntryHeight = 100;
    int fSelected = -1;
    int fPos = 0;
    HWND fTargetWindow;
    std::vector<ImageDesc> fImages;
};







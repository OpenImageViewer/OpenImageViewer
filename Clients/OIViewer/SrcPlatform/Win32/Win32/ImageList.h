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
        fPen = CreatePen(PS_SOLID, lineWidth, RGB(0, 0, 0));
        fVerticalPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 255));
        fVerticalPen2 = CreatePen(PS_SOLID, 1, RGB(0, 255, 0));
     }

    ~ImageList()
    {
        if (fVerticalPen2)
            DeleteObject(fVerticalPen2);
        if (fVerticalPen)
            DeleteObject(fVerticalPen);
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
        uint32_t index{};
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

            RECT rect;
            GetClientRect(fTargetWindow, &rect);
            auto height = rect.bottom - rect.top;
            int maxEntries = height / fEntryHeight;
            BOOL erase = FALSE;
            if (fSelected < fPos)
            {
                fPos = fSelected;
            }
            else 
            {
                int posOffset = height % fEntryHeight == 0 ? 0 : 1;
                if (fSelected - fPos > (maxEntries - posOffset))
                fPos =  std::min(static_cast<int>( GetNumberOfElements()), fSelected - maxEntries + posOffset);
                //might be the last position, erase the gap to the lowest part of the control.
                erase = TRUE;
            }


            args.imageIndex = selected;
            ImageSelectionChanged.Raise(args);
            InvalidateRect(fTargetWindow, nullptr, erase);
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

    size_t GetNumberOfElements() const
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
            {
                FillRect(hdc, &r, fBlueBrush);
                SetTextColor(hdc, RGB(255, 255, 255));
            }
            else
            {
                FillRect(hdc, &r, currentEntry % 2 == 0 ? fGrayBrush : fLightgrayBrush);
                SetTextColor(hdc, RGB(0, 0, 0));
            }

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
        
        SelectObject(hdc, fVerticalPen);
        MoveToEx(hdc, 0, 0, nullptr);
        LineTo(hdc, 0, y);
        SelectObject(hdc, fVerticalPen2);
        MoveToEx(hdc, 1, 0, nullptr);
        LineTo(hdc, 1, y);

        SelectObject(hdc, fVerticalPen);
        MoveToEx(hdc, 2, 0, nullptr);
        LineTo(hdc, 2, y);

        
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

private:
    static constexpr int lineWidth = 2;
    HFONT fFont{};
    HBRUSH fGrayBrush{};
    HBRUSH fLightgrayBrush{};
    HBRUSH fBlueBrush{};
    HPEN fPen{};
    HPEN fVerticalPen{};
    HPEN fVerticalPen2{};
    uint32_t fEntryHeight = 100;
    int fSelected = -1;
    //First image index in display
    int fPos = 0;
    HWND fTargetWindow{};
    std::vector<ImageDesc> fImages;
};







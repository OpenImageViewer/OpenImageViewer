//#pragma once
//#include <string>
//#include <iomanip>
//
//namespace OIV
//{
//    class ImageDescriptor
//    {
//    public:
//        ImageHandle imageHandle = ImageHandleNull;
//        ImageSource source = ImageSource::None;
//        std::wstring fileName;
//        uint32_t width = 0;
//        uint32_t height = 0;
//        uint8_t bpp = 0;
//        double loadTime = 0.0;
//        LLUtils::StopWatch::time_type_real displayTime = 0.0;
//        
//
//        const std::wstring& GetName() const
//        {
//
//            static std::wstring clipboard = L"[Clipboard]";
//            static std::wstring empty = L"";
//            switch (source)
//            {
//            case ImageSource::File:
//                return  fileName;
//            case ImageSource::Clipboard:
//                return clipboard;
//            case ImageSource::None:
//                return empty;
//            default:
//                LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "unexpected or corrupted value");
//            }
//        }
//
//        std::wstring GetDescription() const
//        {
//            std::wstringstream ss;
//            ss  << width << L" X " << height << L" X "
//                << bpp << L" BPP | loaded in " << std::fixed << std::setprecision(1)
//                << loadTime
//                << L"/" << displayTime + loadTime << L" ms";
//
//            return ss.str();
//
//        }
//    };
//}
//

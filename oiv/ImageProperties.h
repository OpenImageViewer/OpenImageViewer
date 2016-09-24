#pragma once
namespace OIV
{
    enum ImageType
    {
        IT_UNKNOWN
        , IT_BITMAP
        , IT_FLOAT
    };

    enum ImageFormat
    {
        IF_UNKNOWN,
        IF_RGBA,
        IF_BGR,
        IF_RGB

    };
    class ImageProperies
    {
    public:
        ImageProperies();

        bool IsInitialized() const;

        size_t Width;
        size_t Height;
        size_t RowPitchInBytes;
        size_t BitsPerTexel;
        ImageType Type;
        size_t NumSubImages;
        ImageFormat Format;
        void* ImageBuffer;
    };
}

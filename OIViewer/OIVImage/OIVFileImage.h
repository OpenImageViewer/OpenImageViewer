#pragma once
#include "OIVBaseImage.h"
namespace OIV
{

    struct FileLoadOptions
    {
        bool onlyRegisteredExtension;
    };

    class OIVFileImage : public OIVBaseImage
    {
    public:

        const std::wstring& GetFileName() const { return fFileName; }

        OIVFileImage(const std::wstring& fileName) : fFileName(fileName)
        {
            GetDescriptorMutable().Source = ImageSource::File;
        }

        ResultCode Load(const FileLoadOptions& loadOptions);

        ~OIVFileImage()
        {
            FreeImage();

        }


    private:
        const std::wstring fFileName;

    };
}
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
        const std::wstring& GetFileName() const;
        OIVFileImage(const std::wstring& fileName);
        ResultCode Load(const FileLoadOptions& loadOptions);

    private:
        const std::wstring fFileName;
    };
}
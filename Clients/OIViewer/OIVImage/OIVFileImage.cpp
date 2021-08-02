#include "OIVFileImage.h"
#include <LLUtils/FileMapping.h>
#include <LLUtils/StringUtility.h>
#include "../OIVCommands.h"

namespace OIV
{
    const std::wstring & OIVFileImage::GetFileName() const { return fFileName; }
    OIVFileImage::OIVFileImage(const std::wstring & fileName) : fFileName(fileName)
    {
        GetDescriptorMutable().Source = ImageSource::File;
    }
    ResultCode OIVFileImage::Load(const FileLoadOptions& loadOptions)
    {
        using namespace LLUtils;
        FileMapping fileMapping(fFileName);
        void* buffer = fileMapping.GetBuffer();
        std::size_t size = fileMapping.GetSize();
        std::string extension = LLUtils::StringUtility::ConvertString<std::string>(StringUtility::GetFileExtension(fFileName));

        using namespace LLUtils;
        OIV_CMD_LoadFile_Response loadResponse;
        OIV_CMD_LoadFile_Request loadRequest = {};

        loadRequest.buffer = (void*)buffer;
        loadRequest.length = size;
        std::string fileExtension = extension;
        strcpy_s(loadRequest.extension, OIV_CMD_LoadFile_Request::MAX_EXTENSION_SIZE, fileExtension.c_str());
        loadRequest.flags = static_cast<OIV_CMD_LoadFile_Flags>(
            (loadOptions.onlyRegisteredExtension ? OIV_CMD_LoadFile_Flags::OnlyRegisteredExtension : 0)
            | OIV_CMD_LoadFile_Flags::Load_Exif_Data);


        ResultCode result = OIVCommands::ExecuteCommand(CommandExecute::OIV_CMD_LoadFile, &loadRequest, &loadResponse);
        if (result == RC_Success)
        {
            ImageDescriptor& desc = GetDescriptorMutable();
            //desc.Width = loadResponse.width;
            //desc.Height = loadResponse.height;
            desc.LoadTime = loadResponse.loadTime;
            desc.pluginUsed = loadResponse.pluginUsed;
            /*desc.ImageHandle = loadResponse.handle;
            desc.Bpp = loadResponse.bpp;*/
            SetImageHandle(loadResponse.handle);
            QueryImageInfo();
        }
        return result;
    }
 }
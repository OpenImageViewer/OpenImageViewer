#pragma once

#include <IImagePlugin.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace OIV
{
    struct ImageFormatFilter
    {
        std::wstring description;
        std::vector<std::wstring> extensions;
    };

    struct ImageFormatCatalog
    {
        std::vector<ImageFormatFilter> readFilters;
        std::vector<ImageFormatFilter> writeFilters;
        std::set<std::wstring> knownFileTypesSet;
        std::wstring knownFileTypes;
        std::wstring defaultSaveFileExtension = L"png";
        int16_t defaultSaveFileFormatIndex = 0;
    };

    class ImageFormatCatalogPolicy
    {
      public:
        static ImageFormatCatalog Build(const std::vector<IMCodec::PluginProperties>& codecsInfo);
    };
}

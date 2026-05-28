#pragma once

#include <LLUtils/StringDefs.h>

#include <IImagePlugin.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace OIV
{
    struct ImageFormatFilter
    {
        LLUtils::native_string_type description;
        std::vector<LLUtils::native_string_type> extensions;
    };

    struct ImageFormatCatalog
    {
        std::vector<ImageFormatFilter> readFilters;
        std::vector<ImageFormatFilter> writeFilters;
        std::set<LLUtils::native_string_type> knownFileTypesSet;
        LLUtils::native_string_type knownFileTypes;
        LLUtils::native_string_type defaultSaveFileExtension = LLUTILS_TEXT("png");
        int16_t defaultSaveFileFormatIndex                   = 0;
    };

    class ImageFormatCatalogPolicy
    {
      public:

        static ImageFormatCatalog Build(const std::vector<IMCodec::PluginProperties>& codecsInfo);
    };
}  // namespace OIV

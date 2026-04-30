#include <oivappcore/ImageFormatCatalogPolicy.h>

#include <LLUtils/StringUtility.h>

#include <sstream>

namespace OIV
{
    namespace
    {
        bool HasCapability(IMCodec::CodecCapabilities capabilities, IMCodec::CodecCapabilities capability)
        {
            return (capabilities & capability) == capability;
        }

        void RemoveTrailingSeparator(std::wstring& value)
        {
            if (!value.empty())
                value.erase(value.length() - 1, 1);
        }
    }

    ImageFormatCatalog ImageFormatCatalogPolicy::Build(const std::vector<IMCodec::PluginProperties>& codecsInfo)
    {
        ImageFormatCatalog catalog;

        catalog.readFilters.push_back({L"All files (*.*)", {L"*.*"}});
        catalog.readFilters.push_back({L"All supported image formats", {}});

        const size_t allFormatsIndex = catalog.readFilters.size() - 1;

        for (const IMCodec::PluginProperties& codecInfo : codecsInfo)
        {
            for (const IMCodec::ExtensionCollection& extensionCollection : codecInfo.extensionCollection)
            {
                if (HasCapability(codecInfo.capabilities, IMCodec::CodecCapabilities::Decode))
                {
                    catalog.readFilters.push_back({});
                    ImageFormatFilter& readFilter = catalog.readFilters.back();

                    std::wstring readDialogDescription;
                    for (const std::wstring& extension : extensionCollection.listExtensions)
                    {
                        readDialogDescription += LLUtils::StringUtility::ToUpper(extension) + L'/';
                        const std::wstring lowercaseExtension = LLUtils::StringUtility::ToLower(extension);
                        catalog.knownFileTypesSet.insert(lowercaseExtension);
                        readFilter.extensions.push_back(L"*." + lowercaseExtension);
                        catalog.readFilters.at(allFormatsIndex).extensions.push_back(L"*." + lowercaseExtension);
                    }

                    RemoveTrailingSeparator(readDialogDescription);
                    readFilter.description = readDialogDescription + L" - " + extensionCollection.description;
                }

                if (HasCapability(codecInfo.capabilities, IMCodec::CodecCapabilities::Encode) &&
                    !HasCapability(codecInfo.capabilities, IMCodec::CodecCapabilities::BulkCodec))
                {
                    catalog.writeFilters.push_back({});
                    ImageFormatFilter& writeFilter = catalog.writeFilters.back();

                    std::wstring saveDialogDescription;
                    for (const std::wstring& extension : extensionCollection.listExtensions)
                    {
                        saveDialogDescription += LLUtils::StringUtility::ToUpper(extension) + L'/';
                        const std::wstring lowercaseExtension = LLUtils::StringUtility::ToLower(extension);

                        if (writeFilter.extensions.empty())
                            writeFilter.extensions.push_back(L"*." + lowercaseExtension);

                        if (lowercaseExtension == catalog.defaultSaveFileExtension)
                            catalog.defaultSaveFileFormatIndex = static_cast<int16_t>(catalog.writeFilters.size());
                    }

                    RemoveTrailingSeparator(saveDialogDescription);
                    writeFilter.description = saveDialogDescription + L" - " + extensionCollection.description;
                }
            }
        }

        std::wstringstream knownFileTypes;
        for (const std::wstring& knownExtension : catalog.knownFileTypesSet)
            knownFileTypes << knownExtension << L';';

        catalog.knownFileTypes = knownFileTypes.str();
        RemoveTrailingSeparator(catalog.knownFileTypes);

        return catalog;
    }
}

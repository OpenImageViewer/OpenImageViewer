#include <LLUtils/StringDefs.h>
#include <OIVAppCore/ImageFormatCatalogPolicy.h>

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

        void RemoveTrailingSeparator(LLUtils::native_string_type& value)
        {
            if (!value.empty())
                value.erase(value.length() - 1, 1);
        }
    }  // namespace

    ImageFormatCatalog ImageFormatCatalogPolicy::Build(const std::vector<IMCodec::PluginProperties>& codecsInfo)
    {
        ImageFormatCatalog catalog;

        catalog.readFilters.push_back({LLUTILS_TEXT("All files (*.*)"), {LLUTILS_TEXT("*.*")}});
        catalog.readFilters.push_back({LLUTILS_TEXT("All supported image formats"), {}});

        const size_t allFormatsIndex = catalog.readFilters.size() - 1;

        for (const IMCodec::PluginProperties& codecInfo : codecsInfo)
        {
            for (const IMCodec::ExtensionCollection& extensionCollection : codecInfo.extensionCollection)
            {
                if (HasCapability(codecInfo.capabilities, IMCodec::CodecCapabilities::Decode))
                {
                    catalog.readFilters.push_back({});
                    ImageFormatFilter& readFilter = catalog.readFilters.back();

                    LLUtils::native_string_type readDialogDescription;
                    for (const LLUtils::native_string_type& extension : extensionCollection.listExtensions)
                    {
                        readDialogDescription += LLUtils::StringUtility::ToUpper(extension) + LLUTILS_TEXT('/');
                        const LLUtils::native_string_type lowercaseExtension = LLUtils::StringUtility::ToLower(
                            extension);
                        catalog.knownFileTypesSet.insert(lowercaseExtension);
                        readFilter.extensions.push_back(LLUTILS_TEXT("*.") + lowercaseExtension);
                        catalog.readFilters.at(allFormatsIndex)
                            .extensions.push_back(LLUTILS_TEXT("*.") + lowercaseExtension);
                    }

                    RemoveTrailingSeparator(readDialogDescription);
                    readFilter.description = readDialogDescription + LLUTILS_TEXT(" - ") +
                                             extensionCollection.description;
                }

                if (HasCapability(codecInfo.capabilities, IMCodec::CodecCapabilities::Encode) &&
                    !HasCapability(codecInfo.capabilities, IMCodec::CodecCapabilities::BulkCodec))
                {
                    catalog.writeFilters.push_back({});
                    ImageFormatFilter& writeFilter = catalog.writeFilters.back();

                    LLUtils::native_string_type saveDialogDescription;
                    for (const LLUtils::native_string_type& extension : extensionCollection.listExtensions)
                    {
                        saveDialogDescription += LLUtils::StringUtility::ToUpper(extension) + LLUTILS_TEXT('/');
                        const LLUtils::native_string_type lowercaseExtension = LLUtils::StringUtility::ToLower(
                            extension);

                        if (writeFilter.extensions.empty())
                            writeFilter.extensions.push_back(LLUTILS_TEXT("*.") + lowercaseExtension);

                        if (lowercaseExtension == catalog.defaultSaveFileExtension)
                            catalog.defaultSaveFileFormatIndex = static_cast<int16_t>(catalog.writeFilters.size());
                    }

                    RemoveTrailingSeparator(saveDialogDescription);
                    writeFilter.description = saveDialogDescription + LLUTILS_TEXT(" - ") +
                                              extensionCollection.description;
                }
            }
        }

        LLUtils::native_stringstream knownFileTypes;
        for (const LLUtils::native_string_type& knownExtension : catalog.knownFileTypesSet)
            knownFileTypes << knownExtension << LLUTILS_TEXT(';');

        catalog.knownFileTypes = knownFileTypes.str();
        RemoveTrailingSeparator(catalog.knownFileTypes);

        return catalog;
    }
}  // namespace OIV

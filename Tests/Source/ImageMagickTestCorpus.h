#pragma once

#include <LLUtils/StringDefs.h>

#include <cstddef>
#include <filesystem>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace OIV::Tests
{
    struct GeneratedImage
    {
        std::string format;
        std::string variant;
        std::filesystem::path path;
        std::string expectedLoadStatus = "loaded";
        std::string expectedTexelFormat;
        std::vector<std::string> expectedSubImageTexelFormats;
    };

    struct GeneratedCorpus
    {
        std::filesystem::path folder;
        std::vector<GeneratedImage> validImages;
        std::vector<GeneratedImage> badImages;
        std::set<LLUtils::native_string_type> extensions;
        LLUtils::native_string_type extensionList;
    };

    const GeneratedCorpus& EnsureImageMagickCorpus();
    GeneratedCorpus BuildLoadableImageMagickCorpus();
    std::vector<LLUtils::native_string_type> BuildBrowsingFolderFileList(const GeneratedCorpus& corpus);
    std::vector<LLUtils::native_string_type> FindConsecutiveValidFiles(const GeneratedCorpus& corpus,
                                                                       std::size_t count);
    std::pair<LLUtils::native_string_type, LLUtils::native_string_type> FindValidFileBeforeBadFile(
        const GeneratedCorpus& corpus);
    std::string NarrowPath(const std::filesystem::path& path);
}  // namespace OIV::Tests

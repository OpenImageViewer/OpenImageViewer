#pragma once

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
        std::set<std::wstring> extensions;
        std::wstring extensionList;
    };

    const GeneratedCorpus& EnsureImageMagickCorpus();
    std::vector<std::wstring> BuildBrowsingFolderFileList(const GeneratedCorpus& corpus);
    std::vector<std::wstring> FindConsecutiveValidFiles(const GeneratedCorpus& corpus, std::size_t count);
    std::pair<std::wstring, std::wstring> FindValidFileBeforeBadFile(const GeneratedCorpus& corpus);
    std::string NarrowPath(const std::filesystem::path& path);
}  // namespace OIV::Tests

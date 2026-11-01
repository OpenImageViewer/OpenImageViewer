#include "ImageMagickBrowsingCorpus.h"

#include <catch2/catch_all.hpp>
#include <OIVShared/FileSorter.h>

#include <algorithm>

namespace OIV::Tests
{
    namespace
    {
        std::set<LLUtils::native_string_type> BuildBadFileSet(const GeneratedCorpus& corpus)
        {
            std::set<LLUtils::native_string_type> badFiles;
            for (const auto& image : corpus.badImages)
                badFiles.insert(image.path.native());
            return badFiles;
        }

    }  // namespace

    std::vector<LLUtils::native_string_type> BuildBrowsingFolderFileList(const GeneratedCorpus& corpus)
    {
        std::vector<LLUtils::native_string_type> result;
        for (const auto& image : corpus.validImages)
            result.push_back(image.path.native());
        for (const auto& image : corpus.badImages)
            result.push_back(image.path.native());

        OIV::FileSorter sorter;
        std::sort(result.begin(), result.end(), sorter);
        return result;
    }

    std::vector<LLUtils::native_string_type> FindConsecutiveValidFiles(const GeneratedCorpus& corpus, std::size_t count)
    {
        const auto allFiles = BuildBrowsingFolderFileList(corpus);
        const auto badFiles = BuildBadFileSet(corpus);

        for (auto first = allFiles.begin(); first != allFiles.end(); ++first)
        {
            std::vector<LLUtils::native_string_type> window;
            for (auto it = first; it != allFiles.end() && window.size() < count; ++it)
            {
                if (badFiles.contains(*it))
                    break;
                window.push_back(*it);
            }

            if (window.size() == count)
                return window;
        }

        FAIL("Could not find " << count << " consecutive valid ImageMagick corpus files");
        return {};
    }

    std::pair<LLUtils::native_string_type, LLUtils::native_string_type> FindValidFileBeforeBadFile(
        const GeneratedCorpus& corpus)
    {
        const auto allFiles = BuildBrowsingFolderFileList(corpus);
        const auto badFiles = BuildBadFileSet(corpus);

        for (std::size_t i = 1; i < allFiles.size(); ++i)
        {
            if (!badFiles.contains(allFiles[i - 1]) && badFiles.contains(allFiles[i]))
                return {allFiles[i - 1], allFiles[i]};
        }

        FAIL("Could not find a valid ImageMagick corpus file immediately before a bad file");
        return {};
    }
}  // namespace OIV::Tests

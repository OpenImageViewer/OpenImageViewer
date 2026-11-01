#pragma once

#include "ImageMagickTestCorpus.h"

namespace OIV::Tests
{
    std::vector<LLUtils::native_string_type> BuildBrowsingFolderFileList(const GeneratedCorpus& corpus);
    std::vector<LLUtils::native_string_type> FindConsecutiveValidFiles(const GeneratedCorpus& corpus,
                                                                       std::size_t count);
    std::pair<LLUtils::native_string_type, LLUtils::native_string_type> FindValidFileBeforeBadFile(
        const GeneratedCorpus& corpus);
}  // namespace OIV::Tests

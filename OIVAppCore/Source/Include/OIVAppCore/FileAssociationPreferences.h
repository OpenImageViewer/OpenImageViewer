#pragma once
#include <OIVAppCore/FileAssociationPolicy.h>

namespace OIV
{
    // Executable locations must already be normalized by FileAssociationPolicy::ExecutableLocation.
    // The caller coordinates reads and writes with the session lock. Invalid data and I/O failures throw.
    bool IsAssociationPromptSuppressed(const std::filesystem::path& configDirectory,
                                       const LLUtils::native_string_type& executableLocation);
    // AskNextTime makes no file changes. Associate and SuppressForCopy persist the location.
    void SaveAssociationPromptChoice(const std::filesystem::path& configDirectory,
                                     const LLUtils::native_string_type& executableLocation,
                                     FileAssociationChoice choice = FileAssociationChoice::SuppressForCopy);
}  // namespace OIV

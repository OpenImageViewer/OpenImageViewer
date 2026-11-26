#pragma once

#include <OIVAppCore/FileAssociationModel.h>

#include <set>
#include <stop_token>
#include <string>

namespace OIV::Linux
{
    // Uses the supplied config directory and the current user's XDG application directory. Unknown desktop MIME types
    // are omitted rather than claiming generic binary files. Call from FileAssociationSession with its coordination
    // lock held.
    FileAssociationNotice ManageFileAssociations(const std::filesystem::path& configDirectory,
                                                 const std::set<std::string>& extensions, const std::string& executable,
                                                 FileAssociationAction action, std::stop_token stop = {},
                                                 bool selective = false);
    std::vector<FileAssociationEntry> ReadFileAssociations(const std::set<std::string>& extensions,
                                                           const LLUtils::native_string_type& executable);
    // Caller holds the association coordination lock. Only requested defaults may change.
    std::string UpdateFileAssociations(const std::filesystem::path& configDirectory,
                                       const std::vector<FileAssociationEntry>& entries,
                                       const LLUtils::native_string_type& executable, std::stop_token stop);

}  // namespace OIV::Linux

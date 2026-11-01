#pragma once
#include <Windows.h>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace OIV::Win32
{
    inline constexpr wchar_t ImageAssociationProgId[] = L"OIViewer.Image";

    std::expected<std::wstring, LSTATUS> GetAssociationUserSid();
    // Timestamp is the UserChoice key's last-write FILETIME. The hash uses its UTC minute.
    std::expected<std::wstring, LSTATUS> GenerateUserChoiceHash(std::wstring_view extension, std::wstring_view sid,
                                                                std::wstring_view progId, uint64_t timestamp);

    // Extensions include the leading dot. The old key is retained for undo. A successful result reports whether
    // this call installed a new choice, so verification never rolls back an unrelated existing choice.
    std::expected<bool, LSTATUS> SetDefaultFileAssociation(HKEY userRoot, const std::wstring& extension,
                                                           std::wstring_view sid);
    // The caller verifies that OIViewer.Image and ownedApplicationProgId belong to the running executable.
    // Restore only those choices. Later selections of other applications or viewer copies take precedence.
    LSTATUS RemoveDefaultFileAssociation(HKEY userRoot, const std::wstring& extension,
                                         std::wstring_view ownedApplicationProgId);
}  // namespace OIV::Win32

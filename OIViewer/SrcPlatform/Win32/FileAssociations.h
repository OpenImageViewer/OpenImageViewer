#pragma once
#include <OIVAppCore/FileAssociationPolicy.h>
#include <Windows.h>
#include <set>
#include <stop_token>
#include <string>

namespace OIV::Win32
{
    LSTATUS RegisterFileAssociations(HKEY userRoot, const std::wstring& executable,
                                     const std::set<std::wstring>& extensions, std::stop_token stop);
    struct AssociationQueryResult
    {
        FileAssociationStatus status{FileAssociationStatus::Unknown};
        HRESULT error{S_OK};
    };
    AssociationQueryResult QueryFileAssociation(const std::wstring& extension, const std::wstring& executable);
    // Called by FileAssociationSession with its coordination lock held.
    void ManageFileAssociations(const std::set<std::wstring>& extensions, const std::wstring& executable,
                                const FileAssociationNotify& notify, std::stop_token stop,
                                FileAssociationAction action);
}  // namespace OIV::Win32

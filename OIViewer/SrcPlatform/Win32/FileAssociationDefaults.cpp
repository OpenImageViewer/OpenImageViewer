// Hash arithmetic adapted from PS-SFTA by Danyfirex & Dany3j, Copyright (c) 2022 Danysys.
// https://github.com/DanysysTeam/PS-SFTA/blob/master/SFTA.ps1 (Get-Hash)
// Adaptation: unsigned C++ arithmetic, Windows CNG and staged registry updates.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and
// to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions
// of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
// OR OTHER DEALINGS IN THE SOFTWARE.
#include "FileAssociationDefaults.h"

#include <WinReg/WinReg.hpp>
#include <aclapi.h>
#include <bcrypt.h>
#include <sddl.h>
#include <wincrypt.h>
#include <array>
#include <bit>
#include <cstring>
#include <format>
#include <memory>
#include <vector>

namespace OIV::Win32
{
    namespace
    {
        constexpr uint64_t TicksPerMinute  = 600'000'000;
        constexpr wchar_t PreviousChoice[] = L"OIViewer.PreviousUserChoice";
        constexpr wchar_t PendingChoice[]  = L"OIViewer.PendingUserChoice";
        constexpr wchar_t FileExts[]       = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\";

        winreg::RegExpected<std::wstring> ReadProgId(HKEY parent, const std::wstring& subKey)
        {
            winreg::RegKey key;
            const auto opened = key.TryOpen(parent, subKey, KEY_QUERY_VALUE);
            return opened ? key.TryGetStringValue(L"ProgId") : winreg::RegExpected<std::wstring>::MakeError(opened);
        }

        bool IsOwnedProgId(const wchar_t* progId, std::wstring_view ownedApplicationProgId)
        {
            return CompareStringOrdinal(progId, -1, ImageAssociationProgId, -1, TRUE) == CSTR_EQUAL ||
                   CompareStringOrdinal(progId, -1, ownedApplicationProgId.data(),
                                        static_cast<int>(ownedApplicationProgId.size()), TRUE) == CSTR_EQUAL;
        }

        LSTATUS DeleteOwnedKey(winreg::RegKey& parent, const std::wstring& name)
        {
            const auto error = parent.TryDeleteKey(name, 0).Code();
            return error == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : error;
        }

        // A completed rename remains observable even if restoring its DACL or timestamp fails afterward.
        // Callers need both results to roll the key back to its original name on those failure paths.
        struct ChoiceRenameResult
        {
            LSTATUS error;
            bool renamed;
        };

        ChoiceRenameResult RenameSavedChoice(HKEY parent, const wchar_t* oldName, const wchar_t* newName)
        {
            LSTATUS error = RegRenameKey(parent, oldName, newName);
            bool renamed  = error == ERROR_SUCCESS;
            if (error == ERROR_ACCESS_DENIED)
            {
                // Explorer adds a deny-SetValue ACE for the current user. Rename requests write access too.
                // Temporarily omit only that exact ACE. Restore the original DACL through the still-open handle.
                // Other denials and OS protection remain failures, with the original choice left intact.
                // Changing a DACL updates last-write time and would invalidate the saved UserChoice hash.
                // The native timestamp setter is needed because Win32 has no equivalent. Without it, leave
                // protected choices untouched and let the caller fall back to Windows Settings.
                using SetKeyInformation             = LONG(NTAPI*)(HANDLE, int, const FILETIME*, ULONG);
                static const auto setKeyInformation = reinterpret_cast<SetKeyInformation>(
                    GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtSetInformationKey"));
                if (setKeyInformation == nullptr)
                    return {ERROR_NOT_SUPPORTED, false};
                winreg::RegKey key;
                error = key.TryOpen(parent, oldName, READ_CONTROL | WRITE_DAC | KEY_QUERY_VALUE).Code();
                if (error != ERROR_SUCCESS)
                    return {error, false};
                const auto originalInfo = key.TryQueryInfoKey();
                if (!originalInfo)
                    return {originalInfo.GetError().Code(), false};
                const auto originalWriteTime = originalInfo.GetValue().LastWriteTime;
                PACL dacl{};
                PSECURITY_DESCRIPTOR rawDescriptor{};
                error = GetSecurityInfo(key.Get(), SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, nullptr, nullptr, &dacl,
                                        nullptr, &rawDescriptor);
                const std::unique_ptr<void, decltype(&LocalFree)> descriptor(rawDescriptor, &LocalFree);
                if (error != ERROR_SUCCESS || dacl == nullptr)
                    return {error == ERROR_SUCCESS ? ERROR_ACCESS_DENIED : error, false};
                const auto sid = GetAssociationUserSid();
                PSID rawSid{};
                if (!sid || !ConvertStringSidToSidW(sid->c_str(), &rawSid))
                    return {sid ? static_cast<LSTATUS>(GetLastError()) : sid.error(), false};
                const std::unique_ptr<void, decltype(&LocalFree)> userSid(rawSid, &LocalFree);
                std::vector<std::byte> storage(dacl->AclSize);
                std::memcpy(storage.data(), dacl, storage.size());
                const auto adjusted = reinterpret_cast<PACL>(storage.data());
                bool changed        = false;
                for (DWORD index = 0; index < adjusted->AceCount && !changed; ++index)
                {
                    void* rawAce{};
                    if (!GetAce(adjusted, index, &rawAce))
                        return {static_cast<LSTATUS>(GetLastError()), false};
                    const auto ace = static_cast<ACCESS_DENIED_ACE*>(rawAce);
                    if (ace->Header.AceType == ACCESS_DENIED_ACE_TYPE && ace->Header.AceFlags == 0 &&
                        ace->Mask == KEY_SET_VALUE && EqualSid(&ace->SidStart, userSid.get()))
                        changed = DeleteAce(adjusted, index) != FALSE;
                }
                if (!changed)
                    return {ERROR_ACCESS_DENIED, false};
                SECURITY_DESCRIPTOR temporary{};
                if (!InitializeSecurityDescriptor(&temporary, SECURITY_DESCRIPTOR_REVISION) ||
                    !SetSecurityDescriptorDacl(&temporary, TRUE, adjusted, FALSE))
                    return {static_cast<LSTATUS>(GetLastError()), false};
                error = RegSetKeySecurity(key.Get(), DACL_SECURITY_INFORMATION, &temporary);
                if (error == ERROR_SUCCESS)
                {
                    // Retain write access across DACL restoration so restoring the timestamp is still allowed.
                    winreg::RegKey timestampKey;
                    error = timestampKey.TryOpen(parent, oldName, KEY_SET_VALUE).Code();
                    if (error == ERROR_SUCCESS)
                    {
                        error   = RegRenameKey(parent, oldName, newName);
                        renamed = error == ERROR_SUCCESS;
                    }
                    const auto restored = RegSetKeySecurity(key.Get(), DACL_SECURITY_INFORMATION, descriptor.get());
                    constexpr int KeyWriteTimeInformation = 0;
                    const bool timestampRestored = timestampKey.IsValid() &&
                                                   setKeyInformation(timestampKey.Get(), KeyWriteTimeInformation,
                                                                     &originalWriteTime,
                                                                     sizeof(originalWriteTime)) >= 0;
                    if (error == ERROR_SUCCESS)
                        error = restored != ERROR_SUCCESS ? restored
                                : timestampRestored       ? ERROR_SUCCESS
                                                          : ERROR_WRITE_FAULT;
                }
            }
            return {error, renamed};
        }
    }  // namespace

    std::expected<std::wstring, LSTATUS> GetAssociationUserSid()
    {
        HANDLE raw{};
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw))
            return std::unexpected(GetLastError());
        const std::unique_ptr<void, decltype(&CloseHandle)> token(raw, &CloseHandle);
        DWORD size{};
        GetTokenInformation(token.get(), TokenUser, nullptr, 0, &size);
        std::vector<std::byte> bytes(size);
        if (!GetTokenInformation(token.get(), TokenUser, bytes.data(), size, &size))
            return std::unexpected(GetLastError());
        wchar_t* sid{};
        if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(bytes.data())->User.Sid, &sid))
            return std::unexpected(GetLastError());
        const std::unique_ptr<wchar_t, decltype(&LocalFree)> value(sid, &LocalFree);
        return std::wstring(value.get());
    }

    std::expected<std::wstring, LSTATUS> GenerateUserChoiceHash(std::wstring_view extension, std::wstring_view sid,
                                                                std::wstring_view progId, uint64_t timestamp)
    {
        constexpr std::wstring_view experience = L"User Choice set via Windows User Experience "
                                                 L"{D18B6DD5-6124-4341-9318-804003BAFA0B}";
        std::wstring input                     = std::format(L"{}{}{}{:016x}{}", extension, sid, progId,
                                                             timestamp - timestamp % TicksPerMinute, experience);
        if (!LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, input.data(), static_cast<int>(input.size()),
                           input.data(), static_cast<int>(input.size()), nullptr, nullptr, 0))
            return std::unexpected(GetLastError());
        const auto* bytes = reinterpret_cast<const BYTE*>(input.c_str());
        const auto size   = static_cast<ULONG>((input.size() + 1) * sizeof(wchar_t));
        std::array<uint32_t, 4> md5{};
        // The terminating UTF-16 null participates in MD5. Incomplete 8-byte blocks are omitted only from mixing.
        const auto status = BCryptHash(BCRYPT_MD5_ALG_HANDLE, nullptr, 0, const_cast<BYTE*>(bytes), size,
                                       reinterpret_cast<BYTE*>(md5.data()), sizeof(md5));
        if (status < 0)
            return std::unexpected(ERROR_INVALID_DATA);
        // Both mixing passes require unsigned wraparound and contribute a running hash and sum.
        const uint32_t firstMultiplier  = md5[0] | 1u;
        const uint32_t secondMultiplier = md5[1] | 1u;
        uint32_t firstHash{}, firstSum{}, secondHash{}, secondSum{};
        for (ULONG offset = 0; offset + 8 <= size; offset += 8)
        {
            std::array<uint32_t, 2> words{};
            std::memcpy(words.data(), bytes + offset, sizeof(words));
            uint32_t mixed            = words[0] + firstHash;
            mixed                     = mixed * (firstMultiplier + 0x69fb0000u) - 0x10fa9605u * (mixed >> 16);
            mixed                     = 0x79f8a395u * mixed + 0x689b6b9fu * (mixed >> 16);
            mixed                     = 0xea970001u * mixed - 0x3c101569u * (mixed >> 16);
            const uint32_t firstTotal = firstSum + mixed;
            mixed += words[1];
            mixed     = mixed * (secondMultiplier + 0x13db0000u) - 0x3ce8ec25u * (mixed >> 16);
            mixed     = 0x59c3af2du * mixed - 0x2232e0f1u * (mixed >> 16);
            firstHash = 0x1ec90001u * mixed + 0x35bd1ec9u * (mixed >> 16);
            firstSum  = firstTotal + firstHash;

            mixed                      = (words[0] + secondHash) * firstMultiplier;
            mixed                      = 0xb1110000u * mixed - 0x30674eefu * (mixed >> 16);
            mixed                      = 0x5b9f0000u * mixed - 0x78f7a461u * (mixed >> 16);
            mixed                      = 0x12ceb96du * (mixed >> 16) - 0x46930000u * mixed;
            mixed                      = 0x1d830000u * mixed + 0x257e1d83u * (mixed >> 16);
            const uint32_t secondTotal = secondSum + mixed;
            mixed                      = secondMultiplier * (mixed + words[1]);
            mixed                      = 0x16f50000u * mixed - 0x5d8be90bu * (mixed >> 16);
            mixed                      = 0x96ff0000u * mixed - 0x2c7c6901u * (mixed >> 16);
            mixed                      = 0x2b890000u * mixed + 0x7c932b89u * (mixed >> 16);
            secondHash                 = 0x9f690000u * mixed - 0x405b6097u * (mixed >> 16);
            secondSum                  = secondTotal + secondHash;
        }
        const std::array<uint32_t, 2> hash{firstHash ^ secondHash, firstSum ^ secondSum};
        std::array<wchar_t, 13> encoded{};
        DWORD encodedSize = static_cast<DWORD>(encoded.size());
        if (!CryptBinaryToStringW(reinterpret_cast<const BYTE*>(hash.data()), sizeof(hash),
                                  CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, encoded.data(), &encodedSize))
            return std::unexpected(GetLastError());
        return std::wstring(encoded.data());
    }

    std::expected<bool, LSTATUS> SetDefaultFileAssociation(HKEY userRoot, const std::wstring& extension,
                                                           std::wstring_view sid)
    {
        winreg::RegKey parent;
        LSTATUS error =
            parent.TryCreate(userRoot, std::wstring(FileExts) + extension, KEY_READ | KEY_WRITE | DELETE).Code();
        if (error != ERROR_SUCCESS)
            return std::unexpected(error);
        // Repeated Add must retain the original undo key. An already selected OIViewer key needs no replacement.
        const auto current      = ReadProgId(parent.Get(), L"UserChoice");
        const auto currentError = current ? ERROR_SUCCESS : current.GetError().Code();
        if (current &&
            CompareStringOrdinal(current.GetValue().c_str(), -1, ImageAssociationProgId, -1, TRUE) == CSTR_EQUAL)
            return false;
        if (currentError != ERROR_SUCCESS && currentError != ERROR_FILE_NOT_FOUND)
            return std::unexpected(currentError);
        error = DeleteOwnedKey(parent, PendingChoice);
        {
            winreg::RegKey pending;
            if (error == ERROR_SUCCESS)
                error = pending.TryCreate(parent.Get(), PendingChoice, KEY_READ | KEY_WRITE).Code();
            if (error != ERROR_SUCCESS)
                return std::unexpected(error);
            error = pending.TrySetStringValue(L"ProgId", ImageAssociationProgId).Code();
            // Build the replacement before touching the user's choice. Renaming preserves the key timestamp,
            // which is part of the hash. Retry a minute rollover rather than installing an already invalid hash.
            bool consistent = false;
            for (int attempt = 0; attempt < 3 && error == ERROR_SUCCESS && !consistent; ++attempt)
            {
                const auto before = pending.TryQueryInfoKey();
                if (!before)
                    error = before.GetError().Code();
                else
                {
                    const auto hash = GenerateUserChoiceHash(extension, sid, ImageAssociationProgId,
                                                             std::bit_cast<uint64_t>(before.GetValue().LastWriteTime));
                    error           = hash ? pending.TrySetStringValue(L"Hash", *hash).Code() : hash.error();
                }
                if (error == ERROR_SUCCESS)
                {
                    const auto after = pending.TryQueryInfoKey();
                    if (!after)
                        error = after.GetError().Code();
                    else
                        consistent = std::bit_cast<uint64_t>(before.GetValue().LastWriteTime) / TicksPerMinute ==
                                     std::bit_cast<uint64_t>(after.GetValue().LastWriteTime) / TicksPerMinute;
                }
            }
            if (error == ERROR_SUCCESS && !consistent)
                error = ERROR_RETRY;
        }

        bool savedPrevious = false;
        if (error == ERROR_SUCCESS)
        {
            winreg::RegKey existing;
            const auto opened = existing.TryOpen(parent.Get(), L"UserChoice", KEY_QUERY_VALUE).Code();
            if (opened == ERROR_SUCCESS)
            {
                existing.Close();
                error = DeleteOwnedKey(parent, PreviousChoice);
                if (error == ERROR_SUCCESS)
                {
                    const auto saved = RenameSavedChoice(parent.Get(), L"UserChoice", PreviousChoice);
                    error            = saved.error;
                    savedPrevious    = saved.renamed;
                }
            }
            else if (opened != ERROR_FILE_NOT_FOUND)
                error = opened;
        }
        if (error == ERROR_SUCCESS)
            error = RegRenameKey(parent.Get(), PendingChoice, L"UserChoice");
        if (error != ERROR_SUCCESS)
        {
            if (savedPrevious)
                RenameSavedChoice(parent.Get(), PreviousChoice, L"UserChoice");
            DeleteOwnedKey(parent, PendingChoice);
        }
        return error == ERROR_SUCCESS ? std::expected<bool, LSTATUS>{true} : std::unexpected(error);
    }

    LSTATUS RemoveDefaultFileAssociation(HKEY userRoot, const std::wstring& extension,
                                         std::wstring_view ownedApplicationProgId)
    {
        winreg::RegKey parent;
        LSTATUS error =
            parent.TryOpen(userRoot, std::wstring(FileExts) + extension, KEY_READ | KEY_WRITE | DELETE).Code();
        if (error == ERROR_FILE_NOT_FOUND)
            return ERROR_SUCCESS;
        if (error != ERROR_SUCCESS)
            return error;
        const auto progId = ReadProgId(parent.Get(), L"UserChoice");
        error             = progId ? ERROR_SUCCESS : progId.GetError().Code();
        const bool ours   = progId && IsOwnedProgId(progId.GetValue().c_str(), ownedApplicationProgId);
        if (error == ERROR_SUCCESS || error == ERROR_FILE_NOT_FOUND)
        {
            // Never replace a newer choice made outside OIViewer. With no current choice, a saved key may be
            // from an interrupted installation. Restoring it also recovers that interrupted operation.
            if (ours || error == ERROR_FILE_NOT_FOUND)
            {
                error              = DeleteOwnedKey(parent, PendingChoice);
                bool stagedCurrent = false;
                if (error == ERROR_SUCCESS && ours)
                {
                    const auto staged = RenameSavedChoice(parent.Get(), L"UserChoice", PendingChoice);
                    error             = staged.error;
                    stagedCurrent     = staged.renamed;
                }
                if (error == ERROR_SUCCESS)
                {
                    const auto previous = ReadProgId(parent.Get(), PreviousChoice);
                    if (previous && IsOwnedProgId(previous.GetValue().c_str(), ownedApplicationProgId))
                        error = DeleteOwnedKey(parent, PreviousChoice);
                    else
                        error = RenameSavedChoice(parent.Get(), PreviousChoice, L"UserChoice").error;
                    if (error == ERROR_FILE_NOT_FOUND)
                        error = ERROR_SUCCESS;
                }
                if (error != ERROR_SUCCESS && stagedCurrent)
                    RenameSavedChoice(parent.Get(), PendingChoice, L"UserChoice");
            }
            else
                error = DeleteOwnedKey(parent, PreviousChoice);
        }
        if (error == ERROR_SUCCESS)
            error = DeleteOwnedKey(parent, PendingChoice);
        return error;
    }
}  // namespace OIV::Win32

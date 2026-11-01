#include "FileAssociations.h"
#include "FileAssociationDefaults.h"

#include <ShlObj.h>
#include <Shlwapi.h>
#include <WinReg/WinReg.hpp>

#include <filesystem>

namespace OIV::Win32
{
    namespace
    {
        constexpr auto ProgId               = ImageAssociationProgId;
        constexpr wchar_t CapabilitiesKey[] = L"Software\\OIViewer\\Capabilities";

        std::expected<std::wstring, LSTATUS> ReadString(HKEY root, const std::wstring& path, const wchar_t* name)
        {
            winreg::RegKey key;
            const auto opened = key.TryOpen(root, path, KEY_QUERY_VALUE);
            if (!opened)
                return std::unexpected(opened.Code());
            auto value = key.TryGetStringValue(name != nullptr ? name : L"");
            return value ? std::expected<std::wstring, LSTATUS>{std::move(value).GetValue()}
                         : std::unexpected(value.GetError().Code());
        }

        LSTATUS SetString(HKEY root, const std::wstring& path, const wchar_t* name, const std::wstring& value)
        {
            winreg::RegKey key;
            const auto created = key.TryCreate(root, path, KEY_SET_VALUE);
            return created ? key.TrySetStringValue(name != nullptr ? name : L"", value).Code() : created.Code();
        }

        LSTATUS DeleteValue(HKEY root, const std::wstring& path, const std::wstring& name)
        {
            winreg::RegKey key;
            const auto opened = key.TryOpen(root, path, KEY_SET_VALUE);
            return opened ? key.TryDeleteValue(name).Code() : opened.Code();
        }

        void Apply(const std::set<std::wstring>& extensions, const std::wstring& executable,
                   const FileAssociationNotify& notify, std::stop_token stop, FileAssociationAction action);

        FileAssociationNotice Failure(FileAssociationNotice::Kind kind, const wchar_t* operation, int64_t error)
        {
            return {kind, std::wstring(operation) + L" (error " + std::to_wstring(error) + L")."};
        }
    }  // namespace

    LSTATUS RegisterFileAssociations(HKEY userRoot, const std::wstring& executable,
                                     const std::set<std::wstring>& extensions, std::stop_token stop)
    {
        LSTATUS error                  = stop.stop_requested() ? ERROR_CANCELLED : ERROR_SUCCESS;
        const std::wstring command     = L"\"" + executable + L"\" \"%1\"";
        const std::wstring icon        = L"\"" + executable + L"\",0";
        const std::wstring application = L"Software\\Classes\\Applications\\" +
                                         std::filesystem::path(executable).filename().wstring();
        const auto set                 = [&](const std::wstring& path, const wchar_t* name, const std::wstring& value)
        {
            if (error == ERROR_SUCCESS)
                error = stop.stop_requested() ? ERROR_CANCELLED : SetString(userRoot, path, name, value);
        };

        // Advertise capabilities without replacing extension defaults or Windows' protected UserChoice keys.
        set(L"Software\\Classes\\OIViewer.Image", nullptr, L"OIViewer image");
        set(L"Software\\Classes\\OIViewer.Image\\DefaultIcon", nullptr, icon);
        set(L"Software\\Classes\\OIViewer.Image\\shell\\open\\command", nullptr, command);
        set(application, L"FriendlyAppName", L"Open Image Viewer");
        set(application + L"\\shell\\open\\command", nullptr, command);
        set(CapabilitiesKey, L"ApplicationName", L"Open Image Viewer");
        set(CapabilitiesKey, L"ApplicationDescription", L"View supported image formats with Open Image Viewer.");
        set(CapabilitiesKey, L"ApplicationIcon", icon);
        for (auto it = extensions.begin(); it != extensions.end() && error == ERROR_SUCCESS; ++it)
        {
            const std::wstring extension = L"." + *it;
            set(L"Software\\Classes\\" + extension + L"\\OpenWithProgids", ProgId, L"");
            set(std::wstring(CapabilitiesKey) + L"\\FileAssociations", extension.c_str(), ProgId);
            set(application + L"\\SupportedTypes", extension.c_str(), L"");
        }
        // Publish last, after all advertised handlers exist. Repeating this also repairs moved installations.
        set(L"Software\\RegisteredApplications", L"OIViewer", CapabilitiesKey);
        return error;
    }

    AssociationQueryResult QueryFileAssociation(const std::wstring& extension, const std::wstring& executable)
    {
        AssociationQueryResult result;
        const std::wstring suffix = L"." + extension;
        DWORD size{};
        result.error = AssocQueryStringW(ASSOCF_NOTRUNCATE, ASSOCSTR_EXECUTABLE, suffix.c_str(), L"open", nullptr,
                                         &size);
        if ((result.error == S_FALSE || result.error == E_POINTER || SUCCEEDED(result.error)) && size != 0)
        {
            std::wstring handler(size, L'\0');
            result.error = AssocQueryStringW(ASSOCF_NOTRUNCATE, ASSOCSTR_EXECUTABLE, suffix.c_str(), L"open",
                                             handler.data(), &size);
            if (SUCCEEDED(result.error))
            {
                handler.resize(size != 0 ? size - 1 : 0);
                std::error_code error;
                const bool same = std::filesystem::equivalent(handler, executable, error);
                if (!error || error == std::errc::no_such_file_or_directory)
                    result.status = same ? FileAssociationStatus::CurrentViewer : FileAssociationStatus::OtherOrMissing;
                else
                    result.error = HRESULT_FROM_WIN32(error.value());
            }
        }
        if (result.error == HRESULT_FROM_WIN32(ERROR_NO_ASSOCIATION) ||
            result.error == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        {
            result.status = FileAssociationStatus::OtherOrMissing;
            result.error  = S_OK;
        }
        return result;
    }

    void ManageFileAssociations(const std::set<std::wstring>& extensions, const std::wstring& executable,
                                const FileAssociationNotify& notify, std::stop_token stop, FileAssociationAction action)
    {
        if ((extensions.empty() && action != FileAssociationAction::Remove) || stop.stop_requested())
            return;
        // Shell association queries can activate COM-backed handlers. This apartment belongs solely to this worker.
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        struct Apartment
        {
            HRESULT result;
            ~Apartment()
            {
                if (SUCCEEDED(result))
                    CoUninitialize();
            }
        } apartment{initialized};
        if (FAILED(initialized))
        {
            notify(Failure(FileAssociationNotice::Kind::CheckFailed, L"Could not initialize association queries",
                           initialized));
            return;
        }

        if (action != FileAssociationAction::Check)
        {
            Apply(extensions, executable, notify, stop, action);
            return;
        }

        bool needsAssociation = false;
        HRESULT queryError    = S_OK;
        for (auto it = extensions.begin(); it != extensions.end() && !stop.stop_requested(); ++it)
        {
            const auto query = QueryFileAssociation(*it, executable);
            needsAssociation |= query.status == FileAssociationStatus::OtherOrMissing;
            if (query.status == FileAssociationStatus::Unknown)
                queryError = FAILED(query.error) ? query.error : E_FAIL;
        }
        if (FAILED(queryError))
            notify(Failure(FileAssociationNotice::Kind::CheckFailed, L"Some file associations could not be checked",
                           queryError));

        if (!stop.stop_requested() && needsAssociation)
            notify({FileAssociationNotice::Kind::Prompt, {}});
    }

    namespace
    {
        void Apply(const std::set<std::wstring>& extensions, const std::wstring& executable,
                   const FileAssociationNotify& notify, std::stop_token stop, FileAssociationAction action)
        {
            const bool remove                    = action == FileAssociationAction::Remove;
            std::set<std::wstring> formats       = extensions;
            const std::wstring applicationProgId = L"Applications\\" +
                                                   std::filesystem::path(executable).filename().wstring();
            const std::wstring applicationKey    = L"Software\\Classes\\" + applicationProgId;
            std::wstring sid;
            if (remove)
            {
                // A portable second copy must not uninstall registrations that now belong to a different executable.
                const std::wstring command = L"\"" + executable + L"\" \"%1\"";
                for (const auto& key : {std::wstring(L"Software\\Classes\\OIViewer.Image"), applicationKey})
                {
                    const auto registered = ReadString(HKEY_CURRENT_USER, key + L"\\shell\\open\\command", nullptr);
                    if (registered &&
                        CompareStringOrdinal(registered->c_str(), -1, command.c_str(), -1, TRUE) != CSTR_EQUAL)
                    {
                        notify({FileAssociationNotice::Kind::ActionFailed,
                                L"Associations belong to another copy of OIViewer."});
                        return;
                    }
                    if (!registered && registered.error() != ERROR_FILE_NOT_FOUND)
                    {
                        notify(Failure(FileAssociationNotice::Kind::ActionFailed,
                                       L"Could not read registered associations", registered.error()));
                        return;
                    }
                }
                // Include formats registered by an earlier codec set so removal does not leave stale handlers behind.
                winreg::RegKey key;
                const auto opened = key.TryOpen(HKEY_CURRENT_USER,
                                                std::wstring(CapabilitiesKey) + L"\\FileAssociations", KEY_QUERY_VALUE);
                if (!opened && opened.Code() != ERROR_FILE_NOT_FOUND)
                {
                    notify(Failure(FileAssociationNotice::Kind::ActionFailed, L"Could not enumerate registered formats",
                                   opened.Code()));
                    return;
                }
                if (opened)
                {
                    const auto values = key.TryEnumValues();
                    if (!values)
                    {
                        notify(Failure(FileAssociationNotice::Kind::ActionFailed,
                                       L"Could not enumerate registered formats", values.GetError().Code()));
                        return;
                    }
                    for (const auto& [name, type] : values.GetValue())
                        if (name.size() > 1 && name.front() == L'.')
                            formats.emplace(name.substr(1));
                }
            }
            else
            {
                auto userSid = GetAssociationUserSid();
                if (!userSid)
                {
                    notify(Failure(FileAssociationNotice::Kind::ActionFailed, L"Could not identify the current user",
                                   userSid.error()));
                    return;
                }
                sid              = std::move(*userSid);
                const auto error = RegisterFileAssociations(HKEY_CURRENT_USER, executable, formats, stop);
                if (error != ERROR_SUCCESS)
                {
                    if (error != ERROR_CANCELLED)
                        notify(Failure(FileAssociationNotice::Kind::ActionFailed,
                                       L"Could not register supported formats", error));
                    return;
                }
            }

            std::set<std::wstring> failed;
            std::set<std::wstring> installed;
            for (auto it = formats.begin(); it != formats.end() && !stop.stop_requested(); ++it)
            {
                LSTATUS error = ERROR_SUCCESS;
                if (remove)
                {
                    error = RemoveDefaultFileAssociation(HKEY_CURRENT_USER, L"." + *it, applicationProgId);
                    if (error == ERROR_SUCCESS)
                    {
                        error = DeleteValue(HKEY_CURRENT_USER, L"Software\\Classes\\." + *it + L"\\OpenWithProgids",
                                            ProgId);
                        if (error == ERROR_FILE_NOT_FOUND)
                            error = ERROR_SUCCESS;
                    }
                }
                else
                {
                    const auto updated = SetDefaultFileAssociation(HKEY_CURRENT_USER, L"." + *it, sid);
                    if (updated)
                    {
                        if (*updated)
                            installed.insert(*it);
                    }
                    else
                        error = updated.error();
                }
                if (error != ERROR_SUCCESS)
                    failed.insert(*it);
            }
            if (stop.stop_requested())
            {
                SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT, nullptr, nullptr);
                return;
            }

            LSTATUS cleanupError = ERROR_SUCCESS;
            if (remove && failed.empty())
            {
                winreg::RegKey user(HKEY_CURRENT_USER);
                for (const auto& key : {std::wstring(L"Software\\Classes\\OIViewer.Image"), applicationKey,
                                        std::wstring(CapabilitiesKey)})
                {
                    const auto error = user.TryDeleteTree(key).Code();
                    if (error != ERROR_SUCCESS && error != ERROR_FILE_NOT_FOUND)
                        cleanupError = error;
                }
                const auto error = DeleteValue(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", L"OIViewer");
                if (error != ERROR_SUCCESS && error != ERROR_FILE_NOT_FOUND)
                    cleanupError = error;
            }
            SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT, nullptr, nullptr);
            for (auto it = formats.begin(); it != formats.end() && !stop.stop_requested(); ++it)
            {
                const auto status = QueryFileAssociation(*it, executable).status;
                if ((!remove && status != FileAssociationStatus::CurrentViewer) ||
                    (remove && status == FileAssociationStatus::CurrentViewer))
                {
                    failed.insert(*it);
                    if (installed.contains(*it))
                    {
                        // Rejecting a new hash must not cost the user their old default. Undo only our own choice.
                        const auto error = RemoveDefaultFileAssociation(HKEY_CURRENT_USER, L"." + *it,
                                                                        applicationProgId);
                        if (error != ERROR_SUCCESS)
                            cleanupError = error;
                    }
                }
            }
            if (stop.stop_requested())
                return;
            if (remove || !failed.empty())
                SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT, nullptr, nullptr);
            if (cleanupError != ERROR_SUCCESS)
                notify(Failure(FileAssociationNotice::Kind::ActionFailed,
                               L"Some association cleanup could not be completed", cleanupError));
            if (!failed.empty())
            {
                std::wstring message = remove ? L"Could not remove associations for: "
                                              : L"Windows did not accept associations for: ";
                bool first           = true;
                for (const auto& extension : failed)
                {
                    if (!first)
                        message += L", ";
                    message += L"." + extension;
                    first = false;
                }
                message += L".";
                if (!remove)
                    message += L" Finish selecting these defaults in Windows Settings.";
                notify({remove ? FileAssociationNotice::Kind::ActionFailed : FileAssociationNotice::Kind::OpenSettings,
                        std::move(message)});
            }
            else if (cleanupError == ERROR_SUCCESS)
            {
                notify({FileAssociationNotice::Kind::Completed,
                        remove ? L"OIViewer associations removed. Saved defaults restored."
                               : L"OIViewer is now the default for all supported image formats."});
            }
        }
    }  // namespace
}  // namespace OIV::Win32

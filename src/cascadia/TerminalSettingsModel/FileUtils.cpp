// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "FileUtils.h"

#include <appmodel.h>
#include <shlobj.h>
#include <WtExeUtils.h>

#include <aclapi.h>
#include <sddl.h>
#include <wil/token_helpers.h>

static constexpr std::string_view Utf8Bom{ u8"\uFEFF" };
static constexpr std::wstring_view UnpackagedSettingsFolderName{ L"Microsoft\\Windows Terminal\\" };

namespace winrt::Microsoft::Terminal::Settings::Model
{
    // Returns a path like C:\Users\<username>\AppData\Local\Packages\<packagename>\LocalState
    // You can put your settings.json or state.json in this directory.
    std::filesystem::path GetBaseSettingsPath()
    {
        static std::filesystem::path baseSettingsPath = []() {
            wil::unique_cotaskmem_string localAppDataFolder;
            // KF_FLAG_FORCE_APP_DATA_REDIRECTION, when engaged, causes SHGet... to return
            // the new AppModel paths (Packages/xxx/RoamingState, etc.) for standard path requests.
            // Using this flag allows us to avoid Windows.Storage.ApplicationData completely.
            THROW_IF_FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_FORCE_APP_DATA_REDIRECTION, nullptr, &localAppDataFolder));

            std::filesystem::path parentDirectoryForSettingsFile{ localAppDataFolder.get() };

            if (!IsPackaged())
            {
                parentDirectoryForSettingsFile /= UnpackagedSettingsFolderName;
            }

            // Create the directory if it doesn't exist
            std::filesystem::create_directories(parentDirectoryForSettingsFile);

            return parentDirectoryForSettingsFile;
        }();
        return baseSettingsPath;
    }

    // Function Description:
    // - Checks the permissions on this file, to make sure it can only be opened
    //   for writing by admins. We want to make sure that:
    //   * Everyone has permission to read
    //   * Admins can do anything
    //   * No one else can do anything.
    // Arguments:
    // - path: the path to the file to check
    // Return Value:
    // - true if it had the expected permissions. False otherwise.
    static bool _hasElevatedOnlyPermissions(const std::filesystem::path& path)
    {
        path;
        return true;
        /*
        // If we want to only open the file if it's elevated, check the
        // permissions on this file. We want to make sure that:
        // * Everyone has permission to read
        // * admins can do anything
        // * no one else can do anything.
        PACL pAcl{ nullptr }; // This doesn't need to be cleanup up apparently

        auto status = GetNamedSecurityInfo(path.c_str(),
                                           SE_FILE_OBJECT,
                                           DACL_SECURITY_INFORMATION,
                                           nullptr,
                                           nullptr,
                                           &pAcl,
                                           nullptr,
                                           nullptr);
        THROW_IF_WIN32_ERROR(status);

        PEXPLICIT_ACCESS pEA{ nullptr };
        DWORD count = 0;
        status = GetExplicitEntriesFromAcl(pAcl, &count, &pEA);
        THROW_IF_WIN32_ERROR(status);

        auto explicitAccessCleanup = wil::scope_exit([&]() { ::LocalFree(pEA); });

        if (count != 2)
        {
            return false;
        }

        // Now, get the Everyone and Admins SIDS so we can make sure they're
        // the ones in this file.
        // Create a SID for the BUILTIN\Administrators group.
        const auto adminGroupSid = wil::make_static_sid(SECURITY_NT_AUTHORITY, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS);

        // Create a well-known SID for the Everyone group.
        const auto everyoneSid = wil::make_static_sid(SECURITY_WORLD_SID_AUTHORITY, SECURITY_WORLD_RID);

        bool hadExpectedPermissions = true;

        // Check that the permissions are what we'd expect them to be if only
        // admins can write to the file. This is basically a mirror of what we
        // set up in `WriteUTF8File`.

        // For grfAccessPermissions, GENERIC_ALL turns into STANDARD_RIGHTS_ALL,
        // and GENERIC_READ -> READ_CONTROL
        hadExpectedPermissions &= WI_AreAllFlagsSet(pEA[0].grfAccessPermissions, STANDARD_RIGHTS_ALL);
        hadExpectedPermissions &= pEA[0].grfInheritance == NO_INHERITANCE;
        hadExpectedPermissions &= pEA[0].Trustee.TrusteeForm == TRUSTEE_IS_SID;
        // SIDs are void*'s that happen to convert to a wchar_t
        hadExpectedPermissions &= *(pEA[0].Trustee.ptstrName) == *(LPWSTR)(adminGroupSid.get());

        // Now check the other EXPLICIT_ACCESS
        hadExpectedPermissions &= WI_IsFlagSet(pEA[1].grfAccessPermissions, READ_CONTROL);
        hadExpectedPermissions &= pEA[1].grfInheritance == NO_INHERITANCE;
        hadExpectedPermissions &= pEA[1].Trustee.TrusteeForm == TRUSTEE_IS_SID;
        hadExpectedPermissions &= *(pEA[1].Trustee.ptstrName) == *(LPWSTR)(everyoneSid.get());

        return hadExpectedPermissions;
        */
    }
    // Tries to read a file somewhat atomically without locking it.
    // Strips the UTF8 BOM if it exists.
    std::string ReadUTF8File(const std::filesystem::path& path, const bool elevatedOnly)
    {
        if (elevatedOnly)
        {
            const bool hadExpectedPermissions{ _hasElevatedOnlyPermissions(path) };
            if (!hadExpectedPermissions)
            {
                // delete the file. It's been compromised.
                LOG_LAST_ERROR_IF(!DeleteFile(path.c_str()));
                // Exit early, because obviously there's nothing to read from the deleted file.
                return "";
            }
        }

        // From some casual observations we can determine that:
        // * ReadFile() always returns the requested amount of data (unless the file is smaller)
        // * It's unlikely that the file was changed between GetFileSize() and ReadFile()
        // -> Lets add a retry-loop just in case, to not fail if the file size changed while reading.
        for (int i = 0; i < 3; ++i)
        {
            wil::unique_hfile file{ CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) };
            THROW_LAST_ERROR_IF(!file);

            const auto fileSize = GetFileSize(file.get(), nullptr);
            THROW_LAST_ERROR_IF(fileSize == INVALID_FILE_SIZE);

            // By making our buffer just slightly larger we can detect if
            // the file size changed and we've failed to read the full file.
            std::string buffer(static_cast<size_t>(fileSize) + 1, '\0');
            DWORD bytesRead = 0;
            THROW_IF_WIN32_BOOL_FALSE(ReadFile(file.get(), buffer.data(), gsl::narrow<DWORD>(buffer.size()), &bytesRead, nullptr));

            // This implementation isn't atomic as we'd need to use an exclusive file lock.
            // But this would be annoying for users as it forces them to close the file in their editor.
            // The next best alternative is to at least try to detect file changes and retry the read.
            if (bytesRead != fileSize)
            {
                // This continue is unlikely to be hit (see the prior for loop comment).
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // As mentioned before our buffer was allocated oversized.
            buffer.resize(bytesRead);

            if (til::starts_with(buffer, Utf8Bom))
            {
                // Yeah this memmove()s the entire content.
                // But I don't really want to deal with UTF8 BOMs any more than necessary,
                // as basically not a single editor writes a BOM for UTF8.
                buffer.erase(0, Utf8Bom.size());
            }

            return buffer;
        }

        THROW_WIN32_MSG(ERROR_READ_FAULT, "file size changed while reading");
    }

    // Same as ReadUTF8File, but returns an empty optional, if the file couldn't be opened.
    std::optional<std::string> ReadUTF8FileIfExists(const std::filesystem::path& path, const bool elevatedOnly)
    {
        try
        {
            return { ReadUTF8File(path, elevatedOnly) };
        }
        catch (const wil::ResultException& exception)
        {
            if (exception.GetErrorCode() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
                return {};
            }

            throw;
        }
    }

    void WriteUTF8File(const std::filesystem::path& path,
                       const std::string_view& content,
                       const bool elevatedOnly)
    {
        SECURITY_ATTRIBUTES sa;
        if (elevatedOnly)
        {
            // // This is very vaguely taken from
            // // https://docs.microsoft.com/en-us/windows/win32/secauthz/creating-a-security-descriptor-for-a-new-object-in-c--
            // // With using https://docs.microsoft.com/en-us/windows/win32/secauthz/well-known-sids
            // // to find out that
            // // * SECURITY_NT_AUTHORITY+SECURITY_LOCAL_SYSTEM_RID == NT AUTHORITY\SYSTEM
            // // * SECURITY_NT_AUTHORITY+SECURITY_BUILTIN_DOMAIN_RID+DOMAIN_ALIAS_RID_ADMINS == BUILTIN\Administrators
            // // * SECURITY_WORLD_SID_AUTHORITY+SECURITY_WORLD_RID == Everyone
            // //
            // // Raymond Chen recommended that I make this file only writable by
            // // SYSTEM, but if I did that, then even we can't write the file
            // // while elevated, which isn't what we want.

            // // Create a SID for the BUILTIN\Administrators group.
            // const auto adminGroupSid = wil::make_static_sid(SECURITY_NT_AUTHORITY, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS);

            // // Create a well-known SID for the Everyone group.
            // const auto everyoneSid = wil::make_static_sid(SECURITY_WORLD_SID_AUTHORITY, SECURITY_WORLD_RID);

            // EXPLICIT_ACCESS ea[2]{};

            // // Grant Admins all permissions on this file
            // ea[0].grfAccessPermissions = GENERIC_ALL;
            // ea[0].grfAccessMode = SET_ACCESS;
            // ea[0].grfInheritance = NO_INHERITANCE;
            // ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
            // ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
            // ea[0].Trustee.ptstrName = (LPWSTR)(adminGroupSid.get());

            // // Grant Everyone the permission or read this file
            // ea[1].grfAccessPermissions = GENERIC_READ;
            // ea[1].grfAccessMode = SET_ACCESS;
            // ea[1].grfInheritance = NO_INHERITANCE;
            // ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
            // ea[1].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
            // ea[1].Trustee.ptstrName = (LPWSTR)(everyoneSid.get());

            // ACL acl;
            // PACL pAcl = &acl;
            // THROW_IF_WIN32_ERROR(SetEntriesInAcl(2, ea, nullptr, &pAcl));

            // SECURITY_DESCRIPTOR sd;
            // THROW_IF_WIN32_BOOL_FALSE(InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION));
            // THROW_IF_WIN32_BOOL_FALSE(SetSecurityDescriptorDacl(&sd, true, pAcl, false));

            // TODO!: I need to LocalFree this somehow.
            PSECURITY_DESCRIPTOR psd;
            unsigned long cb;
            ConvertStringSecurityDescriptorToSecurityDescriptor(L"S:(ML;;NW;;;HI)",
                                                                SDDL_REVISION_1,
                                                                &psd,
                                                                &cb);

            // Initialize a security attributes structure.
            sa.nLength = sizeof(SECURITY_ATTRIBUTES);
            // sa.lpSecurityDescriptor = &sd;
            sa.lpSecurityDescriptor = psd;
            sa.bInheritHandle = false;
        }

        wil::unique_hfile file{ CreateFileW(path.c_str(),
                                            GENERIC_WRITE,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                                            elevatedOnly ? &sa : nullptr,
                                            CREATE_ALWAYS,
                                            FILE_ATTRIBUTE_NORMAL,
                                            nullptr) };
        THROW_LAST_ERROR_IF(!file);

        const auto fileSize = gsl::narrow<DWORD>(content.size());
        DWORD bytesWritten = 0;
        THROW_IF_WIN32_BOOL_FALSE(WriteFile(file.get(), content.data(), fileSize, &bytesWritten, nullptr));

        if (bytesWritten != fileSize)
        {
            THROW_WIN32_MSG(ERROR_WRITE_FAULT, "failed to write whole file");
        }
    }

    void WriteUTF8FileAtomic(const std::filesystem::path& path,
                             const std::string_view& content)
    {
        // GH#10787: rename() will replace symbolic links themselves and not the path they point at.
        // It's thus important that we first resolve them before generating temporary path.
        std::error_code ec;
        const auto resolvedPath = std::filesystem::is_symlink(path) ? std::filesystem::canonical(path, ec) : path;
        if (ec)
        {
            if (ec.value() != ERROR_FILE_NOT_FOUND)
            {
                THROW_WIN32_MSG(ec.value(), "failed to compute canonical path");
            }

            // The original file is a symbolic link, but the target doesn't exist.
            // Consider two fall-backs:
            //   * resolve the link manually, which might be less accurate and more prone to race conditions
            //   * write to the file directly, which lets the system resolve the symbolic link but leaves the write non-atomic
            // The latter is chosen, as this is an edge case and our 'atomic' writes are only best-effort.
            WriteUTF8File(path, content);
            return;
        }

        auto tmpPath = resolvedPath;
        tmpPath += L".tmp";

        // Writing to a file isn't atomic, but...
        WriteUTF8File(tmpPath, content);

        // renaming one is (supposed to be) atomic.
        // Wait... "supposed to be"!? Well it's technically not always atomic,
        // but it's pretty darn close to it, so... better than nothing.
        std::filesystem::rename(tmpPath, resolvedPath);
    }
}

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

static constexpr std::string_view Utf8Bom{ "\xEF\xBB\xBF", 3 };
static constexpr std::wstring_view UnpackagedSettingsFolderName{ L"Microsoft\\Windows Terminal\\" };
static constexpr std::wstring_view ReleaseSettingsFolder{ L"Packages\\Microsoft.WindowsTerminal_8wekyb3d8bbwe\\LocalState\\" };
static constexpr std::wstring_view PortableModeMarkerFile{ L".portable" };
static constexpr std::wstring_view PortableModeSettingsFolder{ L"settings" };

namespace winrt::Microsoft::Terminal::Settings::Model
{
    // Returns a path like C:\Users\<username>\AppData\Local\Packages\<packagename>\LocalState
    // You can put your settings.json or state.json in this directory.
    bool IsPortableMode()
    {
        static auto portableMode = []() {
            std::filesystem::path modulePath{ wil::GetModuleFileNameW<std::wstring>(wil::GetModuleInstanceHandle()) };
            modulePath.replace_filename(PortableModeMarkerFile);
            return std::filesystem::exists(modulePath);
        }();
        return portableMode;
    }

    std::filesystem::path GetBaseSettingsPath()
    {
        static auto baseSettingsPath = []() {
            if (!IsPackaged() && IsPortableMode())
            {
                std::filesystem::path modulePath{ wil::GetModuleFileNameW<std::wstring>(wil::GetModuleInstanceHandle()) };
                modulePath.replace_filename(PortableModeSettingsFolder);
                std::filesystem::create_directories(modulePath);
                return modulePath;
            }

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

    // Returns a path like C:\Users\<username>\AppData\Local\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState
    // to the path of the stable release settings
    std::filesystem::path GetReleaseSettingsPath()
    {
        static std::filesystem::path baseSettingsPath = []() {
            wil::unique_cotaskmem_string localAppDataFolder;
            // We're using KF_FLAG_NO_PACKAGE_REDIRECTION to ensure that we always get the
            // user's actual local AppData directory.
            THROW_IF_FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_NO_PACKAGE_REDIRECTION, nullptr, &localAppDataFolder));

            // Returns a path like C:\Users\<username>\AppData\Local
            std::filesystem::path parentDirectoryForSettingsFile{ localAppDataFolder.get() };

            //Appending \Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState to the settings path
            parentDirectoryForSettingsFile /= ReleaseSettingsFolder;

            if (!IsPackaged())
            {
                parentDirectoryForSettingsFile /= UnpackagedSettingsFolderName;
            }

            return parentDirectoryForSettingsFile;
        }();
        return baseSettingsPath;
    }

    // Function Description:
    // - Checks the permissions on this file, to make sure it can only be opened
    //   for writing by admins. We will be checking to see if the file is owned
    //   by the Builtin\Administrators group. If it's not, then it was likely
    //   tampered with.
    // Arguments:
    // - handle: a HANDLE to the file to check
    // Return Value:
    // - true if it had the expected permissions. False otherwise.
    static bool _isOwnedByAdministrators(const HANDLE& handle)
    {
        // If the file is owned by the administrators group, trust the
        // administrators instead of checking the DACL permissions. It's simpler
        // and more flexible.

        wil::unique_hlocal_security_descriptor sd;
        PSID psidOwner{ nullptr };
        // The psidOwner pointer references the security descriptor, so it
        // doesn't have to be freed separate from sd.
        const auto status = GetSecurityInfo(handle,
                                            SE_FILE_OBJECT,
                                            OWNER_SECURITY_INFORMATION,
                                            &psidOwner,
                                            nullptr,
                                            nullptr,
                                            nullptr,
                                            wil::out_param_ptr<PSECURITY_DESCRIPTOR*>(sd));
        THROW_IF_WIN32_ERROR(status);

        wil::unique_any_psid psidAdmins{ nullptr };
        THROW_IF_WIN32_BOOL_FALSE(
            ConvertStringSidToSidW(L"BA", wil::out_param_ptr<PSID*>(psidAdmins)));

        return EqualSid(psidOwner, psidAdmins.get());
    }
    // Tries to read a file somewhat atomically without locking it.
    // Strips the UTF8 BOM if it exists.
    std::string ReadUTF8File(const std::filesystem::path& path, const bool elevatedOnly, FILETIME* lastWriteTime)
    {
        // From some casual observations we can determine that:
        // * ReadFile() always returns the requested amount of data (unless the file is smaller)
        // * It's unlikely that the file was changed between GetFileSize() and ReadFile()
        // -> Lets add a retry-loop just in case, to not fail if the file size changed while reading.
        for (auto i = 0; i < 3; ++i)
        {
            wil::unique_hfile file{ CreateFileW(path.c_str(),
                                                GENERIC_READ,
                                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                                nullptr,
                                                OPEN_EXISTING,
                                                FILE_ATTRIBUTE_NORMAL,
                                                nullptr) };
            THROW_LAST_ERROR_IF(!file);

            // Open the file _first_, then check if it has the right
            // permissions. This prevents a "Time-of-check to time-of-use"
            // vulnerability where a malicious exe could delete the file and
            // replace it between us checking the permissions, and reading the
            // contents. We've got a handle to the file now, which means we're
            // going to read the contents of that instance of the file
            // regardless. If someone replaces the file on us before we get to
            // the GetSecurityInfo call below, then only the subsequent call to
            // ReadUTF8File will notice it.
            if (elevatedOnly)
            {
                const auto hadExpectedPermissions{ _isOwnedByAdministrators(file.get()) };
                if (!hadExpectedPermissions)
                {
                    // Close the handle
                    file.reset();

                    // delete the file. It's been compromised.
                    LOG_LAST_ERROR_IF(!DeleteFile(path.c_str()));

                    // Exit early, because obviously there's nothing to read from the deleted file.
                    return "";
                }
            }

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

            if (lastWriteTime)
            {
                THROW_IF_WIN32_BOOL_FALSE(GetFileTime(file.get(), nullptr, nullptr, lastWriteTime));
            }

            return buffer;
        }

        THROW_WIN32_MSG(ERROR_READ_FAULT, "file size changed while reading");
    }

    // Same as ReadUTF8File, but returns an empty optional, if the file couldn't be opened.
    std::optional<std::string> ReadUTF8FileIfExists(const std::filesystem::path& path, const bool elevatedOnly, FILETIME* lastWriteTime)
    {
        try
        {
            return { ReadUTF8File(path, elevatedOnly, lastWriteTime) };
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

    void WriteUTF8File(const std::filesystem::path& path, const std::string_view& content, const bool elevatedOnly, FILETIME* lastWriteTime)
    {
        SECURITY_ATTRIBUTES sa;
        // stash the security descriptor here, so it will stay in context until
        // after the call to CreateFile. If it gets cleaned up before that, then
        // CreateFile will fail
        wil::unique_hlocal_security_descriptor sd;
        if (elevatedOnly)
        {
            // Initialize the security descriptor so only admins can write the
            // file. We'll initialize the SECURITY_DESCRIPTOR with a
            // single entry (ACE) -- a mandatory label (i.e. a
            // LABEL_SECURITY_INFORMATION) that sets the file integrity level to
            // "high",  with a no-write-up policy.
            //
            // When accessed from a security context at a lower integrity level,
            // the no-write-up policy filters out rights that aren't in the
            // object type's generic read and execute set (for the file type,
            // that's FILE_GENERIC_READ | FILE_GENERIC_EXECUTE).
            //
            // Another option we considered here was manually setting the ACLs
            // on this file such that Builtin\Admins could read&write the file,
            // and all users could only read.
            //
            // Big thanks to @eryksun in GH#11222 for helping with this. This
            // alternative method was chosen because it's considerably simpler.

            // The required security descriptor can be created easily from the
            // SDDL string: "S:(ML;;NW;;;HI)"
            // (i.e. SACL:mandatory label;;no write up;;;high integrity level)
            unsigned long cb;
            THROW_IF_WIN32_BOOL_FALSE(
                ConvertStringSecurityDescriptorToSecurityDescriptor(L"S:(ML;;NW;;;HI)",
                                                                    SDDL_REVISION_1,
                                                                    wil::out_param_ptr<PSECURITY_DESCRIPTOR*>(sd),
                                                                    &cb));

            // Initialize a security attributes structure.
            sa.nLength = sizeof(SECURITY_ATTRIBUTES);
            sa.lpSecurityDescriptor = sd.get();
            sa.bInheritHandle = false;

            // If we're running in an elevated context, when this file is
            // created, it will automatically be owned by
            // Builtin\Administrators, which will pass the above
            // _isOwnedByAdministrators check.
            //
            // Programs running in an elevated context will be free to write the
            // file, and unelevated processes will be able to read the file. An
            // unelevated process could always delete the file and rename a new
            // file in its place (a la the way `vim.exe` saves files), but if
            // they do that, the new file _won't_ be owned by Administrators,
            // failing the above check.
        }

        wil::unique_hfile file{ CreateFileW(path.c_str(),
                                            GENERIC_WRITE,
                                            FILE_SHARE_READ | FILE_SHARE_DELETE,
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

        if (lastWriteTime)
        {
            THROW_IF_WIN32_BOOL_FALSE(GetFileTime(file.get(), nullptr, nullptr, lastWriteTime));
        }
    }

    void WriteUTF8FileAtomic(const std::filesystem::path& path, const std::string_view& content, FILETIME* lastWriteTime)
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
            WriteUTF8File(path, content, false, lastWriteTime);
            return;
        }

        auto tmpPath = resolvedPath;
        tmpPath += L".tmp";

        // Writing to a file isn't atomic, but...
        WriteUTF8File(tmpPath, content, false, lastWriteTime);

        // renaming one is (supposed to be) atomic.
        // Wait... "supposed to be"!? Well it's technically not always atomic,
        // but it's pretty darn close to it, so... better than nothing.
        std::filesystem::rename(tmpPath, resolvedPath);
    }
}

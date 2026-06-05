// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "FileUtils.h"

#include <appmodel.h>
#include <shlobj.h>
#include <WtExeUtils.h>

#include <fmt/compile.h>
#include <til/io.h>
#include <til/hash.h>

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

    // Computes the hash that (approximately) uniquely identifies a settings
    // file's contents on disk. Combines a content hash with the file's last
    // write time.
    winrt::hstring CalculateSettingsHash(std::string_view content, const FILETIME& lastWriteTime)
    {
        const auto fileHash = til::hash(content);
        const ULARGE_INTEGER fileTime{ lastWriteTime.dwLowDateTime, lastWriteTime.dwHighDateTime };
        const auto hash = fmt::format(FMT_COMPILE(L"{:016x}-{:016x}"), fileHash, fileTime.QuadPart);
        return winrt::hstring{ hash };
    }

    // Atomically writes settings content to disk:
    //   1. (optional) best-effort backup of the current file to "<path>.bak"
    //   2. write to a process-unique temp file in the same directory
    //   3. atomically replace the target via rename
    // The unique temp name avoids cross-process temp clobbering when multiple
    // Terminal instances auto-save concurrently. The ".bak" is best-effort and
    // diagnostic only -- it is not a reliable conflict-resolution mechanism.
    void WriteSettingsFile(const std::filesystem::path& path, std::string_view content, bool makeBackup, FILETIME* lastWriteTime)
    {
        // GH#10787: rename() replaces symbolic links themselves, not the path
        // they point at. Resolve them first before generating the temp path.
        std::error_code ec;
        const auto resolvedPath = std::filesystem::is_symlink(path) ? std::filesystem::canonical(path, ec) : path;
        if (ec)
        {
            if (ec.value() != ERROR_FILE_NOT_FOUND)
            {
                THROW_WIN32_MSG(ec.value(), "failed to compute canonical settings path");
            }
            // The original file is a symbolic link whose target doesn't exist.
            // Fall back to a direct (non-atomic) write; this is an edge case.
            til::io::write_utf8_string_to_file(path, content, false, lastWriteTime);
            return;
        }

        if (makeBackup)
        {
            std::error_code backupEc;
            auto backupPath = resolvedPath;
            backupPath += L".bak";
            // Best-effort: ignore failures (e.g. the file doesn't exist yet).
            std::filesystem::copy_file(resolvedPath, backupPath, std::filesystem::copy_options::overwrite_existing, backupEc);
        }

        // Process- and call-unique temp name to avoid cross-process clobber.
        static std::atomic<uint32_t> counter{ 0 };
        auto tmpPath = resolvedPath;
        tmpPath += fmt::format(FMT_COMPILE(L".{}.{}.tmp"), GetCurrentProcessId(), counter.fetch_add(1, std::memory_order_relaxed));

        til::io::write_utf8_string_to_file(tmpPath, content, false, lastWriteTime);

        // renaming is (close enough to) atomic.
        std::filesystem::rename(tmpPath, resolvedPath, ec);
        if (ec)
        {
            std::error_code cleanupEc;
            std::filesystem::remove(tmpPath, cleanupEc);
            THROW_WIN32_MSG(ec.value(), "failed to write settings file");
        }
    }
}

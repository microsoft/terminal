// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "FileUtils.h"

#include <appmodel.h>
#include <shlobj.h>
#include <WtExeUtils.h>

static constexpr std::string_view Utf8Bom{ u8"\uFEFF" };
static constexpr std::wstring_view UnpackagedSettingsFolderName{ L"Microsoft\\Windows Terminal\\" };

namespace Microsoft::Terminal::Settings::Model
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

    // Tries to read a file somewhat atomically without locking it.
    // Strips the UTF8 BOM if it exists.
    std::string ReadUTF8File(const std::filesystem::path& path)
    {
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
    std::optional<std::string> ReadUTF8FileIfExists(const std::filesystem::path& path)
    {
        try
        {
            return { ReadUTF8File(path) };
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

    void WriteUTF8File(const std::filesystem::path& path, const std::string_view content)
    {
        wil::unique_hfile file{ CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) };
        THROW_LAST_ERROR_IF(!file);

        const auto fileSize = gsl::narrow<DWORD>(content.size());
        DWORD bytesWritten = 0;
        THROW_IF_WIN32_BOOL_FALSE(WriteFile(file.get(), content.data(), fileSize, &bytesWritten, nullptr));

        if (bytesWritten != fileSize)
        {
            THROW_WIN32_MSG(ERROR_WRITE_FAULT, "failed to write whole file");
        }
    }

    void WriteUTF8FileAtomic(const std::filesystem::path& path, const std::string_view content)
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

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

    std::string ReadUTF8File(const std::filesystem::path& path)
    {
        // From some casual observations we can determine that:
        // * ReadFile() always returns the requested amount of data (unless the file is smaller)
        // * It's unlikely that the file was changed between GetFileSize() and ReadFile()
        // -> Lets add a retry-loop just in case, to not fail if the file size changed.
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
            THROW_LAST_ERROR_IF(!ReadFile(file.get(), buffer.data(), gsl::narrow<DWORD>(buffer.size()), &bytesRead, nullptr));

            // This implementation isn't atomic as it's not trivially possible to
            // atomically read the entire file without breaking workflow of our users.
            // For instance locking the file would conflict with users who have the file open in an editor.
            // We'll just throw in case where the file size changed as that's likely an error.
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
                buffer.erase(0, Utf8Bom.size());
            }

            return buffer;
        }

        THROW_WIN32_MSG(ERROR_READ_FAULT, "file size changed while reading");
    }

    // Returns an empty optional (aka std::nullopt), if the file couldn't be opened.
    // Use THROW_LAST_ERROR() if you want to fail in that case.
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
        THROW_LAST_ERROR_IF(!WriteFile(file.get(), content.data(), fileSize, &bytesWritten, nullptr));

        if (bytesWritten != fileSize)
        {
            THROW_WIN32_MSG(ERROR_WRITE_FAULT, "failed to write whole file");
        }
    }

    void WriteUTF8FileAtomic(const std::filesystem::path& path, const std::string_view content)
    {
        auto tmpPath = path;
        tmpPath += L".tmp";

        WriteUTF8File(tmpPath, content);
        std::filesystem::rename(tmpPath, path);
    }
}

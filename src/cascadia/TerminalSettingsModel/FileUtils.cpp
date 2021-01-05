// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "FileUtils.h"

#include <appmodel.h>
#include <shlobj.h>

static constexpr std::string_view Utf8Bom{ u8"\uFEFF" };
static constexpr std::wstring_view UnpackagedSettingsFolderName{ L"Microsoft\\Windows Terminal\\" };

namespace Microsoft::Terminal::Settings::Model
{
    // Function Description:
    // - Returns true if we're running in a packaged context.
    //   If we are, we want to change our settings path slightly.
    // Arguments:
    // - <none>
    // Return Value:
    // - true iff we're running in a packaged context.
    static bool _IsPackaged()
    {
        UINT32 length = 0;
        LONG rc = GetCurrentPackageFullName(&length, nullptr);
        return rc != APPMODEL_ERROR_NO_PACKAGE;
    }

    std::filesystem::path GetBaseSettingsPath()
    {
        static std::filesystem::path baseSettingsPath = []() {
            wil::unique_cotaskmem_string localAppDataFolder;
            // KF_FLAG_FORCE_APP_DATA_REDIRECTION, when engaged, causes SHGet... to return
            // the new AppModel paths (Packages/xxx/RoamingState, etc.) for standard path requests.
            // Using this flag allows us to avoid Windows.Storage.ApplicationData completely.
            THROW_IF_FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_FORCE_APP_DATA_REDIRECTION, nullptr, &localAppDataFolder));

            std::filesystem::path parentDirectoryForSettingsFile{ localAppDataFolder.get() };

            if (!_IsPackaged())
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
    // - Reads the content in UTF-8 encoding of the given file using the Win32 APIs
    // - If the file has a BOM, it is stripped.
    // Arguments:
    // - <none>
    // Return Value:
    // - the content of the file if we were able to read it
    std::string ReadUTF8TextFileFull(HANDLE hFile)
    {
        // fileSize is in bytes
        const auto fileSize{ GetFileSize(hFile, nullptr) };
        THROW_LAST_ERROR_IF(fileSize == INVALID_FILE_SIZE);

        auto utf8buffer{ std::make_unique<char[]>(fileSize) };

        DWORD bytesRead{};
        THROW_LAST_ERROR_IF(!ReadFile(hFile, utf8buffer.get(), fileSize, &bytesRead, nullptr));

        std::string_view utf8DataView{ utf8buffer.get(), bytesRead };
        if (utf8DataView.size() >= Utf8Bom.size() &&
            utf8DataView.substr(0, Utf8Bom.size()).compare(Utf8Bom) == 0)
        {
            utf8DataView = utf8DataView.substr(Utf8Bom.size());
        }

        return std::string{ utf8DataView };
    }
}

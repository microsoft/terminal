/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- FileUtils.h
--*/

namespace Microsoft::Terminal::Settings::Model
{
    std::filesystem::path GetBaseSettingsPath();
    std::string ReadUTF8TextFileFull(HANDLE hFile);
}

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

namespace winrt::Microsoft::Terminal::Settings::Model
{
    bool IsPortableMode();
    std::filesystem::path GetBaseSettingsPath();
    std::filesystem::path GetReleaseSettingsPath();
    winrt::hstring CalculateSettingsHash(std::string_view content, const FILETIME& lastWriteTime);
    void WriteSettingsFile(const std::filesystem::path& path, std::string_view content, bool makeBackup, FILETIME* lastWriteTime = nullptr);
}

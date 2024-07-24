// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

namespace winrt::Microsoft::Terminal::Settings::Model
{
    bool IsPortableMode();
    std::filesystem::path GetBaseSettingsPath();
    std::filesystem::path GetReleaseSettingsPath();
    std::string ReadUTF8File(const std::filesystem::path& path, const bool elevatedOnly = false, FILETIME* lastWriteTime = nullptr);
    std::optional<std::string> ReadUTF8FileIfExists(const std::filesystem::path& path, const bool elevatedOnly = false, FILETIME* lastWriteTime = nullptr);
    void WriteUTF8File(const std::filesystem::path& path, const std::string_view& content, const bool elevatedOnly = false, FILETIME* lastWriteTime = nullptr);
    void WriteUTF8FileAtomic(const std::filesystem::path& path, const std::string_view& content, FILETIME* lastWriteTime = nullptr);
}

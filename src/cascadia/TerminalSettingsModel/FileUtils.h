// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

namespace winrt::Microsoft::Terminal::Settings::Model
{
    std::filesystem::path GetBaseSettingsPath();
    std::string ReadUTF8File(const std::filesystem::path& path, const bool elevatedOnly = false);
    std::optional<std::string> ReadUTF8FileIfExists(const std::filesystem::path& path, const bool elevatedOnly = false);
    void WriteUTF8File(const std::filesystem::path& path, const std::string_view& content, const bool elevatedOnly = false);
    void WriteUTF8FileAtomic(const std::filesystem::path& path, const std::string_view& content);
}

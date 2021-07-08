// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

namespace Microsoft::Terminal::Settings::Model
{
    std::filesystem::path GetBaseSettingsPath();
    std::string ReadUTF8File(const std::filesystem::path& path);
    std::optional<std::string> ReadUTF8FileIfExists(const std::filesystem::path& path);
    void WriteUTF8File(const std::filesystem::path& path, const std::string_view content);
    void WriteUTF8FileAtomic(const std::filesystem::path& path, const std::string_view content);
}

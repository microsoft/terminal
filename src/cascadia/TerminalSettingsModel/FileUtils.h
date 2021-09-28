// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace winrt::Microsoft::Terminal::Settings::Model
{
    // I couldn't find a wil helper for this so I made it myself
    class locked_hfile
    {
    public:
        wil::unique_hfile file;
        OVERLAPPED lockedRegion;

        ~locked_hfile()
        {
            if (file)
            {
                // Need to unlock the file before it is closed
                UnlockFileEx(file.get(), 0, INT_MAX, 0, &lockedRegion);
            }
        }

        HANDLE get() const noexcept
        {
            return file.get();
        }
    };

    std::filesystem::path GetBaseSettingsPath();

    locked_hfile OpenFileReadSharedLocked(const std::filesystem::path& path);
    locked_hfile OpenFileRWExclusiveLocked(const std::filesystem::path& path);
    std::string ReadUTF8FileLocked(const locked_hfile& file);
    void WriteUTF8FileLocked(const locked_hfile& file, const std::string_view& content);

    std::string ReadUTF8File(const std::filesystem::path& path);
    std::optional<std::string> ReadUTF8FileIfExists(const std::filesystem::path& path);
    void WriteUTF8File(const std::filesystem::path& path, const std::string_view& content);
    void WriteUTF8FileAtomic(const std::filesystem::path& path, const std::string_view& content);
}

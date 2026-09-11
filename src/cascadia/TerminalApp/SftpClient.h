// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

struct libssh2_sftp_struct;

namespace winrt::TerminalApp::implementation
{
    struct SftpFileEntryData
    {
        std::wstring name{};
        std::wstring fullPath{};
        std::wstring permissions{};
        std::wstring sizeText{};
        std::wstring modTimeText{};
        std::wstring glyph{};
        unsigned long long size{ 0 };
        bool isDirectory{ false };
    };

    struct SftpTransferProgress
    {
        unsigned long long totalBytes{ 0 };
        unsigned long long transferredBytes{ 0 };
        bool isIndeterminate{ false };
    };

    // Thin, blocking wrapper around libssh2's SFTP subsystem. All public
    // methods are serialized through a mutex so that the UI layer can safely
    // call them from a background thread while another operation is still in
    // flight. Connect/Disconnect/close are safe to call from the UI thread.
    class SftpClient
    {
    public:
        SftpClient();
        ~SftpClient();

        bool Connect(const std::wstring& host,
                     unsigned int port,
                     const std::wstring& username,
                     const std::wstring& password,
                     const std::wstring& privateKeyPath,
                     std::wstring& errorMessage);

        void Disconnect();

        bool IsConnected() const;

        bool ListDirectory(const std::wstring& path,
                           std::vector<SftpFileEntryData>& entries,
                           std::wstring& errorMessage);

        bool DownloadFile(const std::wstring& remotePath,
                          const std::wstring& localPath,
                          const std::atomic<bool>& cancel,
                          SftpTransferProgress& progress,
                          std::wstring& errorMessage);

        bool UploadFile(const std::wstring& localPath,
                        const std::wstring& remotePath,
                        const std::atomic<bool>& cancel,
                        SftpTransferProgress& progress,
                        std::wstring& errorMessage);

        bool Mkdir(const std::wstring& path, std::wstring& errorMessage);
        bool CreateNewFile(const std::wstring& remotePath, std::wstring& errorMessage);
        bool Unlink(const std::wstring& path, std::wstring& errorMessage);
        bool Rmdir(const std::wstring& path, std::wstring& errorMessage);
        bool Rename(const std::wstring& oldPath, const std::wstring& newPath, std::wstring& errorMessage);
        bool Chmod(const std::wstring& path, unsigned long mode, std::wstring& errorMessage);

        // Returns true if the path exists. isDirectory is only meaningful when
        // the path exists.
        bool Stat(const std::wstring& path, bool& isDirectory, std::wstring& errorMessage);

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };
}
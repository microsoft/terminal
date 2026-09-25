// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SftpClient.h"

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <libssh2.h>
#include <libssh2_sftp.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "advapi32.lib")

using namespace std::chrono;

namespace
{
    [[nodiscard]] std::wstring _formatSize(unsigned long long size)
    {
        constexpr std::wstring_view units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
        double value = static_cast<double>(size);
        size_t unitIndex = 0;
        while (value >= 1024.0 && unitIndex < std::size(units) - 1)
        {
            value /= 1024.0;
            ++unitIndex;
        }
        if (unitIndex == 0)
        {
            return fmt::format(L"{} {}", size, units[unitIndex]);
        }
        return fmt::format(L"{:.1f} {}", value, units[unitIndex]);
    }

    [[nodiscard]] std::wstring _formatUnixTime(unsigned long timeValue)
    {
        std::time_t raw{ static_cast<std::time_t>(timeValue) };
        struct tm local{};
        if (localtime_s(&local, &raw) != 0)
        {
            return L"";
        }
        return fmt::format(L"{:02}:{:02} {:02}/{:02}/{:04}",
                           local.tm_hour,
                           local.tm_min,
                           local.tm_mon + 1,
                           local.tm_mday,
                           local.tm_year + 1900);
    }

    [[nodiscard]] std::wstring _formatPermissions(unsigned long mode)
    {
        std::wstring result;
        result.reserve(10);
        if ((mode & LIBSSH2_SFTP_S_IFMT) == LIBSSH2_SFTP_S_IFDIR)
        {
            result += L'd';
        }
        else if ((mode & LIBSSH2_SFTP_S_IFMT) == LIBSSH2_SFTP_S_IFLNK)
        {
            result += L'l';
        }
        else
        {
            result += L'-';
        }
        constexpr wchar_t chars[] = { L'r', L'w', L'x' };
        for (int shift = 6; shift >= 0; shift -= 3)
        {
            for (int bit = 2; bit >= 0; --bit)
            {
                result += (mode & (1 << (shift * 3 + bit))) ? chars[bit] : L'-';
            }
        }
        return result;
    }

    [[nodiscard]] std::wstring _glyphForFile(const std::wstring& name, bool isDirectory, bool isSymlink)
    {
        if (isDirectory)
        {
            return L"\uE8B7"; // Folder
        }
        if (isSymlink)
        {
            return L"\uE71B"; // Link
        }
        const auto dot = name.find_last_of(L'.');
        if (dot != std::wstring::npos)
        {
            auto ext = name.substr(dot);
            CharLowerBuffW(ext.data(), static_cast<DWORD>(ext.size()));
            if (ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".gif" || ext == L".bmp" || ext == L".svg" || ext == L".ico")
            {
                return L"\uEB9F"; // Picture
            }
            if (ext == L".mp3" || ext == L".wav" || ext == L".flac" || ext == L".ogg")
            {
                return L"\uED28"; // MusicNote
            }
            if (ext == L".mp4" || ext == L".avi" || ext == L".mkv" || ext == L".mov")
            {
                return L"\uE8B2"; // Video
            }
            if (ext == L".zip" || ext == L".rar" || ext == L".7z" || ext == L".tar" || ext == L".gz")
            {
                return L"\uE7B8"; // ZipFolder
            }
            if (ext == L".exe" || ext == L".msi" || ext == L".bat" || ext == L".cmd")
            {
                return L"\uE950"; // CommandPrompt
            }
            if (ext == L".md" || ext == L".txt" || ext == L".log" || ext == L".json" || ext == L".xml" ||
                ext == L".yml" || ext == L".yaml" || ext == L".toml" || ext == L".ini" || ext == L".cfg")
            {
                return L"\uE7C3"; // Page
            }
            if (ext == L".cpp" || ext == L".h" || ext == L".hpp" || ext == L".c" || ext == L".cs" ||
                ext == L".py" || ext == L".js" || ext == L".ts" || ext == L".rs" || ext == L".go")
            {
                return L"\uE943"; // Code
            }
        }
        return L"\uE8A5"; // Document
    }
}

namespace winrt::TerminalApp::implementation
{
    struct SftpClient::Impl
    {
        ~Impl()
        {
            Close();
        }

        bool Connect(const std::wstring& host,
                     unsigned int port,
                     const std::wstring& username,
                     const std::wstring& password,
                     const std::wstring& privateKeyPath,
                     std::wstring& errorMessage)
        {
            std::lock_guard guard{ mutex };
            CloseLocked();

            WSADATA wsadata{};
            if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
            {
                errorMessage = L"Failed to initialize Winsock";
                return false;
            }
            wsaInitialized = true;

            addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;

            const auto portStr = fmt::format(FMT_STRING("{}"), port);
            addrinfo* resultInfo = nullptr;
            if (getaddrinfo(til::u16u8(host).c_str(), portStr.c_str(), &hints, &resultInfo) != 0)
            {
                errorMessage = L"Could not resolve host: " + host;
                WSACleanup();
                wsaInitialized = false;
                return false;
            }

            socket = ::socket(resultInfo->ai_family, resultInfo->ai_socktype, resultInfo->ai_protocol);
            if (socket == INVALID_SOCKET)
            {
                errorMessage = L"Failed to create socket";
                freeaddrinfo(resultInfo);
                WSACleanup();
                wsaInitialized = false;
                return false;
            }

            // 15 seconds connect timeout
            DWORD timeout = 15000;
            setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

            int err = ::connect(socket, resultInfo->ai_addr, static_cast<int>(resultInfo->ai_addrlen));
            freeaddrinfo(resultInfo);
            if (err != 0)
            {
                errorMessage = fmt::format(L"Could not connect to {}:{}", host, port);
                closesocket(socket);
                socket = INVALID_SOCKET;
                WSACleanup();
                wsaInitialized = false;
                return false;
            }

            session = libssh2_session_init();
            if (!session)
            {
                errorMessage = L"Failed to initialize SSH session";
                closesocket(socket);
                socket = INVALID_SOCKET;
                WSACleanup();
                wsaInitialized = false;
                return false;
            }

            libssh2_session_set_blocking(session, 1);

            int rc = libssh2_session_handshake(session, socket);
            if (rc != 0)
            {
                errorMessage = fmt::format(L"SSH handshake failed (error {})", rc);
                CloseLocked();
                return false;
            }

            int authResult = 0;
            if (!privateKeyPath.empty())
            {
                authResult = libssh2_userauth_publickey_fromfile(session,
                                                                 til::u16u8(username).c_str(),
                                                                 nullptr,
                                                                 til::u16u8(privateKeyPath).c_str(),
                                                                 nullptr);
            }
            else
            {
                authResult = libssh2_userauth_password(session,
                                                       til::u16u8(username).c_str(),
                                                       til::u16u8(password).c_str());
            }
            if (authResult != 0)
            {
                errorMessage = L"Authentication failed. Check your username, password or private key.";
                CloseLocked();
                return false;
            }

            sftp = libssh2_sftp_init(session);
            if (!sftp)
            {
                errorMessage = L"SFTP subsystem is not available on the remote server";
                CloseLocked();
                return false;
            }

            return true;
        }

        void CloseLocked()
        {
            if (sftp)
            {
                libssh2_sftp_shutdown(sftp);
                sftp = nullptr;
            }
            if (session)
            {
                libssh2_session_disconnect(session, "Terminal SFTP client closing");
                libssh2_session_free(session);
                session = nullptr;
            }
            if (socket != INVALID_SOCKET)
            {
                closesocket(socket);
                socket = INVALID_SOCKET;
            }
            if (wsaInitialized)
            {
                WSACleanup();
                wsaInitialized = false;
            }
        }

        bool ListedDirectory(const std::wstring& path,
                             std::vector<SftpFileEntryData>& entries,
                             std::wstring& errorMessage)
        {
            LIBSSH2_SFTP_HANDLE* dir = libssh2_sftp_opendir(sftp, til::u16u8(path).c_str());
            if (!dir)
            {
                errorMessage = L"Unable to open directory for reading";
                return false;
            }

            char buffer[1024]{};
            do
            {
                LIBSSH2_SFTP_ATTRIBUTES attrs{};
                const auto bytes = libssh2_sftp_readdir(dir, buffer, sizeof(buffer), &attrs);
                if (bytes <= 0)
                {
                    break;
                }

                const auto name = til::u8u16(std::string_view{ buffer, static_cast<size_t>(bytes) });
                if (name == L"." || name == L"..")
                {
                    continue;
                }

                SftpFileEntryData entry;
                entry.name = name;
                entry.fullPath = path;
                if (!entry.fullPath.empty() && entry.fullPath.back() != L'/')
                {
                    entry.fullPath += L'/';
                }
                entry.fullPath += name;
                entry.isDirectory = (attrs.permissions & LIBSSH2_SFTP_S_IFMT) == LIBSSH2_SFTP_S_IFDIR;
                entry.permissions = _formatPermissions(attrs.permissions);
                entry.sizeText = _formatSize(attrs.filesize);
                entry.modTimeText = _formatUnixTime(attrs.mtime);
                entry.glyph = _glyphForFile(name, entry.isDirectory, (attrs.permissions & LIBSSH2_SFTP_S_IFMT) == LIBSSH2_SFTP_S_IFLNK);
                entries.emplace_back(std::move(entry));
            } while (true);

            libssh2_sftp_closedir(dir);

            // Directories first, then by name
            std::stable_sort(entries.begin(), entries.end(), [](const SftpFileEntryData& a, const SftpFileEntryData& b) {
                if (a.isDirectory != b.isDirectory)
                {
                    return a.isDirectory;
                }
                return a.name < b.name;
            });
            return true;
        }

        bool TransferFile(const std::wstring& remotePath,
                          const std::wstring& localPath,
                          bool upload,
                          const std::atomic<bool>& cancel,
                          SftpTransferProgress& progress,
                          std::wstring& errorMessage)
        {
            LIBSSH2_SFTP_ATTRIBUTES attrs{};
            if (libssh2_sftp_stat(sftp, til::u16u8(remotePath).c_str(), &attrs) == 0)
            {
                progress.totalBytes = static_cast<unsigned long long>(attrs.filesize);
            }
            else
            {
                progress.isIndeterminate = true;
            }

            wil::unique_hfile localHandle;
            if (upload)
            {
                localHandle.reset(CreateFileW(localPath.c_str(),
                                              GENERIC_READ,
                                              FILE_SHARE_READ,
                                              nullptr,
                                              OPEN_EXISTING,
                                              FILE_ATTRIBUTE_NORMAL,
                                              nullptr));
                if (!localHandle)
                {
                    errorMessage = L"Unable to open local file for reading";
                    return false;
                }
            }
            else
            {
                localHandle.reset(CreateFileW(localPath.c_str(),
                                              GENERIC_WRITE,
                                              0,
                                              nullptr,
                                              CREATE_ALWAYS,
                                              FILE_ATTRIBUTE_NORMAL,
                                              nullptr));
                if (!localHandle)
                {
                    errorMessage = L"Unable to open local file for writing";
                    return false;
                }
            }

            const auto flags = upload ? (LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC)
                                      : LIBSSH2_FXF_READ;
            LIBSSH2_SFTP_HANDLE* remoteHandle = libssh2_sftp_open(sftp, til::u16u8(remotePath).c_str(), flags, LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH);
            if (!remoteHandle)
            {
                errorMessage = upload ? L"Unable to create the remote file" : L"Unable to open the remote file";
                return false;
            }

            std::vector<char> buffer(65536);
            bool success = true;

            if (upload)
            {
                DWORD bytesRead = 0;
                while (ReadFile(localHandle.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr) && bytesRead > 0)
                {
                    if (cancel.load())
                    {
                        errorMessage = L"Transfer cancelled";
                        success = false;
                        break;
                    }
                    char* ptr = buffer.data();
                    ssize_t remaining = static_cast<ssize_t>(bytesRead);
                    while (remaining > 0)
                    {
                        const ssize_t written = libssh2_sftp_write(remoteHandle, ptr, static_cast<size_t>(remaining));
                        if (written <= 0)
                        {
                            errorMessage = L"Failed to write to the remote file";
                            success = false;
                            break;
                        }
                        ptr += written;
                        remaining -= written;
                        progress.transferredBytes += static_cast<unsigned long long>(written);
                    }
                    if (!success)
                    {
                        break;
                    }
                }
            }
            else
            {
                ssize_t bytesRead = 0;
                while ((bytesRead = libssh2_sftp_read(remoteHandle, buffer.data(), static_cast<size_t>(buffer.size()))) > 0)
                {
                    if (cancel.load())
                    {
                        errorMessage = L"Transfer cancelled";
                        success = false;
                        break;
                    }
                    DWORD written = 0;
                    if (!WriteFile(localHandle.get(), buffer.data(), static_cast<DWORD>(bytesRead), &written, nullptr) || written != static_cast<DWORD>(bytesRead))
                    {
                        errorMessage = L"Failed to write to the local file";
                        success = false;
                        break;
                    }
                    progress.transferredBytes += static_cast<unsigned long long>(bytesRead);
                }
                if (bytesRead < 0 && success)
                {
                    errorMessage = L"Failed to read from the remote file";
                    success = false;
                }
            }

            libssh2_sftp_close_handle(remoteHandle);
            return success;
        }

        bool HomeDirectoryLocked(std::wstring& home, std::wstring& /*errorMessage*/)
        {
            char buffer[4096]{};
            const auto rc = libssh2_sftp_realpath(sftp, ".", buffer, sizeof(buffer));
            if (rc > 0)
            {
                home = til::u8u16(buffer);
            }
            if (home.empty() || home.front() != L'/')
            {
                home = L"/";
            }
            return true;
        }

        std::mutex mutex;
        bool wsaInitialized{ false };
        SOCKET socket{ INVALID_SOCKET };
        LIBSSH2_SESSION* session{ nullptr };
        LIBSSH2_SFTP* sftp{ nullptr };

        void Close()
        {
            std::lock_guard guard{ mutex };
            CloseLocked();
        }
    };

    SftpClient::SftpClient() :
        _impl{ std::make_unique<Impl>() }
    {
    }

    SftpClient::~SftpClient() = default;

    bool SftpClient::Connect(const std::wstring& host,
                             unsigned int port,
                             const std::wstring& username,
                             const std::wstring& password,
                             const std::wstring& privateKeyPath,
                             std::wstring& errorMessage)
    {
        return _impl->Connect(host, port, username, password, privateKeyPath, errorMessage);
    }

    void SftpClient::Disconnect()
    {
        _impl->Close();
    }

    bool SftpClient::IsConnected() const
    {
        std::lock_guard guard{ _impl->mutex };
        return _impl->sftp != nullptr;
    }

    bool SftpClient::ListDirectory(const std::wstring& path,
                                   std::vector<SftpFileEntryData>& entries,
                                   std::wstring& errorMessage)
    {
        std::lock_guard guard{ _impl->mutex };
        if (!_impl->sftp)
        {
            errorMessage = L"Not connected";
            return false;
        }
        return _impl->ListedDirectory(path, entries, errorMessage);
    }

    bool SftpClient::DownloadFile(const std::wstring& remotePath,
                                  const std::wstring& localPath,
                                  const std::atomic<bool>& cancel,
                                  SftpTransferProgress& progress,
                                  std::wstring& errorMessage)
    {
        std::lock_guard guard{ _impl->mutex };
        if (!_impl->sftp)
        {
            errorMessage = L"Not connected";
            return false;
        }
        return _impl->TransferFile(remotePath, localPath, false, cancel, progress, errorMessage);
    }

bool SftpClient::UploadFile(const std::wstring& localPath,
                                const std::wstring& remotePath,
                                const std::atomic<bool>& cancel,
                                SftpTransferProgress& progress,
                                std::wstring& errorMessage)
    {
        std::lock_guard guard{ _impl->mutex };
        if (!_impl->sftp)
        {
            errorMessage = L"Not connected";
            return false;
        }
        return _impl->TransferFile(remotePath, localPath, true, cancel, progress, errorMessage);
    }

    bool SftpClient::Mkdir(const std::wstring& path, std::wstring& errorMessage)
    {
        std::lock_guard guard{ _impl->mutex };
        if (!_impl->sftp)
        {
            errorMessage = L"Not connected";
            return false;
        }
        const int rc = libssh2_sftp_mkdir(_impl->sftp, til::u16u8(path).c_str(), LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRWXG | LIBSSH2_SFTP_S_IRWXO);
        if (rc != 0)
        {
            errorMessage = L"Unable to create the remote directory";
            return false;
        }
        return true;
    }

    bool SftpClient::CreateNewFile(const std::wstring& remotePath, std::wstring& errorMessage)
    {
        std::lock_guard guard{ _impl->mutex };
        if (!_impl->sftp)
        {
            errorMessage = L"Not connected";
            return false;
        }
        LIBSSH2_SFTP_HANDLE* handle = libssh2_sftp_open(_impl->sftp,
                                                        til::u16u8(remotePath).c_str(),
                                                        LIBSSH2_FXF_CREAT | LIBSSH2_FXF_WRITE,
                                                        LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH);
        if (!handle)
        {
            errorMessage = L"Unable to create the remote file";
            return false;
        }
        libssh2_sftp_close_handle(handle);
        return true;
    }

    bool SftpClient::Unlink(const std::wstring& path, std::wstring& errorMessage)
    {
        std::lock_guard guard{ _impl->mutex };
        if (!_impl->sftp)
        {
            errorMessage = L"Not connected";
            return false;
        }
        if (libssh2_sftp_unlink(_impl->sftp, til::u16u8(path).c_str()) != 0)
        {
            errorMessage = L"Unable to delete the remote file";
            return false;
        }
        return true;
    }

    bool SftpClient::Rmdir(const std::wstring& path, std::wstring& errorMessage)
    {
        std::lock_guard guard{ _impl->mutex };
        if (!_impl->sftp)
        {
            errorMessage = L"Not connected";
            return false;
        }
        if (libssh2_sftp_rmdir(_impl->sftp, til::u16u8(path).c_str()) != 0)
        {
            errorMessage = L"Unable to delete the remote directory";
            return false;
        }
        return true;
    }

    bool SftpClient::Rename(const std::wstring& oldPath, const std::wstring& newPath, std::wstring& errorMessage)
    {
        std::lock_guard guard{ _impl->mutex };
        if (!_impl->sftp)
        {
            errorMessage = L"Not connected";
            return false;
        }
        if (libssh2_sftp_rename_ex(_impl->sftp,
                                   til::u16u8(oldPath).c_str(),
                                   static_cast<unsigned int>(til::u16u8(oldPath).size()),
                                   til::u16u8(newPath).c_str(),
                                   static_cast<unsigned int>(til::u16u8(newPath).size()),
                                   LIBSSH2_SFTP_RENAME_OVERWRITE) != 0)
        {
            errorMessage = L"Unable to rename the remote entry";
            return false;
        }
        return true;
    }

    bool SftpClient::Chmod(const std::wstring& path, unsigned long mode, std::wstring& errorMessage)
    {
        std::lock_guard guard{ _impl->mutex };
        if (!_impl->sftp)
        {
            errorMessage = L"Not connected";
            return false;
        }
        LIBSSH2_SFTP_ATTRIBUTES attrs{};
        attrs.permissions = mode;
        attrs.flags = LIBSSH2_SFTP_ATTR_PERMISSIONS;
        if (libssh2_sftp_setstat(_impl->sftp, til::u16u8(path).c_str(), &attrs) != 0)
        {
            errorMessage = L"Unable to change permissions";
            return false;
        }
        return true;
    }

    bool SftpClient::GetFileInfo(const std::wstring& path, SftpFileInfoData& info, std::wstring& errorMessage)
    {
        std::lock_guard guard{ _impl->mutex };
        if (!_impl->sftp)
        {
            errorMessage = L"Not connected";
            return false;
        }
        LIBSSH2_SFTP_ATTRIBUTES attrs{};
        if (libssh2_sftp_stat(_impl->sftp, til::u16u8(path).c_str(), &attrs) != 0)
        {
            errorMessage = L"Path does not exist";
            return false;
        }
        info.path = path;
        const auto slash{ path.find_last_of(L'/') };
        info.name = slash != std::wstring::npos ? path.substr(slash + 1) : path;
        if ((attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) != 0)
        {
            info.size = attrs.filesize;
            info.sizeText = _formatSize(attrs.filesize);
        }
        if ((attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) != 0)
        {
            info.permissions = attrs.permissions;
            info.isDirectory = (attrs.permissions & LIBSSH2_SFTP_S_IFMT) == LIBSSH2_SFTP_S_IFDIR;
            info.isSymlink = (attrs.permissions & LIBSSH2_SFTP_S_IFMT) == LIBSSH2_SFTP_S_IFLNK;
            info.permissionsText = _formatPermissions(attrs.permissions);
        }
        if ((attrs.flags & LIBSSH2_SFTP_ATTR_UIDGID) != 0)
        {
            info.uid = attrs.uid;
            info.gid = attrs.gid;
        }
        if ((attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) != 0)
        {
            info.atime = attrs.atime;
            info.mtime = attrs.mtime;
            info.modTimeText = _formatUnixTime(attrs.mtime);
            info.accessTimeText = _formatUnixTime(attrs.atime);
        }
        info.exists = true;
        return true;
    }

    bool SftpClient::HomeDirectory(std::wstring& home, std::wstring& errorMessage)
    {
        std::lock_guard guard{ _impl->mutex };
        if (!_impl->sftp)
        {
            errorMessage = L"Not connected";
            return false;
        }
        return _impl->HomeDirectoryLocked(home, errorMessage);
    }
}
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SftpBrowserContent.h"
#include "SftpBrowserContent.g.cpp"
#include "SftpFileEntry.g.cpp"
#include "SftpProfileItem.g.cpp"

#include "Utils.h"

#include <algorithm>
#include <cstdlib>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string_view>

#include <dpapi.h>
#include <wincrypt.h>

#pragma comment(lib, "Crypt32.lib")

using namespace winrt;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Pickers;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Input;
using namespace winrt::Windows::UI::Xaml::Media;

namespace
{
    // How many consecutive failed sync-upload attempts we tolerate for a file
    // before giving up and reporting the failure once. Prevents the sync timer
    // (which ticks every 1.5s) from retrying forever and spamming error dialogs.
    constexpr uint32_t kMaxSyncAttempts{ 2 };

    // Combine a remote parent directory with a child name, handling the
    // leading "/" of the absolute path.
    std::wstring CombineRemotePath(const std::wstring& parent, const std::wstring& name)
    {
        if (parent.empty() || parent == L"/")
        {
            return L"/" + name;
        }
        std::wstring result{ parent };
        if (result.back() != L'/')
        {
            result.push_back(L'/');
        }
        result += name;
        return result;
    }

    // Returns the parent directory of an absolute remote path.
    std::wstring GetParentRemotePath(const std::wstring& path)
    {
        auto end{ path.find_last_of(L'/') };
        if (end == std::wstring::npos)
        {
            return L"/";
        }
        if (end == 0)
        {
            return L"/";
        }
        auto result{ path.substr(0, end) };
        while (result.size() > 1 && result.back() == L'/')
        {
            result.pop_back();
        }
        return result;
    }

    // Returns the last path component of an absolute remote path.
    std::wstring GetRemoteFileName(const std::wstring& path)
    {
        const auto end{ path.find_last_of(L"/\\") };
        if (end == std::wstring::npos)
        {
            return path;
        }
        return path.substr(end + 1);
    }

    // Fetches the last-write time of a local file (ignoring system files).
    bool GetLocalFileLastWriteTime(const std::wstring& localPath, FILETIME& out)
    {
        const auto h{ CreateFileW(localPath.c_str(),
                                  FILE_READ_ATTRIBUTES,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr,
                                  OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL,
                                  nullptr) };
        if (h == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        auto scope{ wil::scope_exit([&]() { CloseHandle(h); }) };
        out = FILETIME{};
        return GetFileTime(h, nullptr, nullptr, &out) != FALSE;
    }

    // Returns a stable local cache directory for the given remote path.
    std::filesystem::path CacheDirectoryForRemote(const std::wstring& remotePath)
    {
        const auto hash{ std::hash<std::wstring>{}(remotePath) };
        return std::filesystem::temp_directory_path()
            / L"WinTermSftp"
            / (std::to_wstring(hash));
    }

    // Escapes a string so it can be stored as a JSON string literal.
    std::wstring JsonEscape(const std::wstring& in)
    {
        std::wstring out;
        out.reserve(in.size());
        for (const wchar_t ch : in)
        {
            switch (ch)
            {
            case L'\\': out += L"\\\\"; break;
            case L'\"': out += L"\\\""; break;
            case L'\n': out += L"\\n"; break;
            case L'\r': out += L"\\r"; break;
            case L'\t': out += L"\\t"; break;
            default: out.push_back(ch); break;
            }
        }
        return out;
    }

    // Returns the position of the closing quote for the string that starts at
    // `open`, skipping escaped characters.
    size_t FindJsonStringEnd(const std::wstring& text, size_t open)
    {
        size_t i{ open + 1 };
        while (i < text.size())
        {
            if (text[i] == L'\\')
            {
                i += 2;
                continue;
            }
            if (text[i] == L'"')
            {
                return i;
            }
            ++i;
        }
        return std::wstring::npos;
    }

    // Decodes the JSON escape sequences used when writing profiles.
    std::wstring JsonUnescape(const std::wstring& in)
    {
        std::wstring out;
        out.reserve(in.size());
        for (size_t i = 0; i < in.size(); ++i)
        {
            if (in[i] != L'\\')
            {
                out.push_back(in[i]);
                continue;
            }
            if (i + 1 >= in.size())
            {
                out.push_back(L'\\');
                break;
            }
            switch (in[++i])
            {
            case L'n': out.push_back(L'\n'); break;
            case L'r': out.push_back(L'\r'); break;
            case L't': out.push_back(L'\t'); break;
            case L'b': out.push_back(L'\b'); break;
            case L'f': out.push_back(L'\f'); break;
            case L'u':
                if (i + 4 < in.size())
                {
                    wchar_t digits[5]{ in[i + 1], in[i + 2], in[i + 3], in[i + 4], L'\0' };
                    out.push_back(static_cast<wchar_t>(wcstoul(digits, nullptr, 16)));
                    i += 4;
                }
                else
                {
                    out.push_back(L'u');
                }
                break;
            default: out.push_back(in[i]); break; // covers escaped quotes and backslashes
            }
        }
        return out;
    }

    // Marker used to distinguish a DPAPI-encrypted password from a legacy
    // plaintext one, so old configuration files keep working.
    constexpr std::wstring_view EncryptedPasswordMarker{ L"sftp-dpapi:" };

    // Encrypts a password with DPAPI (current user scope) and returns it in a
    // form that is safe to store in the JSON config. An empty input yields an
    // empty result. The blob is base64-encoded so it survives the hand-rolled
    // JSON serializer.
    std::wstring EncryptPassword(const std::wstring& password)
    {
        if (password.empty())
        {
            return L"";
        }

        const auto utf8{ til::u16u8(password) };
        DATA_BLOB plain{};
        plain.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(utf8.data()));
        plain.cbData = static_cast<DWORD>(utf8.size());

        DATA_BLOB encrypted{};
        if (!CryptProtectData(&plain,
                              L"windows-terminal-sftp-password",
                              nullptr,
                              nullptr,
                              nullptr,
                              CRYPTPROTECT_UI_FORBIDDEN,
                              &encrypted))
        {
            return L"";
        }

        std::wstring encoded;
        DWORD encodedSize{ 0 };
        if (!CryptBinaryToStringW(encrypted.pbData,
                                  encrypted.cbData,
                                  CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                                  nullptr,
                                  &encodedSize))
        {
            LocalFree(encrypted.pbData);
            return L"";
        }
        encoded.resize(encodedSize);
        if (!CryptBinaryToStringW(encrypted.pbData,
                                  encrypted.cbData,
                                  CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                                  encoded.data(),
                                  &encodedSize))
        {
            LocalFree(encrypted.pbData);
            return L"";
        }
        LocalFree(encrypted.pbData);

        // CryptBinaryToStringW writes a trailing NUL; drop it.
        if (!encoded.empty() && encoded.back() == L'\0')
        {
            encoded.pop_back();
        }

        return std::wstring{ EncryptedPasswordMarker } + encoded;
    }

    // Decrypts a password that was stored by EncryptPassword. Values that do
    // not carry the DPAPI marker (legacy plaintext) are returned untouched.
    std::wstring DecryptPassword(const std::wstring& password)
    {
        if (password.size() < EncryptedPasswordMarker.size() ||
            password.compare(0, EncryptedPasswordMarker.size(), EncryptedPasswordMarker) != 0)
        {
            return password;
        }

        const auto encoded{ password.substr(EncryptedPasswordMarker.size()) };

        DWORD blobSize{ 0 };
        if (!CryptStringToBinaryW(encoded.c_str(),
                                  static_cast<DWORD>(encoded.size()),
                                  CRYPT_STRING_BASE64,
                                  nullptr,
                                  &blobSize,
                                  nullptr,
                                  nullptr))
        {
            return L"";
        }
        std::vector<BYTE> blob(blobSize);
        if (!CryptStringToBinaryW(encoded.c_str(),
                                  static_cast<DWORD>(encoded.size()),
                                  CRYPT_STRING_BASE64,
                                  blob.data(),
                                  &blobSize,
                                  nullptr,
                                  nullptr))
        {
            return L"";
        }

        DATA_BLOB encrypted{};
        encrypted.pbData = blob.data();
        encrypted.cbData = blobSize;

        DATA_BLOB plain{};
        if (!CryptUnprotectData(&encrypted,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                CRYPTPROTECT_UI_FORBIDDEN,
                                &plain))
        {
            // Password was encrypted on a different account/machine and cannot
            // be recovered here.
            return L"";
        }

        std::string utf8{ reinterpret_cast<const char*>(plain.pbData), plain.cbData };
        LocalFree(plain.pbData);

        return til::u8u16(utf8);
    }
}

namespace winrt::TerminalApp::implementation
{
    hstring SftpFileEntry::Name() const
    {
        return _name;
    }

    void SftpFileEntry::Name(hstring const& value)
    {
        _name = value;
    }

    hstring SftpFileEntry::FullPath() const
    {
        return _fullPath;
    }

    void SftpFileEntry::FullPath(hstring const& value)
    {
        _fullPath = value;
    }

    hstring SftpFileEntry::Permissions() const
    {
        return _permissions;
    }

    void SftpFileEntry::Permissions(hstring const& value)
    {
        _permissions = value;
    }

    hstring SftpFileEntry::SizeText() const
    {
        return _sizeText;
    }

    void SftpFileEntry::SizeText(hstring const& value)
    {
        _sizeText = value;
    }

    hstring SftpFileEntry::ModTimeText() const
    {
        return _modTimeText;
    }

    void SftpFileEntry::ModTimeText(hstring const& value)
    {
        _modTimeText = value;
    }

    hstring SftpFileEntry::Glyph() const
    {
        return _glyph;
    }

    void SftpFileEntry::Glyph(hstring const& value)
    {
        _glyph = value;
    }

    bool SftpFileEntry::IsDirectory() const
    {
        return _isDirectory;
    }

    void SftpFileEntry::IsDirectory(bool value)
    {
        _isDirectory = value;
    }

    hstring SftpProfileItem::Name() const
    {
        return _name;
    }

    void SftpProfileItem::Name(hstring const& value)
    {
        _name = value;
    }

    hstring SftpProfileItem::Host() const
    {
        return _host;
    }

    void SftpProfileItem::Host(hstring const& value)
    {
        _host = value;
    }

    hstring SftpProfileItem::Username() const
    {
        return _username;
    }

    void SftpProfileItem::Username(hstring const& value)
    {
        _username = value;
    }

    hstring SftpProfileItem::AuthMode() const
    {
        return _authMode;
    }

    void SftpProfileItem::AuthMode(hstring const& value)
    {
        _authMode = value;
    }

    SftpBrowserContent::SftpBrowserContent() :
        _entries{ winrt::single_threaded_observable_vector<TerminalApp::SftpFileEntry>() },
        _profileItems{ winrt::single_threaded_observable_vector<TerminalApp::SftpProfileItem>() }
    {
        InitializeComponent();

        _syncTimer = DispatcherTimer{};
        _syncTimer.Interval(std::chrono::milliseconds{ 1500 });
        _syncTimer.Tick({ this, &SftpBrowserContent::_syncTimerTick });

        _statusTimer = DispatcherTimer{};
        _statusTimer.Interval(std::chrono::milliseconds{ 3000 });
        _statusTimer.Tick({ this, &SftpBrowserContent::_statusTimerTick });

        KeyDown({ get_weak(), &SftpBrowserContent::_contentKeyDown });

        _loadProfiles();
    }

    // Method Description:
    // - Used by the action handler to pre-populate the connection form and
    //   kick off the connection as soon as the pane appears.
    void SftpBrowserContent::Initialize(const hstring& host,
                                        uint32_t port,
                                        const hstring& username,
                                        const hstring& password,
                                        const hstring& privateKeyPath)
    {
        _host = host;
        _port = port;
        _username = username;
        _password = password;
        _privateKeyPath = privateKeyPath;

        if (!host.empty())
        {
            hostBox().Text(host);
            portBox().Text(std::to_wstring(port));
            userBox().Text(username);
            if (!password.empty())
            {
                passBox().Password(password);
            }
            if (!privateKeyPath.empty())
            {
                keyBox().Text(privateKeyPath);
            }
        }
    }

    void SftpBrowserContent::SetHostingWindow(uint64_t hwnd)
    {
        _hostingWindow = hwnd;
    }

    void SftpBrowserContent::Navigate(const hstring& path)
    {
        if (!_isConnected)
        {
            return;
        }
        _currentPath = path;
        _loadDirectoryAsync();
    }

#pragma region IPaneContent

    FrameworkElement SftpBrowserContent::GetRoot()
    {
        return *this;
    }

    void SftpBrowserContent::Close()
    {
        // Stop the edit-sync timer up front. Its tick handler is a
        // fire-and-forget coroutine that dereferences this object, so letting
        // it fire after the pane is destroyed would crash the process.
        _syncTimer.Stop();
        _statusTimer.Stop();
        _syncAttempts.clear();

        // Raise the close event first so the pane closes immediately. The
        // actual teardown (Disconnect) goes to a background thread: the libssh2
        // calls inside Disconnect block the caller, and doing them on the UI
        // thread made the pane look like it refused to close on the X button.
        CloseRequested.raise(*this, nullptr);
        _disconnectAsync();
    }

    fire_and_forget SftpBrowserContent::_disconnectAsync()
    {
        // The pane is being torn down right now; keep the object alive while
        // the background disconnect runs so the client stays valid.
        auto lifetime{ get_strong() };
        co_await winrt::resume_background();
        _client.Disconnect();
    }

    INewContentArgs SftpBrowserContent::GetNewTerminalArgs(BuildStartupKind /* kind */) const
    {
        // A duplicated pane opens a fresh SFTP browser; the user reconnects
        // with the credentials from the form.
        return BaseContentArgs(L"sftp-browser");
    }

    hstring SftpBrowserContent::Title()
    {
        if (_isConnected)
        {
            return winrt::hstring{ fmt::format(FMT_COMPILE(L"SFTP: {0} [{1}]"), _host, _currentPath) };
        }
        return L"SFTP Browser";
    }

    hstring SftpBrowserContent::Icon() const
    {
        // "\xE8B7" is the "Folder" glyph which reads as the SFTP browser pane.
        return L"\xE8B7";
    }

#pragma endregion

    // Builds a locally-cached copy of a remote file so VS Code (or any local
    // editor) can open it with a real local path.
    std::wstring SftpBrowserContent::_localCachePath(const std::wstring& remotePath)
    {
        try
        {
            const auto dir{ CacheDirectoryForRemote(remotePath) };
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            return (dir / GetRemoteFileName(remotePath)).wstring();
        }
        catch (...)
        {
            return L"";
        }
    }

    void SftpBrowserContent::_removeEditSession(const std::wstring& remotePath)
    {
        const auto it{ _editSessions.find(remotePath) };
        if (it != _editSessions.end())
        {
            std::error_code ec;
            std::filesystem::remove(it->second, ec);
            _editSessions.erase(it);
        }
        _pendingSync.erase(remotePath);
        _editedTimes.erase(remotePath);
    }

    // Polls the locally-cached copies of edited files and uploads any that
    // changed since we last looked at them. This provides the "edit a remote
    // file in VS Code and watch it sync back" experience without a custom SFTP
    // host configuration.
    void SftpBrowserContent::_syncEditedFiles()
    {
        if (!_isConnected)
        {
            return;
        }

        for (const auto& [remotePath, localPath] : _editSessions)
        {
            FILETIME modified{};
            if (!GetLocalFileLastWriteTime(localPath, modified))
            {
                // The file was deleted or moved out of the cache.
                auto it{ _pendingSync.find(remotePath) };
                if (it != _pendingSync.end())
                {
                    _pendingSync.erase(it);
                }
                continue;
            }

            const auto it{ _editedTimes.find(remotePath) };
            if (it != _editedTimes.end() && it->second.dwLowDateTime == modified.dwLowDateTime && it->second.dwHighDateTime == modified.dwHighDateTime)
            {
                continue;
            }
            _editedTimes[remotePath] = modified;
            _pendingSync[remotePath] = localPath;

            if (!_pendingSync.empty())
            {
                refreshButton().IsEnabled(false);
            }
        }
    }

    fire_and_forget SftpBrowserContent::_pullFileIntoVSCode(std::wstring remotePath)
    {
        if (!_isConnected)
        {
            co_return;
        }

        // Keep the control alive across every await point so the coroutine can
        // never touch a destroyed object, and remember the dispatcher up front:
        // accessing Dispatcher() after resuming from the background would reuse
        // a dangling `this` if the pane was closed mid-download.
        auto lifetime{ get_strong() };
        const auto dispatcher{ Dispatcher() };

        const auto localPath{ _localCachePath(remotePath) };
        if (localPath.empty())
        {
            _showError(L"Could not resolve a local cache path for the file.");
            co_return;
        }

        auto errorMessage{ std::wstring{} };
        auto downloaded{ false };

        _setStatus(L"Downloading for edit...");

        co_await resume_background();

        // Wrap the background work in a catch that does NOT co_await.
        // C++ coroutines prohibit co_await inside catch blocks (C2304).
        // If the download throws, we set a flag and handle it on the UI thread.
        try
        {
            auto progress = SftpTransferProgress{};
            downloaded = _client.DownloadFile(remotePath, localPath, _cancelTransfer, progress, errorMessage);
        }
        catch (...)
        {
            downloaded = false;
            if (errorMessage.empty())
            {
                errorMessage = L"Download failed with an exception";
            }
        }

        co_await wil::resume_foreground(dispatcher);

        if (!downloaded)
        {
            _showError(errorMessage.empty() ? L"Failed to download file" : errorMessage);
            co_return;
        }

        if (!_editSessions.contains(remotePath))
        {
            _editSessions.emplace(remotePath, localPath);
        }
        FILETIME modified{};
        if (GetLocalFileLastWriteTime(localPath, modified))
        {
            _editedTimes[remotePath] = modified;
        }

        // Open the file in VS Code if it's installed, otherwise fall back to
        // the default handler for the file type.
        auto codeArgs{ L"\"" + localPath + L"\"" };
        auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", L"code", codeArgs.c_str(), nullptr, SW_SHOWNORMAL));
        if (result <= 32)
        {
            result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", localPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
            if (result <= 32)
            {
                _showError(L"Could not launch an editor for the file. It was downloaded to:\n" + localPath);
                co_return;
            }
        }

        _setStatus(L"Editing remote file. Save in your editor and changes will sync back.");
        _syncTimer.Start();
    }

    fire_and_forget SftpBrowserContent::_downloadAsync(TerminalApp::SftpFileEntry entry, hstring localPath)
    {
        if (!_isConnected || entry == nullptr)
        {
            co_return;
        }

        if (entry.IsDirectory())
        {
            _showError(L"Directories are not downloaded yet.");
            co_return;
        }

        const auto dispatcher{ Dispatcher() };

        auto lifetime{ get_strong() };

        const auto remotePath{ entry.FullPath() };
        const auto localPathCopy{ std::wstring{ localPath } };
        auto errorMessage{ std::wstring{} };
        auto succeeded{ false };

        _setStatus(L"Downloading...");
        progressRing().IsActive(true);
        progressRing().Visibility(Visibility::Visible);

        co_await resume_background();

        try
        {
            auto progress = SftpTransferProgress{};
            succeeded = _client.DownloadFile(std::wstring{ remotePath }, localPathCopy, _cancelTransfer, progress, errorMessage);
        }
        catch (...)
        {
            succeeded = false;
            if (errorMessage.empty())
            {
                errorMessage = L"Download failed with an exception";
            }
        }

        co_await wil::resume_foreground(dispatcher);

        progressRing().IsActive(false);
        progressRing().Visibility(Visibility::Collapsed);

        if (!succeeded)
        {
            _showError(errorMessage.empty() ? L"Download failed" : errorMessage);
        }
        else
        {
            _setStatus(L"Downloaded to " + localPath);
        }
    }

    fire_and_forget SftpBrowserContent::_loadDirectoryAsync()
    {
        if (!_isConnected)
        {
            co_return;
        }

        // Keep the control alive and remember the dispatcher before the
        // background hop: accessing Dispatcher() and poking XAML after resuming
        // from the thread pool would crash if the pane is torn down mid-call.
        auto lifetime{ get_strong() };
        const auto dispatcher{ Dispatcher() };

        auto errorMessage{ std::wstring{} };
        std::vector<SftpFileEntryData> entries{};

        _setStatus(L"Loading...");
        progressRing().IsActive(true);
        progressRing().Visibility(Visibility::Visible);

        co_await resume_background();

        auto ok{ false };
        try
        {
            ok = _client.ListDirectory(_currentPath, entries, errorMessage);
        }
        catch (...)
        {
            ok = false;
            if (errorMessage.empty())
            {
                errorMessage = L"Failed to load directory with an exception";
            }
        }

        co_await wil::resume_foreground(dispatcher);

        progressRing().IsActive(false);
        progressRing().Visibility(Visibility::Collapsed);

        if (!ok)
        {
            _showError(errorMessage.empty() ? L"Failed to load directory" : errorMessage);
            if (_currentPath != L"/")
            {
                _currentPath = L"/";
                _loadDirectoryAsync();
            }
            co_return;
        }

        // Sort directories first, then names (case-insensitive).
        std::stable_sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.isDirectory != rhs.isDirectory)
            {
                return lhs.isDirectory;
            }
            auto lo{ lhs.name };
            auto ro{ rhs.name };
            std::transform(lo.begin(), lo.end(), lo.begin(), [](wchar_t c) { return static_cast<wchar_t>(CharLowerBuffW(&c, 1)); });
            std::transform(ro.begin(), ro.end(), ro.begin(), [](wchar_t c) { return static_cast<wchar_t>(CharLowerBuffW(&c, 1)); });
            return lo < ro;
        });

        // Unless the "show hidden" toggle is on, drop dot-files from view.
        if (!_showHiddenFiles)
        {
            entries.erase(std::remove_if(entries.begin(), entries.end(), [](const auto& entry) {
                              return !entry.name.empty() && entry.name.front() == L'.';
                          }),
                          entries.end());
        }

        _entries.Clear();
        for (const auto& data : entries)
        {
            auto impl{ winrt::make_self<SftpFileEntry>() };
            impl->Name(winrt::hstring{ data.name });
            impl->FullPath(winrt::hstring{ data.fullPath });
            impl->Permissions(winrt::hstring{ data.permissions });
            impl->SizeText(winrt::hstring{ data.sizeText });
            impl->ModTimeText(winrt::hstring{ data.modTimeText });
            impl->Glyph(winrt::hstring{ data.glyph });
            impl->IsDirectory(data.isDirectory);
            _entries.Append(*impl);
        }
        fileList().ItemsSource(_entries);

        _updateBreadcrumbBar();
        _statusText = til::hstring_format(FMT_COMPILE(L"Connected to {0}  |  {1} item(s)"), _host, _entries.Size());
        _setStatus(_statusText);
    }

    fire_and_forget SftpBrowserContent::_connectAsync()
    {
        if (_connecting)
        {
            co_return;
        }
        _connecting = true;

        // Remember the dispatcher before the background hop and keep the
        // control alive across every await point.
        auto lifetime{ get_strong() };
        const auto dispatcher{ Dispatcher() };

        const auto host{ _host };
        const auto port{ _port };
        const auto username{ _username };
        const auto password{ _password };
        const auto privateKeyPath{ _privateKeyPath };

        auto errorMessage{ std::wstring{} };
        auto ok{ false };

        _setStatus(L"Connecting...");
        progressRing().IsActive(true);
        progressRing().Visibility(Visibility::Visible);
        connectActionButton().IsEnabled(false);

        co_await resume_background();

        try
        {
            ok = _client.Connect(std::wstring{ host }, port, std::wstring{ username }, std::wstring{ password }, std::wstring{ privateKeyPath }, errorMessage);
        }
        catch (...)
        {
            ok = false;
            if (errorMessage.empty())
            {
                errorMessage = L"Connection failed with an exception";
            }
        }

        co_await wil::resume_foreground(dispatcher);

        progressRing().IsActive(false);
        progressRing().Visibility(Visibility::Collapsed);
        _connecting = false;

        if (!ok)
        {
            connectActionButton().IsEnabled(true);
            _showError(errorMessage.empty() ? L"Failed to connect" : errorMessage);
            _setStatus(L"Connection failed");
            co_return;
        }

        _isConnected = true;
        _setConnectedUi(true);
        _setStatus(L"Connected");

        {
            auto home{ std::wstring{} };
            auto homeError{ std::wstring{} };
            if (!_client.HomeDirectory(home, homeError) || home.empty())
            {
                home = L"/";
            }
            _homePath = home;
            _currentPath = _homePath;
        }

        _loadDirectoryAsync();
    }

    fire_and_forget SftpBrowserContent::_openInVSCodeAsync(TerminalApp::SftpFileEntry entry)
    {
        try
        {
            if (!_isConnected || entry == nullptr)
            {
                co_return;
            }
            if (entry.IsDirectory())
            {
                _showError(L"Opening folders in VS Code is not supported yet. Open a file instead.");
                co_return;
            }
            _pullFileIntoVSCode(std::wstring{ entry.FullPath() });
        }
        catch (...)
        {
            _showError(L"Failed to open the file. Check that an editor is installed and the path is valid.");
        }
    }

    fire_and_forget SftpBrowserContent::_uploadFilesAsync(winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Storage::StorageFile> files)
    {
        std::wstring errorMessage{};

        // Lifetime and dispatcher reads are thread-safe; this coroutine can be
        // entered from a threadpool continuation (drag&drop or picker
        // Completed), so pin to the UI thread before touching StorageFile or
        // any UI element (RPC_E_WRONG_THREAD).
        auto lifetime{ get_strong() };
        const auto dispatcher{ Dispatcher() };

        co_await wil::resume_foreground(dispatcher);

        try
        {
            if (!_isConnected)
            {
                co_return;
            }

            uint32_t uploaded{ 0 };
            uint32_t failed{ 0 };

            _setStatus(L"Uploading...");
            progressRing().IsActive(true);
            progressRing().Visibility(Visibility::Visible);

            // StorageFile is a UI-thread-bound object; materialize the paths
            // before hopping to the background thread (RPC_E_WRONG_THREAD).
            std::vector<std::wstring> localPaths;
            for (const auto& file : files)
            {
                localPaths.push_back(std::wstring{ file.Path() });
            }

            co_await resume_background();

            for (const auto& localPath : localPaths)
            {
                try
                {
                    const auto remotePath{ CombineRemotePath(_currentPath, GetRemoteFileName(localPath)) };

                    auto progress = SftpTransferProgress{};
                    if (_client.UploadFile(localPath, remotePath, _cancelTransfer, progress, errorMessage))
                    {
                        ++uploaded;
                    }
                    else
                    {
                        ++failed;
                    }
                }
                catch (...)
                {
                    ++failed;
                    if (errorMessage.empty())
                    {
                        errorMessage = L"Upload failed with an exception";
                    }
                }
            }

            co_await wil::resume_foreground(dispatcher);

            progressRing().IsActive(false);
            progressRing().Visibility(Visibility::Collapsed);

            if (failed == 0)
            {
                _setStatus(winrt::hstring{ fmt::format(FMT_COMPILE(L"Uploaded {0} file(s)"), uploaded) });
            }
            else
            {
                _showError(errorMessage.empty() ? L"Some files failed to upload" : errorMessage);
                _setStatus(winrt::hstring{ fmt::format(FMT_COMPILE(L"Uploaded {0}, failed {1}"), uploaded, failed) });
            }

            _loadDirectoryAsync();
        }
        catch (const winrt::hresult_error& ex)
        {
            _showError(L"Upload failed: " + std::wstring{ ex.message().c_str() });
        }
        catch (...)
        {
            _showError(errorMessage.empty() ? L"Upload failed with an unexpected error." : L"Upload failed: " + errorMessage);
        }
    }

    fire_and_forget SftpBrowserContent::_newFolderAsync()
    {
        if (!_isConnected)
        {
            co_return;
        }

        auto lifetime{ get_strong() };
        const auto dispatcher{ Dispatcher() };

        try
        {
            const auto name{ co_await _promptForInputAsync(L"New folder", L"Folder name", L"") };
            if (name.empty())
            {
                co_return;
            }

            co_await resume_background();

            auto errorMessage{ std::wstring{} };
            const auto ok{ _client.Mkdir(CombineRemotePath(_currentPath, std::wstring{ name }), errorMessage) };

            co_await wil::resume_foreground(dispatcher);

            if (!ok)
            {
                _showError(errorMessage.empty() ? L"Failed to create folder" : errorMessage);
                co_return;
            }
            _loadDirectoryAsync();
        }
        catch (...)
        {
            _showError(L"New folder dialog failed.");
        }
    }

    fire_and_forget SftpBrowserContent::_newFileAsync()
    {
        if (!_isConnected)
        {
            co_return;
        }

        auto lifetime{ get_strong() };
        const auto dispatcher{ Dispatcher() };

        try
        {
            const auto name{ std::wstring{ co_await _promptForInputAsync(L"New file", L"File name", L"") } };
            if (name.empty())
            {
                _showError(L"File name cannot be empty.");
                co_return;
            }
            if (name.find(L'/') != std::wstring::npos || name.find(L'\\') != std::wstring::npos)
            {
                _showError(L"File name cannot contain path separators.");
                co_return;
            }

            co_await resume_background();

            auto errorMessage{ std::wstring{} };
            const auto ok{ _client.CreateNewFile(CombineRemotePath(_currentPath, std::wstring{ name }), errorMessage) };

            co_await wil::resume_foreground(dispatcher);

            if (!ok)
            {
                _showError(errorMessage.empty() ? L"Failed to create file" : errorMessage);
                co_return;
            }
            _loadDirectoryAsync();
        }
        catch (...)
        {
            _showError(L"New file dialog failed.");
        }
    }

    fire_and_forget SftpBrowserContent::_renameAsync(TerminalApp::SftpFileEntry entry)
    {
        if (!_isConnected || entry == nullptr)
        {
            co_return;
        }

        auto lifetime{ get_strong() };
        const auto dispatcher{ Dispatcher() };

        try
        {
            const auto newName{ co_await _promptForInputAsync(L"Rename", L"File name", entry.Name()) };
            if (newName.empty() || newName == entry.Name())
            {
                co_return;
            }

            const auto oldPath{ std::wstring{ entry.FullPath() } };
            const auto newPath{ CombineRemotePath(GetParentRemotePath(oldPath), std::wstring{ newName }) };

            co_await resume_background();

            auto errorMessage{ std::wstring{} };
            const auto ok{ _client.Rename(oldPath, newPath, errorMessage) };

            co_await wil::resume_foreground(dispatcher);

            if (!ok)
            {
                _showError(errorMessage.empty() ? L"Failed to rename" : errorMessage);
                co_return;
            }
            _removeEditSession(oldPath);
            _loadDirectoryAsync();
        }
        catch (...)
        {
            _showError(L"Rename dialog failed.");
        }
    }

    fire_and_forget SftpBrowserContent::_deleteAsync(TerminalApp::SftpFileEntry entry)
    {
        if (!_isConnected || entry == nullptr)
        {
            co_return;
        }

        try
        {
            auto dialog{ ContentDialog{} };
            dialog.Title(box_value(entry.IsDirectory() ? L"Delete folder?" : L"Delete file?"));
            dialog.Content(box_value(winrt::hstring{ L"Are you sure you want to delete \"" + std::wstring{ entry.FullPath() } + L"\"? This cannot be undone." }));
            dialog.PrimaryButtonText(L"Delete");
            dialog.CloseButtonText(L"Cancel");
            dialog.DefaultButton(ContentDialogButton::Close);
            dialog.XamlRoot(XamlRoot());

            const auto result{ co_await dialog.ShowAsync(ContentDialogPlacement::Popup) };
            if (result != ContentDialogResult::Primary)
            {
                co_return;
            }
        }
        catch (...)
        {
            _showError(L"Delete dialog failed.");
            co_return;
        }

        // Remember the dispatcher before the background hop and keep the
        // control alive across every await point.
        auto lifetime{ get_strong() };
        const auto dispatcher{ Dispatcher() };

        const auto path{ std::wstring{ entry.FullPath() } };
        auto errorMessage{ std::wstring{} };
        auto ok{ false };

        _setStatus(L"Deleting...");

        co_await resume_background();

        try
        {
            if (entry.IsDirectory())
            {
                ok = _client.Rmdir(path, errorMessage);
            }
            else
            {
                ok = _client.Unlink(path, errorMessage);
            }
        }
        catch (...)
        {
            ok = false;
            if (errorMessage.empty())
            {
                errorMessage = L"Failed to delete with an exception";
            }
        }

        co_await wil::resume_foreground(dispatcher);

        if (!ok)
        {
            _showError(errorMessage.empty() ? L"Failed to delete" : errorMessage);
            co_return;
        }
        _removeEditSession(path);
        _loadDirectoryAsync();
    }

    fire_and_forget SftpBrowserContent::_showFileInfoAsync(TerminalApp::SftpFileEntry entry)
    {
        if (!_isConnected || entry == nullptr)
        {
            co_return;
        }

        // Remember the dispatcher before the background hop and keep the
        // control alive across every await point.
        auto lifetime{ get_strong() };
        const auto dispatcher{ Dispatcher() };

        const auto path{ std::wstring{ entry.FullPath() } };
        auto errorMessage{ std::wstring{} };
        SftpFileInfoData info;
        auto ok{ false };

        co_await resume_background();

        try
        {
            ok = _client.GetFileInfo(path, info, errorMessage);
        }
        catch (...)
        {
            ok = false;
            if (errorMessage.empty())
            {
                errorMessage = L"Failed to read file information.";
            }
        }

        co_await wil::resume_foreground(dispatcher);

        if (!ok)
        {
            _showError(errorMessage);
            co_return;
        }

        const auto objectType{ info.isSymlink ? L"Symbolic link" : (info.isDirectory ? L"Folder" : L"File") };

        struct InfoRow
        {
            winrt::hstring label;
            winrt::hstring value;
        };
        const auto rows{ std::vector<InfoRow>{
            { L"Name", winrt::hstring{ info.name } },
            { L"Path", winrt::hstring{ info.path } },
            { L"Type", winrt::hstring{ objectType } },
            { L"Size", winrt::hstring{ info.sizeText } },
            { L"Permissions", winrt::hstring{ info.permissionsText } },
            { L"UID", winrt::hstring{ fmt::format(L"{}", info.uid) } },
            { L"GID", winrt::hstring{ fmt::format(L"{}", info.gid) } },
            { L"Modified", winrt::hstring{ info.modTimeText } },
            { L"Accessed", winrt::hstring{ info.accessTimeText } },
        } };

        // Two aligned columns: keys on the left, values on the right.
        auto content{ Grid{} };
        auto nameColumn{ ColumnDefinition{} };
        auto valueColumn{ ColumnDefinition{} };
        valueColumn.Width(GridLength{ 1, GridUnitType::Star });
        content.ColumnDefinitions().Append(nameColumn);
        content.ColumnDefinitions().Append(valueColumn);
        for (size_t i{ 0 }; i < rows.size(); ++i)
        {
            content.RowDefinitions().Append(RowDefinition{});

            auto label{ TextBlock{} };
            label.Text(rows[i].label);
            label.Margin(ThicknessHelper::FromLengths(0, 0, 16, 6));
            label.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
            label.TextWrapping(TextWrapping::NoWrap);
            Grid::SetColumn(label, 0);
            Grid::SetRow(label, static_cast<int>(i));
            content.Children().Append(label);

            auto value{ TextBlock{} };
            value.Text(rows[i].value);
            value.TextWrapping(TextWrapping::Wrap);
            Grid::SetColumn(value, 1);
            Grid::SetRow(value, static_cast<int>(i));
            content.Children().Append(value);
        }

        try
        {
            auto dialog{ ContentDialog{} };
            dialog.Title(box_value(entry.Name()));
            dialog.Content(box_value(content));
            dialog.PrimaryButtonText(L"OK");
            dialog.XamlRoot(XamlRoot());
            co_await dialog.ShowAsync(ContentDialogPlacement::Popup);
        }
        catch (...)
        {
            _showError(L"Properties dialog failed.");
        }
    }

    fire_and_forget SftpBrowserContent::_chmodAsync(TerminalApp::SftpFileEntry entry)
    {
        if (!_isConnected || entry == nullptr)
        {
            co_return;
        }

        auto lifetime{ get_strong() };
        const auto dispatcher{ Dispatcher() };

        try
        {
            const auto text{ co_await _promptForInputAsync(L"Permissions (chmod)", L"e.g. 755", entry.Permissions()) };
            if (text.empty())
            {
                co_return;
            }

            unsigned long mode{ 0 };
            try
            {
                mode = static_cast<unsigned long>(std::stoul(std::wstring{ text }, nullptr, 8));
            }
            catch (...)
            {
                _showError(L"Enter permissions as an octal number, e.g. 755.");
                co_return;
            }

            co_await resume_background();

            auto errorMessage{ std::wstring{} };
            const auto ok{ _client.Chmod(std::wstring{ entry.FullPath() }, mode, errorMessage) };

            co_await wil::resume_foreground(dispatcher);

            if (!ok)
            {
                _showError(errorMessage.empty() ? L"Failed to change permissions" : errorMessage);
                co_return;
            }
            _loadDirectoryAsync();
        }
        catch (...)
        {
            _showError(L"Permissions dialog failed.");
        }
    }

    fire_and_forget SftpBrowserContent::_downloadToFolderAsync(TerminalApp::SftpFileEntry entry)
    {
        if (!_isConnected || entry == nullptr)
        {
            co_return;
        }

        if (entry.IsDirectory())
        {
            _showError(L"Directories are not downloaded yet.");
            co_return;
        }

        // Let the OS pick a destination file.
        const auto hwnd{ _hostingWindow ? reinterpret_cast<HWND>(_hostingWindow) : nullptr };
        const auto targetPath{ co_await SaveFilePicker(hwnd, [filename = std::wstring{ entry.Name() }](IFileDialog* dialog) {
            LOG_IF_FAILED(dialog->SetFileName(filename.c_str()));
        }) };

        if (targetPath.empty())
        {
            co_return;
        }
        _downloadAsync(entry, targetPath);
    }

    void SftpBrowserContent::_setStatus(const hstring& text)
    {
        try
        {
            statusText().Text(text);
        }
        catch (...)
        {
        }
    }

    void SftpBrowserContent::_showError(const std::wstring& message)
    {
        // From any thread, marshal to the UI thread via DispatcherQueue to
        // avoid poking XAML from the thread pool.
        const auto queue{ winrt::Windows::System::DispatcherQueue::GetForCurrentThread() };
        const auto isUiThread{ queue != nullptr };

        if (!isUiThread)
        {
            // We are on a background thread. Try to get the UI dispatcher
            // queue through the element's dispatcher instead.
            // On WinUI3, Dispatcher() returns CoreDispatcher. Use its
            // RunAsync equivalent via the element's DispatcherQueue.
            // Note: this path is reached from fire_and_forget catch blocks
            // that run on the thread pool. Post the work and return.
            try
            {
                // On background threads there is no DispatcherQueue for this
                // thread. Fall back to posting via CoreDispatcher.RunAsync.
                Dispatcher().RunAsync(
                    winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                    [weakThis = get_weak(), message]() {
                        if (const auto self{ weakThis.get() })
                        {
                            self->_showErrorOnUi(message);
                        }
                    });
            }
            catch (...)
            {
            }
            return;
        }
        try
        {
            _showErrorOnUi(message);
        }
        catch (...)
        {
        }
    }

    void SftpBrowserContent::_showErrorOnUi(const std::wstring& message)
    {
        progressRing().IsActive(false);
        progressRing().Visibility(Visibility::Collapsed);
        statusText().Text(L"Error");
        try
        {
            auto dialog{ ContentDialog{} };
            dialog.Title(box_value(L"SFTP Browser"));
            dialog.Content(box_value(winrt::hstring{ message }));
            dialog.CloseButtonText(L"OK");
            if (auto root{ XamlRoot() })
            {
                dialog.XamlRoot(root);
                dialog.ShowAsync(ContentDialogPlacement::Popup);
            }
        }
        catch (...)
        {
        }
    }

    winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> SftpBrowserContent::_promptForInputAsync(const winrt::hstring& title, const winrt::hstring& placeholder, const winrt::hstring& initial)
    {
        // ContentDialog text boxes don't receive keypresses in XAML Islands, so
        // text input is done through an overlay attached to this control which
        // stays fully inside the pane's visual tree.
        auto lifetime{ get_strong() };
        const auto dispatcher{ Dispatcher() };
        co_await wil::resume_foreground(dispatcher);

        _inputDialogResult.reset();
        _inputDialogDone = false;
        inputTitleText().Text(title);
        inputValueBox().PlaceholderText(placeholder);
        inputValueBox().Text(initial);
        inputOverlay().Visibility(Visibility::Visible);
        inputValueBox().Focus(FocusState::Programmatic);
        inputValueBox().SelectAll();

        while (!_inputDialogDone)
        {
            co_await winrt::resume_after(std::chrono::milliseconds{ 50 });
            co_await wil::resume_foreground(dispatcher);
        }

        inputOverlay().Visibility(Visibility::Collapsed);
        const auto result{ _inputDialogResult.value_or(winrt::hstring{}) };
        _inputDialogResult.reset();
        co_return result;
    }

    void SftpBrowserContent::_inputOkClick(const IInspectable&, const RoutedEventArgs&)
    {
        _inputDialogResult = inputValueBox().Text();
        _inputDialogDone = true;
    }

    void SftpBrowserContent::_inputCancelClick(const IInspectable&, const RoutedEventArgs&)
    {
        _inputDialogDone = true;
    }

    void SftpBrowserContent::_inputValueBoxKeyDown(const IInspectable&, KeyRoutedEventArgs const& e)
    {
        if (e.Key() == winrt::Windows::System::VirtualKey::Enter)
        {
            _inputDialogResult = inputValueBox().Text();
            _inputDialogDone = true;
            e.Handled(true);
        }
        else if (e.Key() == winrt::Windows::System::VirtualKey::Escape)
        {
            _inputDialogDone = true;
            e.Handled(true);
        }
    }

    void SftpBrowserContent::_refreshList()
    {
        if (_isConnected)
        {
            _loadDirectoryAsync();
        }
    }

    void SftpBrowserContent::_setConnectedUi(bool connected)
    {
        connectActionButton().IsEnabled(!connected);
        disconnectButton().IsEnabled(connected);
        hiddenButton().IsEnabled(connected);
        backButton().IsEnabled(connected && !_backHistory.empty());
        forwardButton().IsEnabled(connected && !_forwardHistory.empty());
        newFileButton().IsEnabled(connected);
        newFolderButton().IsEnabled(connected);
        uploadButton().IsEnabled(connected);
        downloadButton().IsEnabled(connected);
        refreshButton().IsEnabled(connected);
        connectForm().Visibility(connected ? Visibility::Collapsed : Visibility::Visible);
    }

    fire_and_forget SftpBrowserContent::_syncTimerTick(const IInspectable&, const IInspectable&)
    {
        // Keep the control alive across the await points. The timer can fire
        // right as the pane is being torn down, and an unguarded coroutine that
        // touches member state afterwards would crash the process.
        auto lifetime{ get_strong() };
        const auto dispatcher{ Dispatcher() };

        try
        {
            if (!_isConnected)
            {
                co_return;
            }

            _syncEditedFiles();

            if (_pendingSync.empty())
            {
                refreshButton().IsEnabled(true);
                co_return;
            }

            // Grab one pending file and push it to the server. Doing the actual
            // transfer on the background thread keeps the UI responsive.
            const auto [remotePath, localPath]{ *_pendingSync.begin() };
            _pendingSync.erase(_pendingSync.begin());

            auto errorMessage{ std::wstring{} };
            auto ok{ false };

            _setStatus(winrt::hstring{ L"Syncing " } + winrt::hstring{ remotePath } + L"...");
            progressRing().IsActive(true);
            progressRing().Visibility(Visibility::Visible);

            co_await resume_background();

            auto progress = SftpTransferProgress{};
            try
            {
                ok = _client.UploadFile(localPath, remotePath, _cancelTransfer, progress, errorMessage);
            }
            catch (...)
            {
                ok = false;
                if (errorMessage.empty())
                {
                    errorMessage = L"Failed to sync with an exception";
                }
            }

            co_await wil::resume_foreground(dispatcher);

            progressRing().IsActive(false);
            progressRing().Visibility(Visibility::Collapsed);

            if (!ok)
            {
                // Don't retry forever with a modal dialog on every 1.5s tick:
                // a persistent failure previously produced an endless stream of
                // error popups. Retry a bounded number of times, then give up
                // on the file with a single, actionable error message.
                auto attempts{ _syncAttempts.find(remotePath) };
                if (attempts == _syncAttempts.end())
                {
                    _syncAttempts.emplace(remotePath, 1);
                }
                else if (++attempts->second >= kMaxSyncAttempts)
                {
                    _syncAttempts.erase(remotePath);
                    _editSessions.erase(remotePath);
                    _pendingSync.erase(remotePath);
                    _editedTimes.erase(remotePath);

                    // Leave the local cached copy on disk so the user can push
                    // it manually with Upload.
                    _setStatus(winrt::hstring{ L"Sync failed; stopped watching " } + winrt::hstring{ remotePath });
                    _showError((errorMessage.empty() ? L"Failed to sync the edited file." : errorMessage) +
                               L"\n\nThe file is left on disk; use Upload to push it manually.");
                    co_return;
                }

                _pendingSync.emplace(remotePath, localPath); // retry on the next tick
                _setStatus(winrt::hstring{ L"Sync failed, retrying: " } + winrt::hstring{ remotePath });
            }
            else
            {
                _syncAttempts.erase(remotePath);
                _setStatus(winrt::hstring{ L"Synced " } + winrt::hstring{ remotePath });
                _statusTimer.Stop();
                _statusTimer.Start();
            }
        }
        catch (...)
        {
            // The try block can still throw from the UI-side setup while the
            // timer is tearing down. Just stop the progress ring from the
            // UI thread without awaiting in the catch block.
            try
            {
                Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [weakThis = get_weak()]() {
                    if (const auto self{ weakThis.get() })
                    {
                        self->progressRing().IsActive(false);
                        self->progressRing().Visibility(Visibility::Collapsed);
                    }
                });
            }
            catch (...)
            {
            }
        }
    }

    void SftpBrowserContent::_statusTimerTick(const IInspectable&, const IInspectable&)
    {
        // The "Synced <path>" confirmation is transient; restore the regular
        // connection/statistics line once it has been visible long enough.
        _statusTimer.Stop();
        if (_isConnected && !_statusText.empty())
        {
            _setStatus(_statusText);
        }
    }

    void SftpBrowserContent::_contentKeyDown(const IInspectable&, const KeyRoutedEventArgs& e)
    {
        // Ctrl+R (or Ctrl+Shift+R) refreshes the current directory listing.
        if (!_isConnected || e.OriginalKey() != winrt::Windows::System::VirtualKey::R)
        {
            return;
        }

        const auto window{ winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread() };
        if (!window)
        {
            return;
        }
        const auto ctrlDown{ WI_IsFlagSet(window.GetKeyState(winrt::Windows::System::VirtualKey::Control), winrt::Windows::UI::Core::CoreVirtualKeyStates::Down) };
        if (!ctrlDown)
        {
            return;
        }

        e.Handled(true);
        _refreshList();
    }

    void SftpBrowserContent::_connectClick(const IInspectable&, const RoutedEventArgs&)
    {
        if (_isConnected || _connecting)
        {
            return;
        }

        _host = hostBox().Text();
        try
        {
            _port = static_cast<uint32_t>(std::stoul(std::wstring{ portBox().Text() }, nullptr, 10));
        }
        catch (...)
        {
            _port = 22;
        }
        _username = userBox().Text();
        _password = passBox().Password();
        _privateKeyPath = keyBox().Text();

        _connectAsync();
    }

    void SftpBrowserContent::_disconnectClick(const IInspectable&, const RoutedEventArgs&)
    {
        _client.Disconnect();
        _isConnected = false;
        _setConnectedUi(false);
        _currentPath = L"/";
        _homePath = L"/";
        _backHistory.clear();
        _forwardHistory.clear();
        _entries.Clear();
        _editSessions.clear();
        _pendingSync.clear();
        _editedTimes.clear();
        _syncAttempts.clear();
        _updateBreadcrumbBar();
        _setStatus(L"Disconnected");
    }

    void SftpBrowserContent::_navigateTo(const std::wstring& path)
    {
        if (!_isConnected || path.empty() || path == _currentPath)
        {
            return;
        }
        _backHistory.push_back(_currentPath);
        _forwardHistory.clear();
        _currentPath = path;
        _updateNavButtons();
        _loadDirectoryAsync();
    }

    void SftpBrowserContent::_updateNavButtons()
    {
        backButton().IsEnabled(!_backHistory.empty());
        forwardButton().IsEnabled(!_forwardHistory.empty());
    }

    void SftpBrowserContent::_backClick(const IInspectable&, const RoutedEventArgs&)
    {
        if (!_isConnected || _backHistory.empty())
        {
            return;
        }
        _forwardHistory.push_back(_currentPath);
        _currentPath = _backHistory.back();
        _backHistory.pop_back();
        _updateNavButtons();
        _loadDirectoryAsync();
    }

    void SftpBrowserContent::_forwardClick(const IInspectable&, const RoutedEventArgs&)
    {
        if (!_isConnected || _forwardHistory.empty())
        {
            return;
        }
        _backHistory.push_back(_currentPath);
        _currentPath = _forwardHistory.back();
        _forwardHistory.pop_back();
        _updateNavButtons();
        _loadDirectoryAsync();
    }

    void SftpBrowserContent::_toggleHiddenFilesClick(const IInspectable&, const RoutedEventArgs&)
    {
        const auto checked{ hiddenButton().IsChecked() };
        _showHiddenFiles = checked && checked.Value();
        _refreshList();
    }

    void SftpBrowserContent::_refreshClick(const IInspectable&, const RoutedEventArgs&)
    {
        _refreshList();
    }

    void SftpBrowserContent::_uploadClick(const IInspectable&, const RoutedEventArgs&)
    {
        if (!_isConnected)
        {
            return;
        }

        const auto hwnd{ _hostingWindow ? reinterpret_cast<HWND>(_hostingWindow) : nullptr };
        auto picker{ OpenFilePicker(hwnd, [](IFileDialog* dialog) {
            COMDLG_FILTERSPEC allFiles[] = { { L"All Files", L"*.*" } };
#pragma warning(suppress : 26485)
            dialog->SetFileTypes(1, allFiles);
            dialog->SetFileTypeIndex(1);
        }) };
        picker.Completed([weak = get_weak()](const IAsyncOperation<hstring>& op, AsyncStatus status) {
            if (status != AsyncStatus::Completed)
            {
                return;
            }
            const auto page{ weak.get() };
            if (!page)
            {
                return;
            }
            try
            {
                const auto path{ op.GetResults() };
                if (path.empty())
                {
                    return;
                }
                StorageFile::GetFileFromPathAsync(path).Completed([weak, path](const IAsyncOperation<StorageFile>& fileOp, AsyncStatus fileStatus) {
                    try
                    {
                        if (fileStatus != AsyncStatus::Completed)
                        {
                            return;
                        }
                        const auto page2{ weak.get() };
                        if (!page2)
                        {
                            return;
                        }
                        std::vector<StorageFile> files{ fileOp.GetResults() };
                        page2->_uploadFilesAsync(winrt::single_threaded_vector<StorageFile>(std::move(files)));
                    }
                    catch (...)
                    {
                        if (const auto self{ weak.get() })
                        {
                            self->_showError(L"Failed to read the selected file.");
                        }
                    }
                });
            }
            catch (...)
            {
                if (const auto self{ weak.get() })
                {
                    self->_showError(L"Failed to open the file picker result.");
                }
            }
        });
    }

    void SftpBrowserContent::_downloadClick(const IInspectable&, const RoutedEventArgs&)
    {
        if (!_isConnected)
        {
            return;
        }

        const auto selected{ fileList().SelectedItems() };
        if (selected.Size() == 0)
        {
            _showError(L"Select a file to download first.");
            return;
        }
        _downloadToFolderAsync(selected.GetAt(0).try_as<TerminalApp::SftpFileEntry>());
    }

    void SftpBrowserContent::_newFolderClick(const IInspectable&, const RoutedEventArgs&)
    {
        _newFolderAsync();
    }

    void SftpBrowserContent::_newFileClick(const IInspectable&, const RoutedEventArgs&)
    {
        _newFileAsync();
    }

    void SftpBrowserContent::_closeClick(const IInspectable&, const RoutedEventArgs&)
    {
        Close();
    }

    void SftpBrowserContent::_saveProfileClick(const IInspectable&, const RoutedEventArgs&)
    {
        _saveProfileAsync();
    }

    fire_and_forget SftpBrowserContent::_saveProfileAsync()
    {
        try
        {
            const auto host{ hostBox().Text() };
            if (host.empty())
            {
                _showError(L"Enter a host first.");
                co_return;
            }

            auto name{ std::wstring{ co_await _promptForInputAsync(L"Save connection profile", L"Profile name", host) } };
            if (name.empty())
            {
                name = std::wstring{ host };
            }

            SftpConnectionProfile profile;
            profile.name = name;
            profile.host = std::wstring{ host };
            try
            {
                profile.port = static_cast<uint32_t>(std::stoul(std::wstring{ portBox().Text() }, nullptr, 10));
            }
            catch (...)
            {
                profile.port = 22;
            }
            profile.username = std::wstring{ userBox().Text() };
            profile.password = std::wstring{ passBox().Password() };
            profile.keyPath = std::wstring{ keyBox().Text() };

            const auto existing{ std::find_if(_profiles.begin(), _profiles.end(), [&](const SftpConnectionProfile& p) { return p.name == profile.name; }) };
            if (existing != _profiles.end())
            {
                *existing = profile;
            }
            else
            {
                _profiles.push_back(profile);
            }

            _saveProfiles();
            _refreshProfilesList();
            profilesListView().SelectedIndex(static_cast<int32_t>(_profiles.size()) - 1);
        }
        catch (...)
        {
            _showError(L"Failed to save the profile.");
        }
    }

    void SftpBrowserContent::_deleteProfileClick(const IInspectable&, const RoutedEventArgs&)
    {
        const auto idx{ profilesListView().SelectedIndex() };
        if (idx >= 0 && idx < static_cast<int32_t>(_profiles.size()))
        {
            _profiles.erase(_profiles.begin() + idx);
            _saveProfiles();
            _refreshProfilesList();
        }
    }

    void SftpBrowserContent::_newProfileClick(const IInspectable&, const RoutedEventArgs&)
    {
        hostBox().Text(L"");
        portBox().Text(L"");
        userBox().Text(L"");
        passBox().Password(L"");
        keyBox().Text(L"");
        profilesListView().SelectedIndex(-1);
    }

    void SftpBrowserContent::_profilesListSelectionChanged(const IInspectable&, const Controls::SelectionChangedEventArgs&)
    {
        const auto idx{ profilesListView().SelectedIndex() };
        if (idx >= 0 && idx < static_cast<int32_t>(_profiles.size()))
        {
            _applyProfile(_profiles[idx]);
        }
    }

    void SftpBrowserContent::_applyProfile(const SftpConnectionProfile& profile)
    {
        hostBox().Text(winrt::hstring{ profile.host });
        portBox().Text(std::to_wstring(profile.port));
        userBox().Text(winrt::hstring{ profile.username });
        passBox().Password(winrt::hstring{ profile.password });
        keyBox().Text(winrt::hstring{ profile.keyPath });
    }

    void SftpBrowserContent::_refreshProfilesList()
    {
        _profileItems.Clear();
        for (const auto& profile : _profiles)
        {
            auto item{ winrt::make_self<SftpProfileItem>() };
            item->Name(winrt::hstring{ profile.name });
            item->Host(winrt::hstring{ profile.host });
            item->Username(winrt::hstring{ profile.username });
            item->AuthMode(winrt::hstring{ profile.keyPath.empty() ? L"password" : L"key" });
            _profileItems.Append(*item);
        }
        profilesListView().ItemsSource(_profileItems);
    }

    std::filesystem::path SftpBrowserContent::_profilesFile() const
    {
        wchar_t localAppData[MAX_PATH]{};
        const auto len{ GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH) };
        if (len == 0 || len >= MAX_PATH)
        {
            return {};
        }
        return std::filesystem::path{ localAppData } / L"Microsoft" / L"Windows Terminal" / L"sftp-profiles.json";
    }

    void SftpBrowserContent::_loadProfiles()
    {
        _profiles.clear();

        const auto path{ _profilesFile() };
        if (path.empty())
        {
            return;
        }

        // One-time migration from the legacy underscore-named file. Load its
        // contents, save them under the new name (which encrypts any legacy
        // plaintext passwords) and delete the old file.
        const auto legacyPath{ path.parent_path() / L"sftp_profiles.json" };
        const auto useLegacy{ std::filesystem::exists(legacyPath) };

        std::ifstream is{ useLegacy ? legacyPath : path, std::ios::binary | std::ios::in };
        if (!is)
        {
            return;
        }
        std::string bytes{ std::istreambuf_iterator<char>{ is }, std::istreambuf_iterator<char>{} };

        // The file is written as UTF-16 (without a BOM) by _saveProfiles.
        if (bytes.size() % sizeof(wchar_t) != 0 || bytes.size() < sizeof(wchar_t))
        {
            return;
        }
        const auto* wide{ reinterpret_cast<const wchar_t*>(bytes.data()) };
        const std::wstring text{ wide, bytes.size() / sizeof(wchar_t) };

        size_t pos{ 0 };
        while (pos < text.size())
        {
            const auto openBrace{ text.find(L'{', pos) };
            if (openBrace == std::wstring::npos)
            {
                break;
            }
            pos = openBrace + 1;

            SftpConnectionProfile profile;
            while (pos < text.size())
            {
                const auto openQuote{ text.find(L'"', pos) };
                if (openQuote == std::wstring::npos)
                {
                    break;
                }
                const auto closeQuote{ FindJsonStringEnd(text, openQuote) };
                if (closeQuote == std::wstring::npos)
                {
                    break;
                }
                const auto key{ JsonUnescape(text.substr(openQuote + 1, closeQuote - openQuote - 1)) };
                pos = closeQuote + 1;

                const auto colon{ text.find(L':', pos) };
                if (colon == std::wstring::npos)
                {
                    break;
                }
                pos = colon + 1;

                while (pos < text.size() && iswspace(text[pos]))
                {
                    ++pos;
                }

                if (pos < text.size() && text[pos] == L'"')
                {
                    const auto valueEnd{ FindJsonStringEnd(text, pos) };
                    if (valueEnd == std::wstring::npos)
                    {
                        break;
                    }
                    auto value{ JsonUnescape(text.substr(pos + 1, valueEnd - pos - 1)) };
                    pos = valueEnd + 1;

                    if (key == L"name")
                    {
                        profile.name = std::move(value);
                    }
                    else if (key == L"host")
                    {
                        profile.host = std::move(value);
                    }
                    else if (key == L"username")
                    {
                        profile.username = std::move(value);
                    }
else if (key == L"password")
{
    profile.password = DecryptPassword(value);
}
                    else if (key == L"keyPath")
                    {
                        profile.keyPath = std::move(value);
                    }
                }
                else
                {
                    const auto numStart{ pos };
                    while (pos < text.size() && (iswdigit(text[pos]) || text[pos] == L'-'))
                    {
                        ++pos;
                    }
                    if (key == L"port")
                    {
                        try
                        {
                            profile.port = static_cast<uint32_t>(std::stoul(text.substr(numStart, pos - numStart), nullptr, 10));
                        }
                        catch (...)
                        {
                            profile.port = 22;
                        }
                    }
                }

                while (pos < text.size() && text[pos] != L',' && text[pos] != L'}')
                {
                    ++pos;
                }
                if (pos < text.size() && text[pos] == L'}')
                {
                    ++pos;
                    break;
                }
                ++pos;
            }

            if (!profile.name.empty() || !profile.host.empty())
            {
                _profiles.push_back(std::move(profile));
            }
        }

        if (useLegacy)
        {
            // Hand the profiles to the new file (re-encrypting the stored
            // passwords in the process) and drop the legacy file.
            _saveProfiles();
            std::error_code ec;
            std::filesystem::remove(legacyPath, ec);
        }

        _refreshProfilesList();
    }

    void SftpBrowserContent::_saveProfiles()
    {
        const auto path{ _profilesFile() };
        if (path.empty())
        {
            return;
        }

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        std::wstring json{ L"[\n" };
        for (size_t i = 0; i < _profiles.size(); ++i)
        {
            const auto& profile{ _profiles[i] };
            json += L"  {\n";
            json += L"    \"name\": \"" + JsonEscape(profile.name) + L"\",\n";
            json += L"    \"host\": \"" + JsonEscape(profile.host) + L"\",\n";
            json += L"    \"port\": " + std::to_wstring(profile.port) + L",\n";
            json += L"    \"username\": \"" + JsonEscape(profile.username) + L"\",\n";
            json += L"    \"password\": \"" + JsonEscape(EncryptPassword(profile.password)) + L"\",\n";
            json += L"    \"keyPath\": \"" + JsonEscape(profile.keyPath) + L"\"\n";
            json += (i + 1 == _profiles.size()) ? L"  }\n" : L"  },\n";
        }
        json += L"]\n";

        std::ofstream os{ path, std::ios::binary | std::ios::out | std::ios::trunc };
        if (!os)
        {
            return;
        }
        os.write(reinterpret_cast<const char*>(json.data()), static_cast<std::streamsize>(json.size() * sizeof(wchar_t)));
    }

    void SftpBrowserContent::_updateBreadcrumbBar()
    {
        pathBreadcrumbBar().Children().Clear();

        if (!_isConnected || _currentPath.empty())
        {
            return;
        }

        const auto splitBreadcrumb = [](const std::wstring& path) {
            std::vector<std::wstring> segments;
            std::wstring current;
            for (const auto ch : path)
            {
                if (ch == L'/')
                {
                    if (!current.empty())
                    {
                        segments.push_back(std::move(current));
                        current.clear();
                    }
                }
                else
                {
                    current += ch;
                }
            }
            if (!current.empty())
            {
                segments.push_back(std::move(current));
            }
            return segments;
        };

        const auto segments{ splitBreadcrumb(_currentPath) };

        auto makeSeparator = []() {
            auto separator{ Controls::TextBlock{} };
            separator.Text(winrt::hstring{ L"\uE76C" });
            separator.FontFamily(Media::FontFamily{ L"Segoe MDL2 Assets" });
            separator.FontSize(10);
            separator.Margin(Thickness{ 4, 0, 4, 0 });
            separator.VerticalAlignment(VerticalAlignment::Center);
            return separator;
        };

        auto makeSegmentButton = [this](const std::wstring& text, const std::wstring& path) {
            auto button{ Controls::Button{} };
            button.Content(box_value(winrt::hstring{ text }));
            button.Background(nullptr);
            button.BorderThickness(Thickness{ 0, 0, 0, 0 });
            button.Padding(Thickness{ 4, 2, 4, 2 });
            button.MinWidth(0);
            button.MinHeight(0);
            button.FontSize(12);
            const auto lifetime{ get_strong() };
            button.Click([lifetime, path](const auto&, const auto&) { lifetime->_navigateTo(path); });
            return button;
        };

        auto makeCrumbText = [](const std::wstring& text) {
            auto crumb{ Controls::TextBlock{} };
            crumb.Text(winrt::hstring{ text });
            crumb.FontSize(12);
            crumb.Opacity(0.8);
            crumb.VerticalAlignment(VerticalAlignment::Center);
            crumb.Margin(Thickness{ 4, 0, 4, 0 });
            return crumb;
        };

        // Home button (house icon, navigates to user's home directory).
        {
            auto homeBtn{ Controls::Button{} };
            auto icon{ Controls::FontIcon{} };
            icon.FontFamily(Media::FontFamily{ L"Segoe MDL2 Assets" });
            icon.FontSize(14);
            icon.Glyph(L"\uE80F");
            homeBtn.Content(icon);
            homeBtn.Background(nullptr);
            homeBtn.BorderThickness(Thickness{ 0, 0, 0, 0 });
            homeBtn.Padding(Thickness{ 4, 2, 4, 2 });
            homeBtn.MinWidth(0);
            homeBtn.MinHeight(0);
            const auto lifetime{ get_strong() };
            const auto hp{ _homePath };
            homeBtn.Click([lifetime, hp](const auto&, const auto&) { lifetime->_navigateTo(hp); });
            pathBreadcrumbBar().Children().Append(homeBtn);
        }

        const auto rootButton{ makeSegmentButton(L"/", L"/") };
        pathBreadcrumbBar().Children().Append(rootButton);

        if (segments.empty())
        {
            return;
        }

        std::wstring accumulated;
        const auto count{ segments.size() };
        for (size_t i = 0; i < count; ++i)
        {
            accumulated += L"/";
            accumulated += segments[i];
            pathBreadcrumbBar().Children().Append(makeSeparator());
            if (i == count - 1)
            {
                pathBreadcrumbBar().Children().Append(makeCrumbText(segments[i]));
            }
            else
            {
                pathBreadcrumbBar().Children().Append(makeSegmentButton(segments[i], accumulated));
            }
        }
    }

    void SftpBrowserContent::_fileListDoubleTapped(const IInspectable&, DoubleTappedRoutedEventArgs const&)
    {
        if (!_isConnected)
        {
            return;
        }
        if (const auto entry{ fileList().SelectedItem().try_as<TerminalApp::SftpFileEntry>() })
        {
            if (entry.IsDirectory())
            {
                _navigateTo(std::wstring{ entry.FullPath() });
            }
            else
            {
                _openInVSCodeAsync(entry);
            }
        }
    }

    void SftpBrowserContent::_fileListRightTapped(const IInspectable&, RightTappedRoutedEventArgs const& e)
    {
        if (!_isConnected)
        {
            return;
        }

        const auto element{ e.OriginalSource().try_as<FrameworkElement>() };
        if (!element)
        {
            return;
        }
        const auto entry{ element.DataContext().try_as<TerminalApp::SftpFileEntry>() };
        if (!entry)
        {
            return;
        }

        fileList().SelectedItem(entry);

        auto flyout{ MenuFlyout{} };

        auto editCode{ MenuFlyoutItem{} };
        editCode.Text(L"Edit (VS Code)");
        editCode.Icon(Microsoft::Terminal::UI::IconPathConverter::IconWUX(L"\xE943"));
        editCode.Click([weak = get_weak(), entry](const IInspectable&, const RoutedEventArgs&) {
            if (const auto page{ weak.get() })
            {
                page->_openInVSCodeAsync(entry);
            }
        });
        flyout.Items().Append(editCode);

        auto download{ MenuFlyoutItem{} };
        download.Text(L"Download");
        download.Icon(Microsoft::Terminal::UI::IconPathConverter::IconWUX(L"\xE896"));
        download.Click([weak = get_weak(), entry](const IInspectable&, const RoutedEventArgs&) {
            if (const auto page{ weak.get() })
            {
                page->_downloadToFolderAsync(entry);
            }
        });
        flyout.Items().Append(download);

        auto rename{ MenuFlyoutItem{} };
        rename.Text(L"Rename");
        rename.Icon(Microsoft::Terminal::UI::IconPathConverter::IconWUX(L"\xE8AC"));
        rename.Click([weak = get_weak(), entry](const IInspectable&, const RoutedEventArgs&) {
            if (const auto page{ weak.get() })
            {
                page->_renameAsync(entry);
            }
        });
        flyout.Items().Append(rename);

        auto remove{ MenuFlyoutItem{} };
        remove.Text(L"Delete");
        remove.Icon(Microsoft::Terminal::UI::IconPathConverter::IconWUX(L"\xE74D"));
        remove.Click([weak = get_weak(), entry](const IInspectable&, const RoutedEventArgs&) {
            if (const auto page{ weak.get() })
            {
                page->_deleteAsync(entry);
            }
        });
        flyout.Items().Append(remove);

        if (!entry.IsDirectory())
        {
            auto chmod{ MenuFlyoutItem{} };
            chmod.Text(L"Permissions");
            chmod.Icon(Microsoft::Terminal::UI::IconPathConverter::IconWUX(L"\xE72E"));
            chmod.Click([weak = get_weak(), entry](const IInspectable&, const RoutedEventArgs&) {
                if (const auto page{ weak.get() })
                {
                    page->_chmodAsync(entry);
                }
            });
            flyout.Items().Append(chmod);
        }

        auto properties{ MenuFlyoutItem{} };
        properties.Text(L"Properties");
        properties.Icon(Microsoft::Terminal::UI::IconPathConverter::IconWUX(L"\xE946"));
        properties.Click([weak = get_weak(), entry](const IInspectable&, const RoutedEventArgs&) {
            if (const auto page{ weak.get() })
            {
                page->_showFileInfoAsync(entry);
            }
        });
        flyout.Items().Append(properties);

        flyout.ShowAt(element);
    }

    void SftpBrowserContent::_fileListDragOver(const IInspectable&, DragEventArgs const& e)
    {
        if (!_isConnected)
        {
            return;
        }
        const auto deferral{ e.GetDeferral() };
        const auto items{ e.DataView().AvailableFormats() };
        bool hasFiles = false;
        for (const auto& format : items)
        {
            if (format == StandardDataFormats::StorageItems())
            {
                hasFiles = true;
                break;
            }
        }
        e.AcceptedOperation(hasFiles ? DataPackageOperation::Copy : DataPackageOperation::None);
        e.DragUIOverride().Caption(winrt::hstring{ L"Upload to " } + winrt::hstring{ _currentPath });
        deferral.Complete();
    }

    fire_and_forget SftpBrowserContent::_fileListDrop(const IInspectable&, DragEventArgs const& e)
    {
        if (!_isConnected)
        {
            co_return;
        }
        try
        {
            const auto items{ co_await e.DataView().GetStorageItemsAsync() };
            std::vector<StorageFile> files;
            for (const auto& item : items)
            {
                if (const auto file{ item.try_as<StorageFile>() })
                {
                    files.push_back(file);
                }
            }
            if (!files.empty())
            {
                _uploadFilesAsync(winrt::single_threaded_vector<StorageFile>(std::move(files)));
            }
        }
        catch (...)
        {
            _showError(L"Failed to read dropped items.");
        }
    }
}
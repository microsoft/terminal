// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "SftpBrowserContent.g.h"
#include "SftpFileEntry.g.h"
#include "SftpProfileItem.g.h"
#include "BasicPaneEvents.h"
#include "SftpClient.h"

#include <atomic>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace winrt::TerminalApp::implementation
{
    struct SftpFileEntry : SftpFileEntryT<SftpFileEntry>
    {
        SftpFileEntry() = default;
        SftpFileEntry(const SftpFileEntry&) = delete;
        SftpFileEntry& operator=(const SftpFileEntry&) = delete;

        winrt::hstring Name() const;
        void Name(const winrt::hstring& value);

        winrt::hstring FullPath() const;
        void FullPath(const winrt::hstring& value);

        winrt::hstring Permissions() const;
        void Permissions(const winrt::hstring& value);

        winrt::hstring SizeText() const;
        void SizeText(const winrt::hstring& value);

        winrt::hstring ModTimeText() const;
        void ModTimeText(const winrt::hstring& value);

        winrt::hstring Glyph() const;
        void Glyph(const winrt::hstring& value);

        bool IsDirectory() const;
        void IsDirectory(bool value);

    private:
        winrt::hstring _name{};
        winrt::hstring _fullPath{};
        winrt::hstring _permissions{};
        winrt::hstring _sizeText{};
        winrt::hstring _modTimeText{};
        winrt::hstring _glyph{};
        bool _isDirectory{ false };
    };

    struct SftpProfileItem : SftpProfileItemT<SftpProfileItem>
    {
        SftpProfileItem() = default;
        SftpProfileItem(const SftpProfileItem&) = delete;
        SftpProfileItem& operator=(const SftpProfileItem&) = delete;

        winrt::hstring Name() const;
        void Name(const winrt::hstring& value);

        winrt::hstring Host() const;
        void Host(const winrt::hstring& value);

        winrt::hstring Username() const;
        void Username(const winrt::hstring& value);

        winrt::hstring AuthMode() const;
        void AuthMode(const winrt::hstring& value);

    private:
        winrt::hstring _name{};
        winrt::hstring _host{};
        winrt::hstring _username{};
        winrt::hstring _authMode{};
    };

    struct SftpBrowserContent : SftpBrowserContentT<SftpBrowserContent>, BasicPaneEvents
    {
    public:
        struct SftpConnectionProfile
        {
            std::wstring name{};
            std::wstring host{};
            uint32_t port{ 22 };
            std::wstring username{};
            std::wstring password{};
            std::wstring keyPath{};
        };

        SftpBrowserContent();
        SftpBrowserContent(const SftpBrowserContent&) = delete;
        SftpBrowserContent& operator=(const SftpBrowserContent&) = delete;

        void Initialize(const winrt::hstring& host,
                        uint32_t port,
                        const winrt::hstring& username,
                        const winrt::hstring& password,
                        const winrt::hstring& privateKeyPath);
        void Navigate(const winrt::hstring& path);
        void SetHostingWindow(uint64_t hwnd);

#pragma region IPaneContent
        winrt::Windows::UI::Xaml::FrameworkElement GetRoot();

        void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings&,
                            const winrt::Microsoft::Terminal::Settings::Model::WindowSettings&) {}

        winrt::Windows::Foundation::Size MinimumSize() { return { 1, 1 }; }
        void Focus(winrt::Windows::UI::Xaml::FocusState reason = winrt::Windows::UI::Xaml::FocusState::Programmatic) { reason; }
        void Close();
        winrt::Microsoft::Terminal::Settings::Model::INewContentArgs GetNewTerminalArgs(BuildStartupKind kind) const;

        winrt::hstring Title();
        uint64_t TaskbarState() { return 0; }
        uint64_t TaskbarProgress() { return 0; }
        bool ReadOnly() { return false; }
        winrt::hstring Icon() const;
        winrt::Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() const noexcept { return nullptr; }
        winrt::Windows::UI::Xaml::Media::Brush BackgroundBrush() { return Background(); }

#pragma endregion

    private:
        friend struct SftpBrowserContentT<SftpBrowserContent>;

        SftpClient _client{};
        winrt::Windows::Foundation::Collections::IObservableVector<TerminalApp::SftpFileEntry> _entries{ nullptr };
        winrt::Windows::Foundation::Collections::IObservableVector<TerminalApp::SftpProfileItem> _profileItems{ nullptr };

        winrt::hstring _host{};
        winrt::hstring _username{};
        winrt::hstring _password{};
        winrt::hstring _privateKeyPath{};
        uint32_t _port{ 22 };
        bool _connecting{ false };
        bool _isConnected{ false };
        uint64_t _hostingWindow{ 0 };

        std::wstring _currentPath{ L"/" };
        std::wstring _homePath{ L"/" };
        std::atomic<bool> _cancelTransfer{ false };

        std::vector<std::wstring> _backHistory;
        std::vector<std::wstring> _forwardHistory;

        // remote path -> locally cached copy for the "edit in VS Code" workflow
        std::map<std::wstring, std::wstring> _editSessions;
        // remote path whose local copy changed and is waiting for the debounce tick
        std::map<std::wstring, std::wstring> _pendingSync;
        winrt::Windows::UI::Xaml::DispatcherTimer _syncTimer{ nullptr };
        std::map<std::wstring, FILETIME> _editedTimes;
        // Consecutive failed upload attempts per remote path. Caps how many
        // sync errors can be produced before we give up on a file, so a
        // persistent failure stops spamming the user instead of looping forever.
        std::map<std::wstring, uint32_t> _syncAttempts;

        // Base status line ("Connected to ... | N item(s)") restored after the
        // transient "Synced ..." message has been shown for a few seconds.
        winrt::hstring _statusText{};
        winrt::Windows::UI::Xaml::DispatcherTimer _statusTimer{ nullptr };

        bool _showHiddenFiles{ false };
        std::vector<SftpConnectionProfile> _profiles;

        // State for the in-pane text input overlay. ContentDialog text boxes
        // don't receive keypresses in XAML Islands, so name/path input is
        // handled by an overlay attached to this UserControl instead.
        std::optional<winrt::hstring> _inputDialogResult;
        bool _inputDialogDone{ false };

        void _setConnectedUi(bool connected);
        void _refreshList();
        void _showError(const std::wstring& message);
        void _showErrorOnUi(const std::wstring& message);
        void _setStatus(const winrt::hstring& text);

        winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> _promptForInputAsync(const winrt::hstring& title, const winrt::hstring& placeholder, const winrt::hstring& initial);

        winrt::fire_and_forget _connectAsync();
        winrt::fire_and_forget _loadDirectoryAsync();
        void _navigateTo(const std::wstring& path);
        void _updateNavButtons();
        winrt::fire_and_forget _showFileInfoAsync(TerminalApp::SftpFileEntry entry);
        winrt::fire_and_forget _uploadFilesAsync(winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Storage::StorageFile> files);
        winrt::fire_and_forget _downloadAsync(TerminalApp::SftpFileEntry entry, winrt::hstring localPath);
        winrt::fire_and_forget _openInVSCodeAsync(TerminalApp::SftpFileEntry entry);
        winrt::fire_and_forget _newFolderAsync();
        winrt::fire_and_forget _newFileAsync();
        winrt::fire_and_forget _renameAsync(TerminalApp::SftpFileEntry entry);
        winrt::fire_and_forget _deleteAsync(TerminalApp::SftpFileEntry entry);
        winrt::fire_and_forget _chmodAsync(TerminalApp::SftpFileEntry entry);
        winrt::fire_and_forget _downloadToFolderAsync(TerminalApp::SftpFileEntry entry);
        winrt::fire_and_forget _pullFileIntoVSCode(std::wstring remotePath);

        std::wstring _localCachePath(const std::wstring& remotePath);
        void _syncEditedFiles();
        void _removeEditSession(const std::wstring& remotePath);

        void _connectClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _disconnectClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _backClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _forwardClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _refreshClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _toggleHiddenFilesClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _uploadClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _downloadClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _newFileClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _newFolderClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _closeClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);

        void _saveProfileClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        winrt::fire_and_forget _saveProfileAsync();
        void _deleteProfileClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _newProfileClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _profilesListSelectionChanged(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs& e);

        void _loadProfiles();
        void _saveProfiles();
        void _refreshProfilesList();
        void _applyProfile(const SftpConnectionProfile& profile);
        std::filesystem::path _profilesFile() const;

        void _updateBreadcrumbBar();
        void _inputOkClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _inputCancelClick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _inputValueBoxKeyDown(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        void _fileListDoubleTapped(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Input::DoubleTappedRoutedEventArgs& e);
        void _fileListRightTapped(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Input::RightTappedRoutedEventArgs& e);
        void _fileListDragOver(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::DragEventArgs& e);
        winrt::fire_and_forget _fileListDrop(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::DragEventArgs& e);
        winrt::fire_and_forget _syncTimerTick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& e);
        winrt::fire_and_forget _disconnectAsync();
        void _statusTimerTick(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& e);
        void _contentKeyDown(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(SftpBrowserContent);
    BASIC_FACTORY(SftpFileEntry);
    BASIC_FACTORY(SftpProfileItem);
}
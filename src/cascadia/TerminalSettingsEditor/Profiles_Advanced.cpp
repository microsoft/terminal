// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles_Advanced.h"
#include "Profiles_Advanced.g.cpp"
#include "ProfileViewModel.h"

#include "EnumEntry.h"
#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Profiles_Advanced::Profiles_Advanced()
    {
        InitializeComponent();

        Automation::AutomationProperties::SetName(AddBellSoundButton(), RS_(L"Profile_AddBellSound/Text"));
    }

    void Profiles_Advanced::OnNavigatedTo(const NavigationEventArgs& e)
    {
        const auto args = e.Parameter().as<Editor::NavigateToProfileArgs>();
        _Profile = args.Profile();
        _windowRoot = args.WindowRoot();

        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "NavigatedToPage",
            TraceLoggingDescription("Event emitted when the user navigates to a page in the settings UI"),
            TraceLoggingValue("profile.advanced", "PageId", "The identifier of the page that was navigated to"),
            TraceLoggingValue(_Profile.IsBaseLayer(), "IsProfileDefaults", "If the modified profile is the profile.defaults object"),
            TraceLoggingValue(static_cast<GUID>(_Profile.Guid()), "ProfileGuid", "The guid of the profile that was navigated to"),
            TraceLoggingValue(_Profile.Source().c_str(), "ProfileSource", "The source of the profile that was navigated to"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    void Profiles_Advanced::OnNavigatedFrom(const NavigationEventArgs& /*e*/)
    {
        _ViewModelChangedRevoker.revoke();
    }

    safe_void_coroutine Profiles_Advanced::BellSoundAudioPreview_Click(const IInspectable& sender, const RoutedEventArgs& /*e*/)
    try
    {
        const auto path = sender.as<Button>().Tag().as<Editor::BellSoundViewModel>().Path();
        if (path.empty())
        {
            co_return;
        }
        const winrt::hstring soundPath{ wil::ExpandEnvironmentStringsW<std::wstring>(path.c_str()) };
        const winrt::Windows::Foundation::Uri uri{ soundPath };

        if (!_bellPlayerCreated)
        {
            // The MediaPlayer might not exist on Windows N SKU.
            try
            {
                _bellPlayerCreated = true;
                _bellPlayer = winrt::Windows::Media::Playback::MediaPlayer();
                // GH#12258: The media keys (like play/pause) should have no effect on our bell sound.
                _bellPlayer.CommandManager().IsEnabled(false);
            }
            CATCH_LOG();
        }
        if (_bellPlayer)
        {
            const auto source{ winrt::Windows::Media::Core::MediaSource::CreateFromUri(uri) };
            const auto item{ winrt::Windows::Media::Playback::MediaPlaybackItem(source) };
            _bellPlayer.Source(item);
            _bellPlayer.Play();
        }
    }
    CATCH_LOG();

    void Profiles_Advanced::BellSoundDelete_Click(const IInspectable& sender, const RoutedEventArgs& /*e*/)
    {
        const auto bellSoundEntry = sender.as<Button>().Tag().as<Editor::BellSoundViewModel>();
        _Profile.RequestDeleteBellSound(bellSoundEntry);
    }

    void Profiles_Advanced::BellSoundAdd_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _PickFileForBellSound();
    }

    safe_void_coroutine Profiles_Advanced::_PickFileForBellSound()
    {
        static constexpr COMDLG_FILTERSPEC supportedFileTypes[] = {
            { L"Sound Files (*.wav;*.mp3;*.flac)", L"*.wav;*.mp3;*.flac" },
            { L"All Files (*.*)", L"*.*" }
        };

        const auto parentHwnd{ reinterpret_cast<HWND>(WindowRoot().GetHostingWindow()) };
        auto file = co_await OpenFilePicker(parentHwnd, [](auto&& dialog) {
            try
            {
                auto folderShellItem{ winrt::capture<IShellItem>(&SHGetKnownFolderItem, FOLDERID_Music, KF_FLAG_DEFAULT, nullptr) };
                dialog->SetDefaultFolder(folderShellItem.get());
            }
            CATCH_LOG(); // non-fatal
            THROW_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(supportedFileTypes), supportedFileTypes));
            THROW_IF_FAILED(dialog->SetFileTypeIndex(1)); // the array is 1-indexed
            THROW_IF_FAILED(dialog->SetDefaultExtension(L"wav;mp3;flac"));
        });
        if (!file.empty())
        {
            _Profile.RequestAddBellSound(file);
        }
    }
}

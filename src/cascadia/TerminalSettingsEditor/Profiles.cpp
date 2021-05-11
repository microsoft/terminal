// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles.h"
#include "Profiles.g.cpp"
#include "EnumEntry.h"

#include <LibraryResources.h>

using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

static const std::array<winrt::guid, 2> InBoxProfileGuids{
    winrt::guid{ 0x61c54bbd, 0xc2c6, 0x5271, { 0x96, 0xe7, 0x00, 0x9a, 0x87, 0xff, 0x44, 0xbf } }, // Windows Powershell
    winrt::guid{ 0x0caa0dad, 0x35be, 0x5f56, { 0xa8, 0xff, 0xaf, 0xce, 0xee, 0xaa, 0x61, 0x01 } } // Command Prompt
};

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    ProfileViewModel::ProfileViewModel(const Model::Profile& profile) :
        _profile{ profile },
        _defaultAppearanceViewModel{ winrt::make<implementation::AppearanceViewModel>(profile.DefaultAppearance().try_as<AppearanceConfig>()) }
    {
        // Add a property changed handler to our own property changed event.
        // This propagates changes from the settings model to anybody listening to our
        //  unique view model members.
        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            const auto viewModelProperty{ args.PropertyName() };
            if (viewModelProperty == L"IsBaseLayer")
            {
                // we _always_ want to show the background image settings in base layer
                _NotifyChanges(L"BackgroundImageSettingsVisible");
            }
            else if (viewModelProperty == L"StartingDirectory")
            {
                // notify listener that all starting directory related values might have changed
                // NOTE: this is similar to what is done with BackgroundImagePath above
                _NotifyChanges(L"UseParentProcessDirectory", L"UseCustomStartingDirectory");
            }
        });

        // Do the same for the starting directory
        if (!StartingDirectory().empty())
        {
            _lastStartingDirectoryPath = StartingDirectory();
        }
    }

    bool ProfileViewModel::CanDeleteProfile() const
    {
        const auto guid{ Guid() };
        if (IsBaseLayer())
        {
            return false;
        }
        else if (std::find(std::begin(InBoxProfileGuids), std::end(InBoxProfileGuids), guid) != std::end(InBoxProfileGuids))
        {
            // in-box profile
            return false;
        }
        else if (!Source().empty())
        {
            // dynamic profile
            return false;
        }
        else
        {
            return true;
        }
    }

    Editor::AppearanceViewModel ProfileViewModel::DefaultAppearance()
    {
        return _defaultAppearanceViewModel;
    }

    bool ProfileViewModel::UseParentProcessDirectory()
    {
        return StartingDirectory().empty();
    }

    // This function simply returns the opposite of UseParentProcessDirectory.
    // We bind the 'IsEnabled' parameters of the textbox and browse button
    // to this because it needs to be the reverse of UseParentProcessDirectory
    // but we don't want to create a whole new converter for inverting a boolean
    bool ProfileViewModel::UseCustomStartingDirectory()
    {
        return !UseParentProcessDirectory();
    }

    void ProfileViewModel::UseParentProcessDirectory(const bool useParent)
    {
        if (useParent)
        {
            // Stash the current value of StartingDirectory. If the user
            // checks and un-checks the "Use parent process directory" button, we want
            // the path that we display in the text box to remain unchanged.
            //
            // Only stash this value if it's not empty
            if (!StartingDirectory().empty())
            {
                _lastStartingDirectoryPath = StartingDirectory();
            }
            StartingDirectory(L"");
        }
        else
        {
            // Restore the path we had previously cached as long as it wasn't empty
            // If it was empty, set the starting directory to %USERPROFILE%
            // (we need to set it to something non-empty otherwise we will automatically
            // disable the text box)
            if (_lastStartingDirectoryPath.empty())
            {
                StartingDirectory(L"%USERPROFILE%");
            }
            else
            {
                StartingDirectory(_lastStartingDirectoryPath);
            }
        }
    }

    void ProfilePageNavigationState::DeleteProfile()
    {
        auto deleteProfileArgs{ winrt::make_self<DeleteProfileEventArgs>(_Profile.Guid()) };
        _DeleteProfileHandlers(*this, *deleteProfileArgs);
    }

    Profiles::Profiles()
    {
        InitializeComponent();

        INITIALIZE_BINDABLE_ENUM_SETTING(AntiAliasingMode, TextAntialiasingMode, winrt::Microsoft::Terminal::Control::TextAntialiasingMode, L"Profile_AntialiasingMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING_REVERSE_ORDER(CloseOnExitMode, CloseOnExitMode, winrt::Microsoft::Terminal::Settings::Model::CloseOnExitMode, L"Profile_CloseOnExit", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING_REVERSE_ORDER(BellStyle, BellStyle, winrt::Microsoft::Terminal::Settings::Model::BellStyle, L"Profile_BellStyle", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(ScrollState, ScrollbarState, winrt::Microsoft::Terminal::Control::ScrollbarState, L"Profile_ScrollbarVisibility", L"Content");

        const auto startingDirCheckboxTooltip{ ToolTipService::GetToolTip(StartingDirectoryUseParentCheckbox()) };
        Automation::AutomationProperties::SetFullDescription(StartingDirectoryUseParentCheckbox(), unbox_value<hstring>(startingDirCheckboxTooltip));

        Automation::AutomationProperties::SetName(DeleteButton(), RS_(L"Profile_DeleteButton/Text"));
    }

    void Profiles::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::ProfilePageNavigationState>();

        // Set the text disclaimer for the text box
        hstring disclaimer{};
        const auto guid{ _State.Profile().Guid() };
        if (std::find(std::begin(InBoxProfileGuids), std::end(InBoxProfileGuids), guid) != std::end(InBoxProfileGuids))
        {
            // load disclaimer for in-box profiles
            disclaimer = RS_(L"Profile_DeleteButtonDisclaimerInBox");
        }
        else if (!_State.Profile().Source().empty())
        {
            // load disclaimer for dynamic profiles
            disclaimer = RS_(L"Profile_DeleteButtonDisclaimerDynamic");
        }
        DeleteButtonDisclaimer().Text(disclaimer);

        // Check the use parent directory box if the starting directory is empty
        if (_State.Profile().StartingDirectory().empty())
        {
            StartingDirectoryUseParentCheckbox().IsChecked(true);
        }

        // Subscribe to some changes in the view model
        // These changes should force us to update our own set of "Current<Setting>" members,
        // and propagate those changes to the UI
        _ViewModelChangedRevoker = _State.Profile().PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
            const auto settingName{ args.PropertyName() };
            if (settingName == L"AntialiasingMode")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentAntiAliasingMode" });
            }
            else if (settingName == L"CloseOnExit")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentCloseOnExitMode" });
            }
            else if (settingName == L"BellStyle")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentBellStyle" });
            }
            else if (settingName == L"ScrollState")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentScrollState" });
            }
        });

        // Navigate to the pivot in the provided navigation state
        ProfilesPivot().SelectedIndex(static_cast<int>(_State.LastActivePivot()));
    }

    void Profiles::OnNavigatedFrom(const NavigationEventArgs& /*e*/)
    {
        _ViewModelChangedRevoker.revoke();
    }

    void Profiles::DeleteConfirmation_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        auto state{ winrt::get_self<ProfilePageNavigationState>(_State) };
        state->DeleteProfile();
    }

    fire_and_forget Profiles::Icon_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        const auto parentHwnd{ reinterpret_cast<HWND>(_State.WindowRoot().GetHostingWindow()) };
        auto file = co_await OpenImagePicker(parentHwnd);
        if (!file.empty())
        {
            _State.Profile().Icon(file);
        }
    }

    fire_and_forget Profiles::Commandline_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        static constexpr COMDLG_FILTERSPEC supportedFileTypes[] = {
            { L"Executable Files (*.exe, *.cmd, *.bat)", L"*.exe;*.cmd;*.bat" },
            { L"All Files (*.*)", L"*.*" }
        };

        static constexpr winrt::guid clientGuidExecutables{ 0x2E7E4331, 0x0800, 0x48E6, { 0xB0, 0x17, 0xA1, 0x4C, 0xD8, 0x73, 0xDD, 0x58 } };
        const auto parentHwnd{ reinterpret_cast<HWND>(_State.WindowRoot().GetHostingWindow()) };
        auto path = co_await OpenFilePicker(parentHwnd, [](auto&& dialog) {
            THROW_IF_FAILED(dialog->SetClientGuid(clientGuidExecutables));
            try
            {
                auto folderShellItem{ winrt::capture<IShellItem>(&SHGetKnownFolderItem, FOLDERID_ComputerFolder, KF_FLAG_DEFAULT, nullptr) };
                dialog->SetDefaultFolder(folderShellItem.get());
            }
            CATCH_LOG(); // non-fatal
            THROW_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(supportedFileTypes), supportedFileTypes));
            THROW_IF_FAILED(dialog->SetFileTypeIndex(1)); // the array is 1-indexed
            THROW_IF_FAILED(dialog->SetDefaultExtension(L"exe;cmd;bat"));
        });

        if (!path.empty())
        {
            _State.Profile().Commandline(path);
        }
    }

    fire_and_forget Profiles::StartingDirectory_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto lifetime = get_strong();
        const auto parentHwnd{ reinterpret_cast<HWND>(_State.WindowRoot().GetHostingWindow()) };
        auto folder = co_await OpenFilePicker(parentHwnd, [](auto&& dialog) {
            static constexpr winrt::guid clientGuidFolderPicker{ 0xAADAA433, 0xB04D, 0x4BAE, { 0xB1, 0xEA, 0x1E, 0x6C, 0xD1, 0xCD, 0xA6, 0x8B } };
            THROW_IF_FAILED(dialog->SetClientGuid(clientGuidFolderPicker));
            try
            {
                auto folderShellItem{ winrt::capture<IShellItem>(&SHGetKnownFolderItem, FOLDERID_ComputerFolder, KF_FLAG_DEFAULT, nullptr) };
                dialog->SetDefaultFolder(folderShellItem.get());
            }
            CATCH_LOG(); // non-fatal

            DWORD flags{};
            THROW_IF_FAILED(dialog->GetOptions(&flags));
            THROW_IF_FAILED(dialog->SetOptions(flags | FOS_PICKFOLDERS)); // folders only
        });

        if (!folder.empty())
        {
            _State.Profile().StartingDirectory(folder);
        }
    }

    void Profiles::Pivot_SelectionChanged(Windows::Foundation::IInspectable const& /*sender*/,
                                          RoutedEventArgs const& /*e*/)
    {
        _State.LastActivePivot(static_cast<Editor::ProfilesPivots>(ProfilesPivot().SelectedIndex()));
    }
}

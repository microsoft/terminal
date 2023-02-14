// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles_Base.h"
#include "Profiles_Base.g.cpp"
#include "ProfileViewModel.h"

#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Profiles_Base::Profiles_Base()
    {
        InitializeComponent();

        const auto startingDirCheckboxTooltip{ ToolTipService::GetToolTip(StartingDirectoryUseParentCheckbox()) };
        Automation::AutomationProperties::SetFullDescription(StartingDirectoryUseParentCheckbox(), unbox_value<hstring>(startingDirCheckboxTooltip));

        Automation::AutomationProperties::SetName(DeleteButton(), RS_(L"Profile_DeleteButton/Text"));
        AppearanceNavigator().Content(box_value(RS_(L"Profile_Appearance/Header")));
        AdvancedNavigator().Content(box_value(RS_(L"Profile_Advanced/Header")));
    }

    void Profiles_Base::OnNavigatedTo(const NavigationEventArgs& e)
    {
        const auto args = e.Parameter().as<Editor::NavigateToProfileArgs>();
        _Profile = args.Profile();
        _windowRoot = args.WindowRoot();

        // Check the use parent directory box if the starting directory is empty
        if (_Profile.StartingDirectory().empty())
        {
            StartingDirectoryUseParentCheckbox().IsChecked(true);
        }

        _layoutUpdatedRevoker = LayoutUpdated(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            // This event fires every time the layout changes, but it is always the last one to fire
            // in any layout change chain. That gives us great flexibility in finding the right point
            // at which to initialize our renderer (and our terminal).
            // Any earlier than the last layout update and we may not know the terminal's starting size.

            // Only let this succeed once.
            _layoutUpdatedRevoker.revoke();

            if (_Profile.FocusDeleteButton())
            {
                DeleteButton().Focus(FocusState::Programmatic);
                _Profile.FocusDeleteButton(false);
                ProfilesBase_ScrollView().ChangeView(nullptr, ProfilesBase_ScrollView().ScrollableHeight(), nullptr);
            }
        });
    }

    void Profiles_Base::OnNavigatedFrom(const NavigationEventArgs& /*e*/)
    {
        _ViewModelChangedRevoker.revoke();
    }

    void Profiles_Base::Appearance_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*args*/)
    {
        _Profile.CurrentPage(ProfileSubPage::Appearance);
    }

    void Profiles_Base::Advanced_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*args*/)
    {
        _Profile.CurrentPage(ProfileSubPage::Advanced);
    }

    void Profiles_Base::DeleteConfirmation_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        winrt::get_self<ProfileViewModel>(_Profile)->DeleteProfile();
    }

    fire_and_forget Profiles_Base::Commandline_Click(const IInspectable&, const RoutedEventArgs&)
    {
        auto lifetime = get_strong();

        static constexpr COMDLG_FILTERSPEC supportedFileTypes[] = {
            { L"Executable Files (*.exe, *.cmd, *.bat)", L"*.exe;*.cmd;*.bat" },
            { L"All Files (*.*)", L"*.*" }
        };

        static constexpr winrt::guid clientGuidExecutables{ 0x2E7E4331, 0x0800, 0x48E6, { 0xB0, 0x17, 0xA1, 0x4C, 0xD8, 0x73, 0xDD, 0x58 } };
        const auto parentHwnd{ reinterpret_cast<HWND>(_windowRoot.GetHostingWindow()) };
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
            _Profile.Commandline(path);
        }
    }

    fire_and_forget Profiles_Base::Icon_Click(const IInspectable&, const RoutedEventArgs&)
    {
        auto lifetime = get_strong();

        const auto parentHwnd{ reinterpret_cast<HWND>(_windowRoot.GetHostingWindow()) };
        auto file = co_await OpenImagePicker(parentHwnd);
        if (!file.empty())
        {
            _Profile.Icon(file);
        }
    }

    fire_and_forget Profiles_Base::StartingDirectory_Click(const IInspectable&, const RoutedEventArgs&)
    {
        auto lifetime = get_strong();
        const auto parentHwnd{ reinterpret_cast<HWND>(_windowRoot.GetHostingWindow()) };
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
            _Profile.StartingDirectory(folder);
        }
    }
}

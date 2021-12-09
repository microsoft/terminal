// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles.h"

#include "PreviewConnection.h"
#include "Profiles.g.cpp"

#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Profiles::Profiles()
    {
        InitializeComponent();
    }

    void Profiles::OnNavigatedTo(const NavigationEventArgs& e)
    {
        auto state{ e.Parameter().as<Editor::ProfilePageNavigationState>() };
        _Profile = state.Profile();

        ProfilesContentFrame().Navigate(xaml_typename<Editor::Profiles_Base>(), _State);

        // TODO: COME BACK TO THIS
        //// generate the font list, if we don't have one
        //if (!_State.Profile().CompleteFontList() || !_State.Profile().MonospaceFontList())
        //{
        //    ProfileViewModel::UpdateFontList();
        //}

        // Check the use parent directory box if the starting directory is empty
        // TODO: COME BACK TO THIS
        //if (_State.Profile().StartingDirectory().empty())
        //{
        //    StartingDirectoryUseParentCheckbox().IsChecked(true);
        //}

        // TODO: COME BACK TO THIS
        //// Subscribe to some changes in the view model
        //// These changes should force us to update our own set of "Current<Setting>" members,
        //// and propagate those changes to the UI
        _ViewModelChangedRevoker = _State.Profile().PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
            const auto settingName{ args.PropertyName() };
            if (settingName == L"CurrentPage")
            {
                const auto currentPage = _State.Profile().CurrentPage();
                if (currentPage == L"Base")
                {
                    ProfilesContentFrame().Navigate(xaml_typename<Editor::Profiles_Base>(), _State);
                }
                else if (currentPage == L"Appearance")
                {
                    ProfilesContentFrame().Navigate(xaml_typename<Editor::Profiles_Appearance>(), _State);
                }
                else if (currentPage == L"Advanced")
                {
                    ProfilesContentFrame().Navigate(xaml_typename<Editor::Profiles_Advanced>(), _State);
                }
            }
            //if (settingName == L"AntialiasingMode")
            //{
            //    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentAntiAliasingMode" });
            //}
            //else if (settingName == L"CloseOnExit")
            //{
            //    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentCloseOnExitMode" });
            //}
            //else if (settingName == L"BellStyle")
            //{
            //    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsBellStyleFlagSet" });
            //}
            //else if (settingName == L"ScrollState")
            //{
            //    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentScrollState" });
            //}
            //_previewControl.Settings(_State.Profile().TermSettings());
            //_previewControl.UpdateSettings();
        });

        // TODO: COME BACK TO THIS
        //// The Appearances object handles updating the values in the settings UI, but
        //// we still need to listen to the changes here just to update the preview control
        //_AppearanceViewModelChangedRevoker = _State.Profile().DefaultAppearance().PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& /*args*/) {
        //    _previewControl.Settings(_State.Profile().TermSettings());
        //    _previewControl.UpdateSettings();
        //});

        // TODO: COME BACK TO THIS
        //_previewControl.Settings(_State.Profile().TermSettings());
        //// There is a possibility that the control has not fully initialized yet,
        //// so wait for it to initialize before updating the settings (so we know
        //// that the renderer is set up)
        //_previewControl.Initialized([&](auto&& /*s*/, auto&& /*e*/) {
        //    _previewControl.UpdateSettings();
        //});
    }

    void Profiles::OnNavigatedFrom(const NavigationEventArgs& /*e*/)
    {
        _ViewModelChangedRevoker.revoke();
        _AppearanceViewModelChangedRevoker.revoke();
    }
}

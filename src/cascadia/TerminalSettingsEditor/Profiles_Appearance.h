// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Profiles_Appearance.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Profiles_Appearance : public HasScrollViewer<Profiles_Appearance>, Profiles_AppearanceT<Profiles_Appearance>
    {
    public:
        Profiles_Appearance();

        void OnNavigatedTo(const WUX::Navigation::NavigationEventArgs& e);
        void OnNavigatedFrom(const WUX::Navigation::NavigationEventArgs& e);

        void CreateUnfocusedAppearance_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& e);
        void DeleteUnfocusedAppearance_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& e);

        WINRT_CALLBACK(PropertyChanged, WUX::Data::PropertyChangedEventHandler);
        WINRT_PROPERTY(Editor::ProfileViewModel, Profile, nullptr);

    private:
        Microsoft::Terminal::Control::TermControl _previewControl;
        WUX::Data::INotifyPropertyChanged::PropertyChanged_revoker _ViewModelChangedRevoker;
        WUX::Data::INotifyPropertyChanged::PropertyChanged_revoker _AppearanceViewModelChangedRevoker;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles_Appearance);
}

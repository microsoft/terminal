// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Profiles_Base.g.h"
#include "ViewModelHelpers.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Profiles_Base : public HasScrollViewer<Profiles_Base>, Profiles_BaseT<Profiles_Base>
    {
    public:
        Profiles_Base();

        void OnNavigatedTo(const WUX::Navigation::NavigationEventArgs& e);
        void OnNavigatedFrom(const WUX::Navigation::NavigationEventArgs& e);

        fire_and_forget StartingDirectory_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& e);
        fire_and_forget Icon_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& e);
        fire_and_forget Commandline_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& e);
        void Appearance_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& e);
        void Advanced_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& e);
        void DeleteConfirmation_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& e);

        WINRT_CALLBACK(PropertyChanged, WUX::Data::PropertyChangedEventHandler);
        WINRT_PROPERTY(Editor::ProfileViewModel, Profile, nullptr);

    private:
        WUX::Data::INotifyPropertyChanged::PropertyChanged_revoker _ViewModelChangedRevoker;
        WUXC::SwapChainPanel::LayoutUpdated_revoker _layoutUpdatedRevoker;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles_Base);
}

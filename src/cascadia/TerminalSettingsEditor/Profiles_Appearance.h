// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <ThrottledFunc.h>

#include "PreviewConnection.h"
#include "Utils.h"

#include "Profiles_Appearance.g.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Profiles_Appearance : public HasScrollViewer<Profiles_Appearance>, Profiles_AppearanceT<Profiles_Appearance>
    {
    public:
        Profiles_Appearance();

        void OnNavigatedTo(const Windows::UI::Xaml::Navigation::NavigationEventArgs& e);
        void OnNavigatedFrom(const Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        void CreateUnfocusedAppearance_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void DeleteUnfocusedAppearance_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);

        Editor::IHostedInWindow WindowRoot() { return _windowRoot; };

        til::property_changed_event PropertyChanged;
        WINRT_PROPERTY(Editor::ProfileViewModel, Profile, nullptr);

    private:
        void _onProfilePropertyChanged(const IInspectable&, const PropertyChangedEventArgs&);

        winrt::com_ptr<PreviewConnection> _previewConnection{ nullptr };
        Microsoft::Terminal::Control::TermControl _previewControl{ nullptr };
        std::shared_ptr<ThrottledFunc<>> _updatePreviewControl;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _ViewModelChangedRevoker;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _AppearanceViewModelChangedRevoker;
        Editor::IHostedInWindow _windowRoot;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles_Appearance);
}

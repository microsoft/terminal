// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Profiles_Advanced.g.h"
#include "ViewModelHelpers.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Profiles_Advanced : public HasScrollViewer<Profiles_Advanced>, Profiles_AdvancedT<Profiles_Advanced>
    {
    public:
        Profiles_Advanced();

        void OnNavigatedTo(const Windows::UI::Xaml::Navigation::NavigationEventArgs& e);
        void OnNavigatedFrom(const Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        safe_void_coroutine BellSoundAudioPreview_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void BellSoundDelete_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void BellSoundAdd_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);

        til::property_changed_event PropertyChanged;
        Editor::IHostedInWindow WindowRoot() { return _windowRoot; };
        WINRT_PROPERTY(Editor::ProfileViewModel, Profile, nullptr);

    private:
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _ViewModelChangedRevoker;
        Editor::IHostedInWindow _windowRoot;
        winrt::Windows::Media::Playback::MediaPlayer _bellPlayer{ nullptr };
        bool _bellPlayerCreated{ false };

        safe_void_coroutine _PickFileForBellSound();
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles_Advanced);
}

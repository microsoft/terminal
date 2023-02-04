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

        void OnNavigatedTo(const WUX::Navigation::NavigationEventArgs& e);
        void OnNavigatedFrom(const WUX::Navigation::NavigationEventArgs& e);

        WINRT_CALLBACK(PropertyChanged, WUX::Data::PropertyChangedEventHandler);
        WINRT_PROPERTY(Editor::ProfileViewModel, Profile, nullptr);

    private:
        WUX::Data::INotifyPropertyChanged::PropertyChanged_revoker _ViewModelChangedRevoker;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles_Advanced);
}

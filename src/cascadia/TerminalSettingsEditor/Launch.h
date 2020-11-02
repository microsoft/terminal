// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Launch.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Launch : LaunchT<Launch>
    {
    public:
        Launch();

        void DefaultProfileSelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

        winrt::Microsoft::Terminal::Settings::Model::GlobalAppSettings GlobalSettings();

        Windows::Foundation::Collections::IObservableVector<hstring> ProfileNameList();

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(Model::Profile, CurrentDefaultProfile, _PropertyChangedHandlers, nullptr);

    private:
        Windows::Foundation::Collections::IObservableVector<hstring> _profileNameList;
        void _UpdateProfileNameList(Windows::Foundation::Collections::IObservableVector<Model::Profile> profiles);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Launch);
}

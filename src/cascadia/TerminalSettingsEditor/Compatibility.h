// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Compatibility.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Compatibility : public HasScrollViewer<Compatibility>, CompatibilityT<Compatibility>
    {
        Compatibility();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(Editor::CompatibilityViewModel, ViewModel, _PropertyChangedHandlers, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Compatibility);
}

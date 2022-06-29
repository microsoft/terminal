// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "EditColorScheme.g.h"
#include "ColorSchemeViewModel.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct EditColorScheme : public HasScrollViewer<EditColorScheme>, EditColorSchemeT<EditColorScheme>
    {
        EditColorScheme();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        void ColorPickerChanged(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Microsoft::UI::Xaml::Controls::ColorChangedEventArgs& args);

        WINRT_OBSERVABLE_PROPERTY(Editor::ColorSchemeViewModel, ViewModel, _PropertyChangedHandlers, nullptr);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(EditColorScheme);
}

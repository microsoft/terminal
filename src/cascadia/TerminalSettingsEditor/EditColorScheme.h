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
        void RenameAccept_Click(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void RenameCancel_Click(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void NameBox_PreviewKeyDown(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);

        til::property_changed_event PropertyChanged;

        WINRT_OBSERVABLE_PROPERTY(Editor::ColorSchemeViewModel, ViewModel, PropertyChanged.raise, nullptr);

    private:
        void _RenameCurrentScheme(hstring newName);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(EditColorScheme);
}

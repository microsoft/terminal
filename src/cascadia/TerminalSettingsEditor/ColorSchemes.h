// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ColorTableEntry.g.h"
#include "ColorSchemes.g.h"
#include "ColorSchemeViewModel.h"
#include "ColorSchemesPageViewModel.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ColorSchemes : public HasScrollViewer<ColorSchemes>, ColorSchemesT<ColorSchemes>
    {
        ColorSchemes();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        void ColorSchemeSelectionChanged(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs& args);
        void ColorPickerChanged(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Microsoft::UI::Xaml::Controls::ColorChangedEventArgs& args);
        void AddNew_Click(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);

        void Rename_Click(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void RenameAccept_Click(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void RenameCancel_Click(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void NameBox_PreviewKeyDown(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);

        void DeleteConfirmation_Click(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);

        WINRT_PROPERTY(Model::ColorScheme, CurrentColorScheme, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorSchemesPageViewModel, ViewModel, _PropertyChangedHandlers, nullptr);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(bool, IsRenaming, _PropertyChangedHandlers, nullptr);

    private:
        void _RenameCurrentScheme(hstring newName);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ColorSchemes);
}

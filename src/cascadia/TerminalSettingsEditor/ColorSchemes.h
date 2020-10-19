// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ColorTableEntry.g.h"
#include "ColorSchemes.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ColorSchemes : ColorSchemesT<ColorSchemes>
    {
        ColorSchemes();

        void ColorSchemeSelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void ColorPickerChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::ColorChangedEventArgs const& args);

        Windows::Foundation::Collections::IObservableVector<winrt::hstring> ColorSchemeList();

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(winrt::Microsoft::Terminal::Settings::Model::ColorScheme, CurrentColorScheme, _PropertyChangedHandlers, nullptr);
        GETSET_PROPERTY(Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Editor::ColorTableEntry>, CurrentColorTable, nullptr);

    private:
        Windows::Foundation::Collections::IObservableVector<winrt::hstring> _ColorSchemeList{ nullptr };

        void _UpdateColorTable(const winrt::Microsoft::Terminal::Settings::Model::ColorScheme& colorScheme);
        void _UpdateColorSchemeList();
    };

    struct ColorTableEntry : ColorTableEntryT<ColorTableEntry>
    {
    public:
        ColorTableEntry() = default;
        ColorTableEntry(uint32_t index, Windows::UI::Color color);

        Windows::UI::Xaml::Media::Brush ColorToBrush(Windows::UI::Color color);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, Name, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(IInspectable, Index, _PropertyChangedHandlers, nullptr);
        OBSERVABLE_GETSET_PROPERTY(Windows::UI::Color, Color, _PropertyChangedHandlers);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ColorSchemes);
}

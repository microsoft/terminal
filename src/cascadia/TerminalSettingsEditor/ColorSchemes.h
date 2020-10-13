// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ColorSchemes.g.h"
#include "ColorTableEntry.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ColorSchemes : ColorSchemesT<ColorSchemes>
    {
        ColorSchemes();

        GETSET_PROPERTY(winrt::Microsoft::Terminal::Settings::Model::ColorScheme, ColorScheme, nullptr);
        GETSET_PROPERTY(Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Editor::ColorTableEntry>, ColorTable, nullptr);
    };

    struct ColorTableEntry : ColorTableEntryT<ColorTableEntry>
    {
    public:
        ColorTableEntry() = default;
        ColorTableEntry(const winrt::hstring& name, const Windows::UI::Color& color);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, Name, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(Windows::UI::Color, Color, _PropertyChangedHandlers);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ColorSchemes);
}

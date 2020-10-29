// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "GlobalAppearance.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct GlobalAppearance : GlobalAppearanceT<GlobalAppearance>
    {
    public:
        GlobalAppearance();
        winrt::Microsoft::Terminal::Settings::Model::GlobalAppSettings GlobalSettings();
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> ElementThemes();

        uint8_t CurrentTheme();
        void CurrentTheme(const uint8_t index);

    private:
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> _ElementThemes;
        winrt::Windows::UI::Xaml::ElementTheme _CurrentTheme;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(GlobalAppearance);
}

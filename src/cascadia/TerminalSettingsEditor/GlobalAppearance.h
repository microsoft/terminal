// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "GlobalAppearance.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct GlobalAppearance : GlobalAppearanceT<GlobalAppearance>
    {
        GlobalAppearance();
        winrt::Microsoft::Terminal::Settings::Model::GlobalAppSettings GlobalSettings();
        winrt::Windows::Foundation::Collections::IMap<winrt::hstring, Windows::UI::Xaml::ElementTheme> ElementThemes();

    private:
        winrt::Microsoft::Terminal::Settings::Model::GlobalAppSettings _GlobalAppSettings{ nullptr };
        winrt::Windows::Foundation::Collections::IMap<winrt::hstring, Windows::UI::Xaml::ElementTheme> _ElementThemeMap{ nullptr };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(GlobalAppearance);
}

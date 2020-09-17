// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Home.g.h"
#include "SettingsEditorViewModel.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Home : HomeT<Home>
    {
        Home();

        Editor::SettingsEditorViewModel HomeViewModel();

        void HomeGridItemClickHandler(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::Controls::ItemClickEventArgs const& args);

    private:
        Editor::SettingsEditorViewModel m_homeViewModel{ nullptr };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Home);
}

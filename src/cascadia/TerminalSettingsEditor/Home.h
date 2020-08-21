// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Home.g.h"
#include "SettingsControlViewModel.h"
#include "Utils.h"

namespace winrt::SettingsControl::implementation
{
    struct Home : HomeT<Home>
    {
        Home();

        SettingsControl::SettingsControlViewModel HomeViewModel();

        void HomeGridItemClickHandler(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::Controls::ItemClickEventArgs const& args);

        private:
            SettingsControl::SettingsControlViewModel m_homeViewModel{ nullptr };
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    BASIC_FACTORY(Home);
}

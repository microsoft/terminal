// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "PaletteItem.h"
#include "TabPaletteItem.g.h"
#include "inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct TabPaletteItem : TabPaletteItemT<TabPaletteItem, PaletteItem>
    {
        TabPaletteItem() = default;
        TabPaletteItem(winrt::TerminalApp::TabBase const& tab);

        GETSET_PROPERTY(winrt::TerminalApp::TabBase, Tab, nullptr);

    private:
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _tabChangedRevoker;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TabPaletteItem);
}

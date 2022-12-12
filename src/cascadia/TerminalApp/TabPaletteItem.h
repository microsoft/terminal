// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "PaletteItem.h"
#include "TabPaletteItem.g.h"

namespace winrt::TerminalApp::implementation
{
    struct TabPaletteItem : TabPaletteItemT<TabPaletteItem, PaletteItem>
    {
        TabPaletteItem() = default;
        TabPaletteItem(const winrt::TerminalApp::TabBase& tab);

        winrt::TerminalApp::TabBase Tab() const noexcept
        {
            return _tab.get();
        }

        WINRT_OBSERVABLE_PROPERTY(winrt::TerminalApp::TerminalTabStatus, TabStatus, _PropertyChangedHandlers);

    private:
        winrt::weak_ref<winrt::TerminalApp::TabBase> _tab;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _tabChangedRevoker;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _tabStatusChangedRevoker;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TabPaletteItem);
}

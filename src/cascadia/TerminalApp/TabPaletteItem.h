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
        TabPaletteItem(const MTApp::TabBase& tab);

        MTApp::TabBase Tab() const noexcept
        {
            return _tab.get();
        }

        WINRT_OBSERVABLE_PROPERTY(MTApp::TerminalTabStatus, TabStatus, _PropertyChangedHandlers);

    private:
        winrt::weak_ref<MTApp::TabBase> _tab;
        WUX::Data::INotifyPropertyChanged::PropertyChanged_revoker _tabChangedRevoker;
        WUX::Data::INotifyPropertyChanged::PropertyChanged_revoker _tabStatusChangedRevoker;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TabPaletteItem);
}

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

        winrt::TerminalApp::TabBase Tab() const noexcept
        {
            return _tab.get();
        }

        OBSERVABLE_GETSET_PROPERTY(bool, IsProgressRingActive, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, IsProgressRingIndeterminate, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(uint32_t, ProgressValue, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, IsPaneZoomed, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, BellIndicator, _PropertyChangedHandlers);

    private:
        winrt::weak_ref<winrt::TerminalApp::TabBase> _tab;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _tabChangedRevoker;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _tabHeaderChangedRevoker;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TabPaletteItem);
}

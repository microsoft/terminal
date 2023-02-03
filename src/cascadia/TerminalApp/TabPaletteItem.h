// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "PaletteItem.h"
#include "TabPaletteItem.g.h"

namespace winrt::Microsoft::Terminal::App::implementation
{
    struct TabPaletteItem : TabPaletteItemT<TabPaletteItem, PaletteItem>
    {
        TabPaletteItem() = default;
        TabPaletteItem(const winrt::Microsoft::Terminal::App::TabBase& tab);

        winrt::Microsoft::Terminal::App::TabBase Tab() const noexcept
        {
            return _tab.get();
        }

        WINRT_OBSERVABLE_PROPERTY(winrt::Microsoft::Terminal::App::TerminalTabStatus, TabStatus, _PropertyChangedHandlers);

    private:
        winrt::weak_ref<winrt::Microsoft::Terminal::App::TabBase> _tab;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _tabChangedRevoker;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _tabStatusChangedRevoker;
    };
}

namespace winrt::Microsoft::Terminal::App::factory_implementation
{
    BASIC_FACTORY(TabPaletteItem);
}

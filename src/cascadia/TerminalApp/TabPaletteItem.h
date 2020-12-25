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

    private:
        winrt::weak_ref<winrt::TerminalApp::TabBase> _tab;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TabPaletteItem);
}

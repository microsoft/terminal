// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "PaletteItem.g.h"

namespace winrt::TerminalApp::implementation
{
    struct PaletteItem : PaletteItemT<PaletteItem>
    {
    public:
        Windows::UI::Xaml::Controls::IconElement ResolvedIcon();

        til::property_changed_event PropertyChanged;

        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Name, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Icon, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, KeyChordText, PropertyChanged.raise);
    };
}

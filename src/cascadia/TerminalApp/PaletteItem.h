// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "PaletteItem.g.h"

namespace winrt::TerminalApp::implementation
{
    struct PaletteItem : PaletteItemT<PaletteItem>
    {
    public:
        WUXC::IconElement ResolvedIcon();

        WINRT_CALLBACK(PropertyChanged, WUX::Data::PropertyChangedEventHandler);

        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Name, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Icon, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, KeyChordText, _PropertyChangedHandlers);
    };
}

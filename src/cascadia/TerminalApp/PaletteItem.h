// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "inc/cppwinrt_utils.h"
#include "PaletteItem.g.h"

namespace winrt::TerminalApp::implementation
{
    struct PaletteItem : PaletteItemT<PaletteItem>
    {
    public:
        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, Name, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, Icon, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, KeyChordText, _PropertyChangedHandlers);
    };
}

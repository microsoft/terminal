// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TerminalBackground.g.h"

namespace winrt::TerminalApp::implementation
{
    struct TerminalBackground : TerminalBackgroundT<TerminalBackground>
    {
        TerminalBackground() = default;

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        // WINRT_OBSERVABLE_PROPERTY(Windows::UI::Xaml::Media::Brush, Brush, _PropertyChangedHandlers, winrt::Windows::UI::Xaml::Media::SolidColorBrush{ til::color{ 128, 128, 0 } });
        WINRT_OBSERVABLE_PROPERTY(Windows::UI::Xaml::Media::SolidColorBrush, Brush, _PropertyChangedHandlers, winrt::Windows::UI::Xaml::Media::SolidColorBrush{ til::color{ 128, 128, 0 } });
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TerminalBackground);
}

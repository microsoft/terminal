// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TerminalTabStatus.g.h"

namespace winrt::TerminalApp::implementation
{
    struct TerminalTabStatus : TerminalTabStatusT<TerminalTabStatus>
    {
        TerminalTabStatus() = default;

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(bool, IsPaneZoomed, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(bool, IsProgressRingActive, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(bool, IsProgressRingIndeterminate, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(bool, BellIndicator, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(bool, IsReadOnlyActive, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(uint32_t, ProgressValue, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(bool, IsInputBroadcastActive, _PropertyChangedHandlers);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TerminalTabStatus);
}

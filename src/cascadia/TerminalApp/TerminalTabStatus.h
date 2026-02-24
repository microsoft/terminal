// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TerminalTabStatus.g.h"

namespace winrt::TerminalApp::implementation
{
    struct TerminalTabStatus : TerminalTabStatusT<TerminalTabStatus>
    {
        TerminalTabStatus() = default;

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(bool, IsConnectionClosed, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(bool, IsPaneZoomed, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(bool, IsProgressRingActive, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(bool, IsProgressRingIndeterminate, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(bool, BellIndicator, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(bool, IsReadOnlyActive, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(uint32_t, ProgressValue, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(bool, IsInputBroadcastActive, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(winrt::Windows::UI::Color, TabColorIndicator, PropertyChanged.raise);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TerminalTabStatus);
}

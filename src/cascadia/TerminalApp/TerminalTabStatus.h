// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "inc/cppwinrt_utils.h"
#include "TerminalTabStatus.g.h"

namespace winrt::TerminalApp::implementation
{
    struct TerminalTabStatus : TerminalTabStatusT<TerminalTabStatus>
    {
        TerminalTabStatus() = default;

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(bool, IsPaneZoomed, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, IsProgressRingActive, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, IsProgressRingIndeterminate, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, BellIndicator, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, IsReadOnlyActive, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(uint32_t, ProgressValue, _PropertyChangedHandlers);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TerminalTabStatus);
}

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "TabRowControl.g.h"

namespace winrt::TerminalApp::implementation
{
    struct TabRowControl : TabRowControlT<TabRowControl>
    {
        TabRowControl();

        void OnNewTabButtonClick(const WF::IInspectable& sender, const MUXC::SplitButtonClickEventArgs& args);
        void OnNewTabButtonDrop(const WF::IInspectable& sender, const WUX::DragEventArgs& e);
        void OnNewTabButtonDragOver(const WF::IInspectable& sender, const WUX::DragEventArgs& e);

        WINRT_CALLBACK(PropertyChanged, WUX::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(bool, ShowElevationShield, _PropertyChangedHandlers, false);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TabRowControl);
}

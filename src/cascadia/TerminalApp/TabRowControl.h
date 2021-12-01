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

        void OnNewTabButtonClick(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::Controls::SplitButtonClickEventArgs const& args);
        void OnNewTabButtonDrop(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::DragEventArgs const& e);
        void OnNewTabButtonDragOver(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::DragEventArgs const& e);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(bool, ShowElevationShield, _PropertyChangedHandlers, false);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TabRowControl);
}

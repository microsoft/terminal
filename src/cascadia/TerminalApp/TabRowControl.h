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

        void OnNewTabButtonClick(const Windows::Foundation::IInspectable& sender, const Microsoft::UI::Xaml::Controls::SplitButtonClickEventArgs& args);
        void OnNewTabButtonDrop(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::DragEventArgs& e);
        void OnNewTabButtonDragOver(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::DragEventArgs& e);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(bool, ShowElevationShield, _PropertyChangedHandlers, false);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TabRowControl);
}

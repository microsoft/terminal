// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"
#include "inc/cppwinrt_utils.h"

#include "TabHeaderControl.g.h"

namespace winrt::TerminalApp::implementation
{
    struct TabHeaderControl : TabHeaderControlT<TabHeaderControl>
    {
        TabHeaderControl();
        winrt::hstring CurrentHeaderText();
        void UpdateHeaderText(winrt::hstring title);
        void SetZoomIcon(Windows::UI::Xaml::Visibility state);
        void ConstructTabRenameBox();

        void RenameBoxLostFocusHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                       winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        WINRT_CALLBACK(HeaderTitleChanged, TerminalApp::HeaderTitleChangedArgs);

    private:
        void _CloseRenameBox();
        bool _receivedKeyDown{ false };
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct TabHeaderControl : TabHeaderControlT<TabHeaderControl, implementation::TabHeaderControl>
    {
    };
}

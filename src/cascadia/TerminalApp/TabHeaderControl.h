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
        void SetZoomIcon(Windows::UI::Xaml::Visibility state);
        void ConstructTabRenameBox();

        void RenameBoxLostFocusHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                       winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        WINRT_CALLBACK(HeaderTitleWantsToChange, TerminalApp::HeaderTitleWantsToChangeArgs);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, Title, _PropertyChangedHandlers);

    private:
        bool _receivedKeyDown{ false };

        void _CloseRenameBox();
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct TabHeaderControl : TabHeaderControlT<TabHeaderControl, implementation::TabHeaderControl>
    {
    };
}

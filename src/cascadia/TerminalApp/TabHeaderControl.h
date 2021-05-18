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
        void BeginRename();

        void RenameBoxLostFocusHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                       winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        bool InRename();

        WINRT_CALLBACK(TitleChangeRequested, TerminalApp::TitleChangeRequestedArgs);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Title, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(double, RenamerMaxWidth, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::TerminalApp::TerminalTabStatus, TabStatus, _PropertyChangedHandlers);

        TYPED_EVENT(RenameEnded, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);

    private:
        bool _receivedKeyDown{ false };
        bool _renameCancelled{ false };

        void _CloseRenameBox();
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TabHeaderControl);
}

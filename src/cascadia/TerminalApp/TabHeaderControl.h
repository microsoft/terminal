// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "TabHeaderControl.g.h"

namespace winrt::TerminalApp::implementation
{
    struct TabHeaderControl : TabHeaderControlT<TabHeaderControl>
    {
        TabHeaderControl();
        void BeginRename();

        void RenameBoxLostFocusHandler(const WF::IInspectable& sender,
                                       const WUX::RoutedEventArgs& e);

        bool InRename();

        WINRT_CALLBACK(TitleChangeRequested, TerminalApp::TitleChangeRequestedArgs);

        WINRT_CALLBACK(PropertyChanged, WUX::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Title, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(double, RenamerMaxWidth, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(MTApp::TerminalTabStatus, TabStatus, _PropertyChangedHandlers);

        TYPED_EVENT(RenameEnded, WF::IInspectable, WF::IInspectable);

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

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace winrt::TerminalApp::implementation
{
    struct BasicPaneEvents
    {
        til::typed_event<> ConnectionStateChanged;
        til::typed_event<IPaneContent> CloseRequested;
        til::typed_event<IPaneContent, winrt::TerminalApp::BellEventArgs> BellRequested;
        til::typed_event<IPaneContent> TitleChanged;
        til::typed_event<IPaneContent> TabColorChanged;
        til::typed_event<IPaneContent> TaskbarProgressChanged;
        til::typed_event<IPaneContent> ReadOnlyChanged;
        til::typed_event<IPaneContent> FocusRequested;

        til::typed_event<winrt::Windows::Foundation::IInspectable, Microsoft::Terminal::Settings::Model::Command> DispatchCommandRequested;
    };
}
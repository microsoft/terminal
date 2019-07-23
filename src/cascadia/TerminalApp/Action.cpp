// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Action.h"
#include "KeyChordSerialization.h"

#include "Action.g.cpp"

using namespace winrt::Microsoft::Terminal;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::Data::Json;

namespace winrt::TerminalApp::implementation
{
    DEFINE_GETSET_PROPERTY(Action, winrt::hstring, Name);
    DEFINE_GETSET_PROPERTY(Action, winrt::hstring, IconPath);
    DEFINE_GETSET_PROPERTY(Action, winrt::TerminalApp::ShortcutAction, Command);
}

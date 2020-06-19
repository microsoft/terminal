// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "ActionArgs.h"

#include "ActionEventArgs.g.cpp"
#include "NewTerminalArgs.g.cpp"
#include "CopyTextArgs.g.cpp"
#include "NewTabArgs.g.cpp"
#include "SwitchToTabArgs.g.cpp"
#include "ResizePaneArgs.g.cpp"
#include "MoveFocusArgs.g.cpp"
#include "AdjustFontSizeArgs.g.cpp"
#include "SplitPaneArgs.g.cpp"
#include "OpenSettingsArgs.g.cpp"

#include <LibraryResources.h>

namespace winrt::TerminalApp::implementation
{
    winrt::hstring NewTerminalArgs::GenerateName()
    {
        return L"NewTerminalArgs";
    }
    winrt::hstring CopyTextArgs::GenerateName()
    {
        if (_SingleLine)
        {
            return L"Copy text as single line";
        }
        return L"Copy Text";
    }
    winrt::hstring NewTabArgs::GenerateName()
    {
        return L"NewTabArgs";
    }
    winrt::hstring SwitchToTabArgs::GenerateName()
    {
        return L"SwitchToTabArgs";
    }
    winrt::hstring ResizePaneArgs::GenerateName()
    {
        return L"ResizePaneArgs";
    }
    winrt::hstring MoveFocusArgs::GenerateName()
    {
        return L"MoveFocusArgs";
    }
    winrt::hstring AdjustFontSizeArgs::GenerateName()
    {
        return L"AdjustFontSizeArgs";
    }
    winrt::hstring SplitPaneArgs::GenerateName()
    {
        return L"SplitPaneArgs";
    }
    winrt::hstring OpenSettingsArgs::GenerateName()
    {
        return L"OpenSettingsArgs";
    }
}

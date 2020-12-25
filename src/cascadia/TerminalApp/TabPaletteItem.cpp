// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TabPaletteItem.h"
#include <LibraryResources.h>

#include "TabPaletteItem.g.cpp"

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    TabPaletteItem::TabPaletteItem(winrt::TerminalApp::TabBase const& tab) :
        _tab(tab)
    {
        Name(tab.Title());
        Icon(tab.Icon());
    }
}

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "CommandLinePaletteItem.h"
#include <LibraryResources.h>

#include "CommandLinePaletteItem.g.cpp"

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
    CommandLinePaletteItem::CommandLinePaletteItem(const winrt::hstring& commandLine) :
        _CommandLine(commandLine)
    {
        Name(commandLine);
    }
}

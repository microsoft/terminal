// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "CommandLinePaletteItem.h"
#include <LibraryResources.h>

#include "CommandLinePaletteItem.g.cpp"

using namespace winrt;
using namespace MTApp;
using namespace WUC;
using namespace WUX;
using namespace WS;
using namespace WF;
using namespace WFC;
using namespace MTSM;

namespace winrt::TerminalApp::implementation
{
    CommandLinePaletteItem::CommandLinePaletteItem(const winrt::hstring& commandLine) :
        _CommandLine(commandLine)
    {
        Name(commandLine);
    }
}

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include "PaletteItem.h"
#include "PaletteItem.g.cpp"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::System;

namespace winrt::TerminalApp::implementation
{
    Controls::IconElement PaletteItem::ResolvedIcon()
    {
        const auto icon = Microsoft::Terminal::UI::IconPathConverter::IconWUX(Icon());
        icon.Width(16);
        icon.Height(16);
        return icon;
    }
}

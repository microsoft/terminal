// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include "PaletteItem.h"
#include "PaletteItem.g.cpp"

using namespace winrt;
using namespace WUX;
using namespace WUC;
using namespace MTControl;
using namespace MTSM;
using namespace WS;

namespace winrt::TerminalApp::implementation
{
    Controls::IconElement PaletteItem::ResolvedIcon()
    {
        const auto icon = IconPathConverter::IconWUX(Icon());
        icon.Width(16);
        icon.Height(16);
        return icon;
    }
}

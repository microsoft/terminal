#pragma once
#include "ControlContextMenu.g.h"

namespace winrt::TerminalApp::implementation
{
    struct ControlContextMenu : ControlContextMenuT<ControlContextMenu>
    {
        ControlContextMenu();
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(ControlContextMenu);
}

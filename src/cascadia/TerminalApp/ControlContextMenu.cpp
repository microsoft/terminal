#include "pch.h"
#include "ControlContextMenu.h"
#include "ControlContextMenu.g.cpp"
#include <LibraryResources.h>

namespace winrt::TerminalApp::implementation
{
    // Method Description:
    // - Default constructor, localizes the buttons and hooks
    // up the event fired by the custom color picker, so that
    // the tab color is set on the fly when selecting a non-preset color
    // Arguments:
    // - <none>
    ControlContextMenu::ControlContextMenu()
    {
        InitializeComponent();
    }
}

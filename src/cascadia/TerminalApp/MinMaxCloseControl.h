//
// Declaration of the MainUserControl class.
//

#pragma once

#include "winrt/Windows.UI.Xaml.h"
#include "winrt/Windows.UI.Xaml.Markup.h"
#include "winrt/Windows.UI.Xaml.Interop.h"
#include "MinMaxCloseControl.g.h"

namespace winrt::TerminalApp::implementation
{
    struct MinMaxCloseControl : MinMaxCloseControlT<MinMaxCloseControl>
    {
        MinMaxCloseControl();
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct MinMaxCloseControl : MinMaxCloseControlT<MinMaxCloseControl, implementation::MinMaxCloseControl>
    {
    };
}

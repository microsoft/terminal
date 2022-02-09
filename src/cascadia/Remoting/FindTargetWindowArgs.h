/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- FindTargetWindowArgs.h

Abstract:
- This is a helper class for determining which window a specific commandline is
  intended for. The Monarch will create one of these, then toss it over to
  TerminalApp. TerminalApp actually contains the logic for parsing a
  commandline, as well as settings like the windowing behavior. Once the
  TerminalApp determines the correct window, it'll fill in the
  ResultTargetWindow property. The monarch will then read that value out to
  invoke the commandline in the appropriate window.

--*/

#pragma once

#include "FindTargetWindowArgs.g.h"

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct FindTargetWindowArgs : public FindTargetWindowArgsT<FindTargetWindowArgs>
    {
        WINRT_PROPERTY(winrt::Microsoft::Terminal::Remoting::CommandlineArgs, Args, nullptr);
        WINRT_PROPERTY(int, ResultTargetWindow, -1);
        WINRT_PROPERTY(winrt::hstring, ResultTargetWindowName);

    public:
        FindTargetWindowArgs(winrt::Microsoft::Terminal::Remoting::CommandlineArgs args) :
            _Args{ args } {};
    };
}

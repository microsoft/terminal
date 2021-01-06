// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "FindTargetWindowArgs.g.h"
#include "../cascadia/inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct FindTargetWindowArgs : public FindTargetWindowArgsT<FindTargetWindowArgs>
    {
    public:
        GETSET_PROPERTY(winrt::Microsoft::Terminal::Remoting::CommandlineArgs, Args, nullptr);
        GETSET_PROPERTY(int, ResultTargetWindow, -1);
    };
}

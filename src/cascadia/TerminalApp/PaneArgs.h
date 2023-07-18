// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "BellEventArgs.g.h"

namespace winrt::TerminalApp::implementation
{
    struct BellEventArgs : public BellEventArgsT<BellEventArgs>
    {
    public:
        BellEventArgs(bool flashTaskbar) :
            FlashTaskbar(flashTaskbar) {}

        til::property<bool> FlashTaskbar;
    };
};

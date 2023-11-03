// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace Microsoft::Console
{
    struct PassthroughState
    {
        explicit PassthroughState(wil::unique_hfile output) :
            _output{ std::move(output) }
        {
        }

    private:
        wil::unique_hfile _output;
    };
}

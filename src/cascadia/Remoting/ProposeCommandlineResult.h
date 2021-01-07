// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ProposeCommandlineResult.g.h"
#include "../cascadia/inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct ProposeCommandlineResult : public ProposeCommandlineResultT<ProposeCommandlineResult>
    {
    public:
        GETSET_PROPERTY(Windows::Foundation::IReference<uint64_t>, Id);
        GETSET_PROPERTY(bool, ShouldCreateWindow, true);
    };
}

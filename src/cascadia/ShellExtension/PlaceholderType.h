/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- PlaceholderType.h

Abstract:
Author(s):
- Mike Griese - May 2020

--*/
#pragma once

#include <conattrs.hpp>
#include "PlaceholderType.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::ShellExtension::implementation
{
    struct PlaceholderType : PlaceholderTypeT<PlaceholderType>
    {
        PlaceholderType() = default;
        GETSET_PROPERTY(int32_t, Placeholder, 42);
    };
}

namespace winrt::Microsoft::Terminal::ShellExtension::factory_implementation
{
    BASIC_FACTORY(PlaceholderType);
}

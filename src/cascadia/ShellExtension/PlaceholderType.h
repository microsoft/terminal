/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- PlaceholderType.h

Abstract:
- This class is just here to make our .wapproj play nicely with this project. If
  we don't define any winrt types, then we won't generate a .winmd, and the
  .wapproj will become _very_ mad at this project. So we'll use this placeholder
  class just to trick cppwinrt into generating a winmd for us. If we ever _do_
  add a real winrt type to this project, this can be removed.

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

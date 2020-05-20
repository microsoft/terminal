/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Foo.h

Abstract:
Author(s):
- Mike Griese - March 2019

--*/
#pragma once

#include <conattrs.hpp>
#include "Foo.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::ShellExtension::implementation
{
    struct Foo : FooT<Foo>
    {
        Foo() = default;
        GETSET_PROPERTY(int32_t, Bar, 42);
    };
}

namespace winrt::Microsoft::Terminal::ShellExtension::factory_implementation
{
    BASIC_FACTORY(Foo);
}

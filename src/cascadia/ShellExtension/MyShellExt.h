/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Foo.h

Abstract:
Author(s):
- Mike Griese - May 2020

--*/
#pragma once

#include <conattrs.hpp>
#include "../../cascadia/inc/cppwinrt_utils.h"

struct __declspec(uuid("8eb80de0-e1ff-442c-956a-c5f2b54ca274")) IMyShellExt : ::IUnknown
{
    virtual HRESULT __stdcall Call() = 0;
};

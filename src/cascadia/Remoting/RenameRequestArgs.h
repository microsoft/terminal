/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- RenameRequestArgs.h

--*/
#pragma once

#include "RenameRequestArgs.g.h"
#include "../cascadia/inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct RenameRequestArgs : public RenameRequestArgsT<RenameRequestArgs>
    {
        WINRT_PROPERTY(winrt::hstring, NewName);
        WINRT_PROPERTY(bool, Succeeded, false);

    public:
        RenameRequestArgs(winrt::hstring newName) :
            _NewName{ newName } {};
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(RenameRequestArgs);
}

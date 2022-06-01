/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- GetWindowLayoutArgs.h

Abstract:
- This is a helper class for getting the window layout from a peasant.
  Depending on if we are running on the monarch or on a peasant we might need
  to switch what thread we are executing on. This gives us the option of
  either returning the json result synchronously, or as a promise.
--*/

#pragma once

#include "GetWindowLayoutArgs.g.h"

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct GetWindowLayoutArgs : public GetWindowLayoutArgsT<GetWindowLayoutArgs>
    {
        WINRT_PROPERTY(winrt::hstring, WindowLayoutJson, L"");
        WINRT_PROPERTY(winrt::Windows::Foundation::IAsyncOperation<winrt::hstring>, WindowLayoutJsonAsync, nullptr)
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(GetWindowLayoutArgs);
}

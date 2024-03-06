/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- QuitAllRequestedArgs.h

Abstract:
- This is a helper class for allowing the monarch to run code before telling all
  peasants to quit. This way the monarch can raise an event and get back a future
  to wait for before continuing.
--*/

#pragma once

#include "QuitAllRequestedArgs.g.h"

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct QuitAllRequestedArgs : public QuitAllRequestedArgsT<QuitAllRequestedArgs>
    {
        WINRT_PROPERTY(winrt::Windows::Foundation::IAsyncAction, BeforeQuitAllAction, nullptr)
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(QuitAllRequestedArgs);
}

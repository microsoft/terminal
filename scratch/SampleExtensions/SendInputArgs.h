#pragma once
#include "SendInputArgs.g.h"

namespace winrt::SampleExtensions::implementation
{
    struct SendInputArgs : SendInputArgsT<SendInputArgs>
    {
        SendInputArgs() = default;
        SendInputArgs(winrt::hstring input) :
            _Input{ input } {};

        winrt::hstring Input() { return _Input; }

    private:
        winrt::hstring _Input;
    };
}

namespace winrt::SampleExtensions::factory_implementation
{
    struct SendInputArgs : SendInputArgsT<SendInputArgs, implementation::SendInputArgs>
    {
    };
}

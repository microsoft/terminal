// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "ThrottledFunc.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;

ThrottledFunc<>::ThrottledFunc(ThrottledFunc::Func func, TimeSpan delay, CoreDispatcher dispatcher) :
    _func{ func },
    _delay{ delay },
    _dispatcher{ dispatcher },
    _isRunPending{}
{
}

// Method Description:
// - Runs the function later, except if `Run` is called again before
//   with a new argument, in which case the request will be ignored.
// - For more information, read the class' documentation.
// - This method is always thread-safe. It can be called multiple times on
//   different threads.
// Arguments:
// - <none>
// Return Value:
// - <none>
void ThrottledFunc<>::Run()
{
    if (_isRunPending.test_and_set(std::memory_order_acquire))
    {
        // already pending
        return;
    }

    _dispatcher.RunAsync(CoreDispatcherPriority::Low, [weakThis = this->weak_from_this()]() {
        if (auto self{ weakThis.lock() })
        {
            DispatcherTimer timer;
            timer.Interval(self->_delay);
            timer.Tick([=](auto&&...) {
                if (auto self{ weakThis.lock() })
                {
                    timer.Stop();
                    self->_isRunPending.clear(std::memory_order_release);
                    self->_func();
                }
            });
            timer.Start();
        }
    });
}

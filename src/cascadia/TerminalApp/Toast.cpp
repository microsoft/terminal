// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "Toast.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;

constexpr const auto ToastDuration = std::chrono::milliseconds(3000);

Toast::Toast(const winrt::Microsoft::UI::Xaml::Controls::TeachingTip& tip) :
    _tip{ tip }
{
    _timer.Interval(ToastDuration);
}

// Method Description:
// - Open() the TeachingTip, and start our timer. When the timer expires, the
//   tip will be closed.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Toast::Open()
{
    _tip.IsOpen(true);

    std::weak_ptr<Toast> weakThis{ shared_from_this() };
    _timer.Tick([weakThis](auto&&...) {
        if (auto self{ weakThis.lock() })
        {
            self->_timer.Stop();
            self->_tip.IsOpen(false);
        }
    });
    _timer.Start();
}

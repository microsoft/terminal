// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "Toast.h"

using namespace WF;
using namespace WUC;
using namespace WUX;

constexpr const auto ToastDuration = std::chrono::milliseconds(3000);

Toast::Toast(const MUXC::TeachingTip& tip) :
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

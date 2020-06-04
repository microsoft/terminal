// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "ScrollBarUpdater.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml::Controls::Primitives;

// Limit the rate of scroll update operation
// See also: Microsoft::Console::Render::RenderThread::s_FrameLimitMilliseconds
constexpr const long long ScrollRateLimitMilliseconds = 8;

// Method Description:
// - Apply the update to a scroll bar.
// Return Value:
// - <none>
void ScrollBarUpdate::Apply(ScrollBar scrollBar) const
{
    scrollBar.Maximum(NewMaximum);
    scrollBar.Minimum(NewMinimum);
    scrollBar.ViewportSize(NewViewportSize);

    if (NewValue.has_value())
    {
        scrollBar.Value(NewValue.value());
    }
}

ScrollBarUpdater::ScrollBarUpdater() :
    _throttle(std::chrono::milliseconds(ScrollRateLimitMilliseconds)),
    _stillExists(std::make_shared<bool>(true))
{
}

// Method Description:
// - Update the scroll bar's settings later, asynchronously.
// - For more information, read the Abstract section in the header file.
// Arguments:
// - scrollBar: the scroll bar control to update
// - newUpdate: the update with the new scroll bar's settings
// Return Value:
// - <none>
void ScrollBarUpdater::DoUpdate(ScrollBar scrollBar, ScrollBarUpdate newUpdate)
{
    {
        std::lock_guard updateGuard(_pendingUpdateMutex);

        bool hasUpdate = _pendingUpdate.has_value();
        _pendingUpdate = { newUpdate };
        if (hasUpdate)
        {
            // `_ApplyPendingUpdateLater` is already running. It will apply
            // the new update.
            return;
        }
    }

    _ApplyPendingUpdateLater(scrollBar);
}

// Method Description:
// - Call this from the scroll bar's value change event handler.
// - It returns whether that event is coming from us or from an external
//   source such as the user.
// - Should only be called from the UI thread!
// Arguments:
// - <none>
// Return Value:
// - `true` if the current event is from us. `false` otherwise.
bool ScrollBarUpdater::IsInternalUpdate() const
{
    return _isInternalUpdate;
}

// Method Description:
// - If there is a pending update, tell it to not update the scroll bar's
//   value (but still update the minimum, maximum and viewport size).
// Arguments:
// - <none>
// Return Value:
// - <none>
void ScrollBarUpdater::CancelPendingValueChange()
{
    std::lock_guard updateGuard(_pendingUpdateMutex);

    if (_pendingUpdate.has_value())
    {
        _pendingUpdate.value().NewValue = { std::nullopt };
    }
}

// Method Description:
// - Starts the updater.
// - There should always be at most one instance of this coroutine running at a
//   time.
// - If this coroutine is already running and you want to change the values of
//   the scroll bar, modify the `_pendingUpdate` member and the method will pick
//   the new values when it is time to update the scroll bar.
// - To cancel the update, call `_CancelPendingUpdate` or `CancelPendingValueChange`.
// - The point of this coroutine is to introduce a small delay before updating the
//   scroll bar position to prevent spending too much CPU while the terminal is
//   scrolling a lot.
// Arguments:
// - scrollBar: the scroll bar control to update
// Return Value:
// - <none>
winrt::fire_and_forget ScrollBarUpdater::_ApplyPendingUpdateLater(winrt::weak_ref<ScrollBar> weakScrollBar)
{
    const auto weakThis = weak_from_this();

    // No need to lock for `_throttle` because even though it's not thread-
    // safe, there should always be at most one instance of
    // `_ApplyPendingUpdateLater` and `_ApplyPendingUpdate` running at a time.
    const auto waitTime = _throttle.GetNextWaitTime();
    if (waitTime.has_value())
    {
        co_await winrt::resume_after(waitTime.value());
    }

    {
        auto scrollBar = weakScrollBar.get();
        if (!scrollBar)
        {
            const auto that = weakThis.lock();
            if (that)
            {
                // update failed because scroll bar is dead
                that->_CancelPendingUpdate();
            }

            co_return;
        }

        co_await winrt::resume_foreground(scrollBar.Dispatcher());
    }

    const auto that = weakThis.lock();
    if (!that)
    {
        co_return;
    }

    const auto scrollBar = weakScrollBar.get();
    if (!scrollBar)
    {
        // update failed because scroll bar is dead
        that->_CancelPendingUpdate();

        co_return;
    }

    that->_ApplyPendingUpdate(scrollBar);
}

// Method Description:
// - If there is a pending update, apply the new settings to the scroll bar.
// - This must be called on the UI thread!
// Arguments:
// - scrollBar: the scroll bar control to update
// Return Value:
// - <none>
void ScrollBarUpdater::_ApplyPendingUpdate(ScrollBar scrollBar)
{
    std::lock_guard updateGuard(_pendingUpdateMutex);

    if (_pendingUpdate.has_value())
    {
        _isInternalUpdate = true;
        _pendingUpdate->Apply(scrollBar);
        _isInternalUpdate = false;

        _throttle.DidAction();

        _pendingUpdate.reset();
    }
}

void ScrollBarUpdater::_CancelPendingUpdate()
{
    std::lock_guard updateGuard(_pendingUpdateMutex);
    _pendingUpdate.reset();
}

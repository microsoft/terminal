/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ScrollBarUpdater.h

Abstract:
- Modify the scroll bar's settings (minimum, maximum, viewport size and value).
- Can be called from another thread than the UI thread. For that reason, the
  `DoUpdate` is asynchronous: it returns `winrt::fire_and_forget`.
- The updates are throttled because updating the scroll bar settings very often
  can use a significant amount of CPU time.
- You can cancel the modification of the scroll bar's value (but not minimum,
  maximum and viewport size) from a pending update.
  This should be called if  the user touches the scroll bar. Indeed, the user
  is king so their choice take priority over the terminal.
- From the scroll bar's value change event handler, determine if the change is
  coming from us or from an external source such as the user with the
  `IsInternalUpdate` method.
--*/

#pragma once
#include "pch.h"

#include "TimeThrottle.h"

struct ScrollBarUpdate
{
    void Apply(winrt::Windows::UI::Xaml::Controls::Primitives::ScrollBar scrollBar) const;

    std::optional<int> NewValue;
    int NewMinimum;
    int NewMaximum;
    int NewViewportSize;
};

class ScrollBarUpdater : public std::enable_shared_from_this<ScrollBarUpdater>
{
public:
    ScrollBarUpdater();

    void DoUpdate(winrt::Windows::UI::Xaml::Controls::Primitives::ScrollBar scrollBar, ScrollBarUpdate update);
    void CancelPendingValueChange();

    bool IsInternalUpdate() const;

private:
    winrt::fire_and_forget _ApplyPendingUpdateLater(winrt::weak_ref<winrt::Windows::UI::Xaml::Controls::Primitives::ScrollBar> weakScrollBar);
    void _ApplyPendingUpdate(winrt::Windows::UI::Xaml::Controls::Primitives::ScrollBar scrollBar);
    void _CancelPendingUpdate();

    TimeThrottle _throttle;

    std::optional<ScrollBarUpdate> _pendingUpdate;
    std::mutex _pendingUpdateMutex;

    bool _isInternalUpdate = false;

    std::shared_ptr<bool> _stillExists;
};

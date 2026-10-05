// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "SummonWindowBehavior.h"

#include <cassert>
#include <limits>

using Microsoft::Terminal::Windowing::MonitorBehavior;
using Microsoft::Terminal::Windowing::SummonWindowBehavior;
using Microsoft::Terminal::Windowing::SummonWindowBehaviorRef;

static_assert(static_cast<int32_t>(MonitorBehavior::InPlace) == 0);
static_assert(static_cast<int32_t>(MonitorBehavior::ToCurrent) == 1);
static_assert(static_cast<int32_t>(MonitorBehavior::ToMouse) == 2);

int main()
{
    winrt::init_apartment();
    {
        SummonWindowBehaviorRef absent{ nullptr };
        assert(!absent);

        auto policy = winrt::make_self<SummonWindowBehavior>();
        assert(policy->MoveToCurrentDesktop());
        assert(policy->ToggleVisibility());
        assert(policy->DropdownDuration() == 0);
        assert(policy->ToMonitor() == MonitorBehavior::ToCurrent);

        auto alias = policy;
        policy->MoveToCurrentDesktop(false);
        policy->ToggleVisibility(false);
        policy->DropdownDuration(std::numeric_limits<uint32_t>::max());
        policy->ToMonitor(MonitorBehavior::ToMouse);
        assert(!alias->MoveToCurrentDesktop());
        assert(!alias->ToggleVisibility());
        assert(alias->DropdownDuration() == std::numeric_limits<uint32_t>::max());
        assert(alias->ToMonitor() == MonitorBehavior::ToMouse);

        auto weak = policy->get_weak();
        assert(weak.get().get() == policy.get());
        policy = nullptr;
        assert(weak.get().get() == alias.get());
        alias = nullptr;
        assert(!weak.get());
    }
    winrt::uninit_apartment();
}

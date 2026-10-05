// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "../WindowsTerminal/SummonWindowBehavior.h"

#include <WexTestClass.h>
#include <limits>

using namespace WEX::TestExecution;
using Microsoft::Terminal::Windowing::MonitorBehavior;
using Microsoft::Terminal::Windowing::SummonWindowBehavior;
using Microsoft::Terminal::Windowing::SummonWindowBehaviorRef;

static_assert(static_cast<int32_t>(MonitorBehavior::InPlace) == 0);
static_assert(static_cast<int32_t>(MonitorBehavior::ToCurrent) == 1);
static_assert(static_cast<int32_t>(MonitorBehavior::ToMouse) == 2);

namespace NativeContractsUnitTests
{
    class SummonTests
    {
        TEST_CLASS(SummonTests);

        TEST_METHOD(DefaultsAndSharedMutation)
        {
            SummonWindowBehaviorRef absent{ nullptr };
            VERIFY_IS_TRUE(absent == nullptr);

            const auto policy = winrt::make_self<SummonWindowBehavior>();
            VERIFY_IS_TRUE(policy->MoveToCurrentDesktop());
            VERIFY_IS_TRUE(policy->ToggleVisibility());
            VERIFY_ARE_EQUAL(0u, policy->DropdownDuration());
            VERIFY_IS_TRUE(policy->ToMonitor() == MonitorBehavior::ToCurrent);

            const auto alias = policy;
            policy->MoveToCurrentDesktop(false);
            policy->ToggleVisibility(false);
            policy->DropdownDuration((std::numeric_limits<uint32_t>::max)());
            policy->ToMonitor(MonitorBehavior::ToMouse);
            VERIFY_IS_FALSE(alias->MoveToCurrentDesktop());
            VERIFY_IS_FALSE(alias->ToggleVisibility());
            VERIFY_ARE_EQUAL((std::numeric_limits<uint32_t>::max)(), alias->DropdownDuration());
            VERIFY_IS_TRUE(alias->ToMonitor() == MonitorBehavior::ToMouse);
        }

        TEST_METHOD(StrongAndWeakLifetime)
        {
            auto policy = winrt::make_self<SummonWindowBehavior>();
            auto alias = policy;
            const auto weak = policy->get_weak();
            VERIFY_IS_TRUE(weak.get().get() == policy.get());
            policy = nullptr;
            VERIFY_IS_TRUE(weak.get().get() == alias.get());
            alias = nullptr;
            VERIFY_IS_TRUE(weak.get() == nullptr);
        }
    };
}

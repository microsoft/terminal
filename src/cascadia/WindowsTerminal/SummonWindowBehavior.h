// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <cstdint>
#include <winrt/base.h>

namespace Microsoft::Terminal::Windowing
{
    enum class MonitorBehavior : int32_t
    {
        InPlace = 0,
        ToCurrent = 1,
        ToMouse = 2,
    };

    struct SummonWindowBehavior : winrt::implements<SummonWindowBehavior, winrt::Windows::Foundation::IInspectable>
    {
        bool MoveToCurrentDesktop() const noexcept { return _moveToCurrentDesktop; }
        void MoveToCurrentDesktop(bool value) noexcept { _moveToCurrentDesktop = value; }
        bool ToggleVisibility() const noexcept { return _toggleVisibility; }
        void ToggleVisibility(bool value) noexcept { _toggleVisibility = value; }
        uint32_t DropdownDuration() const noexcept { return _dropdownDuration; }
        void DropdownDuration(uint32_t value) noexcept { _dropdownDuration = value; }
        MonitorBehavior ToMonitor() const noexcept { return _toMonitor; }
        void ToMonitor(MonitorBehavior value) noexcept { _toMonitor = value; }

    private:
        bool _moveToCurrentDesktop{ true };
        bool _toggleVisibility{ true };
        uint32_t _dropdownDuration{ 0 };
        MonitorBehavior _toMonitor{ MonitorBehavior::ToCurrent };
    };

    using SummonWindowBehaviorRef = winrt::com_ptr<SummonWindowBehavior>;
}

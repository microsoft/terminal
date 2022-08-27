// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TaskbarState.h"
#include "TaskbarState.g.cpp"

namespace winrt::TerminalApp::implementation
{
    // Default to unset, 0%.
    TaskbarState::TaskbarState() :
        TaskbarState(0, 0){};

    TaskbarState::TaskbarState(const uint64_t dispatchTypesState, const uint64_t progressParam) :
        _State{ dispatchTypesState },
        _Progress{ progressParam } {}

    uint64_t TaskbarState::Priority() const
    {
        // This seemingly nonsensical ordering is from
        // https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itaskbarlist3-setprogressstate#how-the-taskbar-button-chooses-the-progress-indicator-for-a-group
        switch (_State)
        {
        case 0: // Clear = 0,
            return 5;
        case 1: // Set = 1,
            return 3;
        case 2: // Error = 2,
            return 1;
        case 3: // Indeterminate = 3,
            return 4;
        case 4: // Paused = 4
            return 2;
        }
        // Here, return 6, to definitely be greater than all the other valid values.
        // This should never really happen.
        return 6;
    }

    int TaskbarState::ComparePriority(const winrt::TerminalApp::TaskbarState& lhs, const winrt::TerminalApp::TaskbarState& rhs)
    {
        return lhs.Priority() < rhs.Priority();
    }

}

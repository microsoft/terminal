// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TaskbarState.h"
#include "TaskbarState.g.cpp"

namespace winrt::TerminalApp::implementation
{
    // Default to unset, 0%.
    TaskbarState::TaskbarState() :
        TaskbarState(winrt::Microsoft::Terminal::Control::TaskbarState::Clear, 0) {};

    TaskbarState::TaskbarState(const winrt::Microsoft::Terminal::Control::TaskbarState state, const uint64_t progressParam) :
        _State{ state },
        _Progress{ progressParam } {}

    uint64_t TaskbarState::Priority() const
    {
        // This seemingly nonsensical ordering is from
        // https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itaskbarlist3-setprogressstate#how-the-taskbar-button-chooses-the-progress-indicator-for-a-group
        using TbState = winrt::Microsoft::Terminal::Control::TaskbarState;
        switch (_State)
        {
        case TbState::Clear:
            return 5;
        case TbState::Set:
            return 3;
        case TbState::Error:
            return 1;
        case TbState::Indeterminate:
            return 4;
        case TbState::Paused:
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

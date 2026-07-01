// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "TaskbarState.g.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class TabTests;
};

namespace winrt::TerminalApp::implementation
{
    struct TaskbarState : TaskbarStateT<TaskbarState>
    {
    public:
        TaskbarState();
        TaskbarState(const winrt::Microsoft::Terminal::Control::TaskbarState state, const uint64_t progress);

        static int ComparePriority(const winrt::TerminalApp::TaskbarState& lhs, const winrt::TerminalApp::TaskbarState& rhs);

        uint64_t Priority() const;

        WINRT_PROPERTY(winrt::Microsoft::Terminal::Control::TaskbarState, State, winrt::Microsoft::Terminal::Control::TaskbarState::Clear);
        WINRT_PROPERTY(uint64_t, Progress, 0);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TaskbarState);
}

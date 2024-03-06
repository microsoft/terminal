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
        TaskbarState(const uint64_t dispatchTypesState, const uint64_t progress);

        static int ComparePriority(const winrt::TerminalApp::TaskbarState& lhs, const winrt::TerminalApp::TaskbarState& rhs);

        uint64_t Priority() const;

        WINRT_PROPERTY(uint64_t, State, 0);
        WINRT_PROPERTY(uint64_t, Progress, 0);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TaskbarState);
}

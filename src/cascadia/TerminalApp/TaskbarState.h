// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "TaskbarState.g.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class TabTests;
};

namespace winrt::Microsoft::Terminal::App::implementation
{
    struct TaskbarState : TaskbarStateT<TaskbarState>
    {
    public:
        TaskbarState();
        TaskbarState(const uint64_t dispatchTypesState, const uint64_t progress);

        static int ComparePriority(const winrt::Microsoft::Terminal::App::TaskbarState& lhs, const winrt::Microsoft::Terminal::App::TaskbarState& rhs);

        uint64_t Priority() const;

        WINRT_PROPERTY(uint64_t, State, 0);
        WINRT_PROPERTY(uint64_t, Progress, 0);
    };
}

namespace winrt::Microsoft::Terminal::App::factory_implementation
{
    BASIC_FACTORY(TaskbarState);
}

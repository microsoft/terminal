// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Native/CommandlineArgs.h"
#include "AppCommandlineArgs.h"

#include <mutex>
#include <winrt/Microsoft.Terminal.TerminalConnection.h>

namespace TerminalApp::Native
{
    struct CommandlineArgs::State
    {
        AppCommandlineArgs parsed;
        int32_t parseResult = 0;
        winrt::com_array<winrt::hstring> args;
        winrt::hstring currentDirectory;
        winrt::hstring currentEnvironment;
        uint32_t showWindowCommand = 1; // SW_NORMAL
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection connection{ nullptr };
        std::mutex adapterMutex;
        winrt::weak_ref<winrt::Windows::Foundation::IInspectable> adapter;
    };
}

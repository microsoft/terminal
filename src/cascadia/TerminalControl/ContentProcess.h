// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ContentProcess.g.h"
#include "ControlInteractivity.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct ContentProcess : ContentProcessT<ContentProcess>
    {
        ContentProcess(winrt::guid g);
        ~ContentProcess();
        static winrt::fire_and_forget final_release(std::unique_ptr<ContentProcess> ptr) noexcept;

        bool Initialize(Control::IControlSettings settings,
                        Control::IControlAppearance unfocusedAppearance,
                        TerminalConnection::ConnectionInformation connectionInfo);
        Control::ControlInteractivity GetInteractivity();

        uint64_t GetPID();
        uint64_t RequestSwapChainHandle(const uint64_t pid);
        winrt::guid Guid();

    private:
        Control::ControlInteractivity _interactivity{ nullptr };
        uint64_t _ourPID;
        winrt::guid _guid;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(ContentProcess);
}

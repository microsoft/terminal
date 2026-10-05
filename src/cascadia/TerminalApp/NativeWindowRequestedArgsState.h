// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Native/WindowRequestedArgs.h"

#include <mutex>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.Terminal.Settings.Model.h>

namespace TerminalApp::Native
{
    struct WindowRequestedArgs::State
    {
        uint64_t id = 0;
        winrt::hstring windowName;
        CommandlineArgsRef command;
        winrt::hstring content;
        std::optional<WindowBounds> bounds;
        // Retained only until the settings graph and controller entry points become native.
        winrt::Microsoft::Terminal::Settings::Model::WindowLayout persistedLayout{ nullptr };
        winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs> startupActions{ nullptr };
        // Compatibility identity anchors; command state and bounds values are native.
        winrt::Windows::Foundation::IInspectable commandAdapter{ nullptr };
        winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Rect> boundsAdapter{ nullptr };
        std::mutex adapterMutex;
        winrt::weak_ref<winrt::Windows::Foundation::IInspectable> adapter;
    };
}

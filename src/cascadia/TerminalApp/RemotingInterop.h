// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Native/CommandlineArgs.h"
#include "Native/WindowRequestedArgs.h"

#include <winrt/TerminalApp.h>
#include <winrt/Microsoft.Terminal.TerminalConnection.h>

// Transitional adapter for callers whose controller contracts still use WinRT.
namespace TerminalApp::Native
{
    struct CommandlineArgsInterop
    {
        APP_NATIVE_API static CommandlineArgsRef FromProjected(const winrt::TerminalApp::CommandlineArgs& value);
        APP_NATIVE_API static winrt::TerminalApp::CommandlineArgs ToProjected(const CommandlineArgsRef& value);
        APP_NATIVE_API static winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection Connection(const CommandlineArgsRef& value);
        APP_NATIVE_API static void Connection(const CommandlineArgsRef& value, const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& connection);
    };

    struct WindowRequestedArgsInterop
    {
        APP_NATIVE_API static WindowRequestedArgsRef FromProjected(const winrt::TerminalApp::WindowRequestedArgs& value);
        APP_NATIVE_API static winrt::TerminalApp::WindowRequestedArgs ToProjected(const WindowRequestedArgsRef& value);
        APP_NATIVE_API static WindowRequestedArgsRef FromCommand(uint64_t id, const winrt::TerminalApp::CommandlineArgs& command);
        APP_NATIVE_API static WindowRequestedArgsRef FromContent(
            const winrt::hstring& window, const winrt::hstring& content,
            const winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Rect>& bounds);
        APP_NATIVE_API static winrt::TerminalApp::CommandlineArgs Command(const WindowRequestedArgsRef& value);
        APP_NATIVE_API static winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Rect> InitialBounds(const WindowRequestedArgsRef& value);
        APP_NATIVE_API static winrt::Microsoft::Terminal::Settings::Model::WindowLayout PersistedLayout(const WindowRequestedArgsRef& value);
        APP_NATIVE_API static void PersistedLayout(const WindowRequestedArgsRef& value, const winrt::Microsoft::Terminal::Settings::Model::WindowLayout& layout);
        APP_NATIVE_API static winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs> StartupActions(const WindowRequestedArgsRef& value);
        APP_NATIVE_API static void StartupActions(const WindowRequestedArgsRef& value, const winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs>& actions);
        APP_NATIVE_API static uint32_t ApplyStartup(const WindowRequestedArgsRef& value, const winrt::TerminalApp::TerminalWindow& window);
    };
}

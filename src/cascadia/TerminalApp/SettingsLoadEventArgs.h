// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "SettingsLoadEventArgs.g.h"
#include <inc/cppwinrt_utils.h>
namespace winrt::TerminalApp::implementation
{
    struct SettingsLoadEventArgs : SettingsLoadEventArgsT<SettingsLoadEventArgs>
    {
        WINRT_PROPERTY(bool, Reload, false);
        WINRT_PROPERTY(uint64_t, Result, S_OK);
        WINRT_PROPERTY(winrt::hstring, ExceptionText, L"");
        WINRT_PROPERTY(winrt::Windows::Foundation::Collections::IVector<Microsoft::Terminal::Settings::Model::SettingsLoadWarnings>, Warnings, nullptr);
        WINRT_PROPERTY(Microsoft::Terminal::Settings::Model::CascadiaSettings, NewSettings, nullptr);

    public:
        SettingsLoadEventArgs(bool reload,
                              uint64_t result,
                              winrt::hstring exceptionText,
                              winrt::Windows::Foundation::Collections::IVector<Microsoft::Terminal::Settings::Model::SettingsLoadWarnings> warnings,
                              Microsoft::Terminal::Settings::Model::CascadiaSettings newSettings) :
            _Reload{ reload },
            _Result{ result },
            _ExceptionText{ std::move(exceptionText) },
            _Warnings{ std::move(warnings) },
            _NewSettings{ std::move(newSettings) } {};
    };
}

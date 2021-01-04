// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ApplicationState.h"
#include "ApplicationState.g.cpp"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    Microsoft::Terminal::Settings::Model::ApplicationState ApplicationState::GetForCurrentApp()
    {
        throw hresult_not_implemented();
    }
    void ApplicationState::Reset()
    {
        throw hresult_not_implemented();
    }
    bool ApplicationState::ShowConfirmCloseAllTabs()
    {
        throw hresult_not_implemented();
    }
    void ApplicationState::ShowConfirmCloseAllTabs(bool value)
    {
        throw hresult_not_implemented();
    }
    bool ApplicationState::ShowConfirmLargePaste()
    {
        throw hresult_not_implemented();
    }
    void ApplicationState::ShowConfirmLargePaste(bool value)
    {
        throw hresult_not_implemented();
    }
    bool ApplicationState::ShowConfirmMultiLinePaste()
    {
        throw hresult_not_implemented();
    }
    void ApplicationState::ShowConfirmMultiLinePaste(bool value)
    {
        throw hresult_not_implemented();
    }
}

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ContentProcess.h"
#include "ContentProcess.g.cpp"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    ContentProcess::ContentProcess() {}

    void ContentProcess::Initialize(Control::IControlSettings settings, TerminalConnection::ITerminalConnection connection)
    {
        _interactivity = winrt::make<implementation::ControlInteractivity>(settings, connection);
    }

    Control::ControlInteractivity ContentProcess::GetInteractivity()
    {
        return _interactivity;
    }

}

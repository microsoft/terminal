// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ParentPane.h"

#include "ParentPane.g.cpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    ParentPane::ParentPane()
    {
        InitializeComponent();
    }

    Controls::Grid ParentPane::GetRootElement()
    {
        return Root();
    }

    void ParentPane::UpdateSettings(const TerminalSettings& /*settings*/, const GUID& /*profile*/)
    {
    }

    void ParentPane::Relayout()
    {
    }

    void ParentPane::FocusPane(uint32_t /*id*/)
    {
    }

    void ParentPane::ResizeContent(const Size& /*newSize*/)
    {
    }

    bool ParentPane::ResizePane(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& direction)
    {
        return false;
    }

    bool ParentPane::NavigateFocus(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction)
    {
        return false;
    }
}

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

    bool ParentPane::ResizePane(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& /*direction*/)
    {
        return false;
    }

    bool ParentPane::NavigateFocus(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& /*direction*/)
    {
        return false;
    }

    float ParentPane::CalcSnappedDimension(const bool /*widthOrHeight*/, const float /*dimension*/) const
    {
        return {};
    }

    int ParentPane::GetLeafPaneCount() const noexcept
    {
        return {};
    }

    Size ParentPane::GetMinSize() const
    {
        return {};
    }

    void ParentPane::_CreateRowColDefinitions()
    {
    }

    void ParentPane::_UpdateBorders()
    {
    }

    bool ParentPane::_Resize(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& /*direction*/)
    {
        return false;
    }

    bool ParentPane::_NavigateFocus(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& /*direction*/)
    {
        return false;
    }

    void ParentPane::_CloseChild(const bool /*closeFirst*/)
    {
    }

    std::pair<float, float> ParentPane::_CalcChildrenSizes(const float /*fullSize*/) const
    {
        return {};
    }

    ParentPane::SnapChildrenSizeResult ParentPane::_CalcSnappedChildrenSizes(const bool /*widthOrHeight*/, const float /*fullSize*/) const
    {
        return {};
    }

    ParentPane::SnapSizeResult ParentPane::_CalcSnappedDimension(const bool /*widthOrHeight*/, const float /*dimension*/) const
    {
        return {};
    }

    void ParentPane::_AdvanceSnappedDimension(const bool /*widthOrHeight*/, LayoutSizeNode& /*sizeNode*/) const
    {
    }

    //ParentPane::LayoutSizeNode ParentPane::_CreateMinSizeTree(const bool widthOrHeight) const
    //{
    //    return {};
    //}

    float ParentPane::_ClampSplitPosition(const bool /*widthOrHeight*/, const float /*requestedValue*/, const float /*totalSize*/) const
    {
        return {};
    }
}

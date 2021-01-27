// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ParentPane.h"

#include "ParentPane.g.cpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;

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

    void ParentPane::UpdateSettings(const TerminalSettings& settings, const GUID& profile)
    {
        _firstChild.UpdateSettings(settings, profile);
        _secondChild.UpdateSettings(settings, profile);
    }

    //LeafPane* ParentPane::GetActivePane()
    //{
    //    auto first = _firstChild.GetActivePane();
    //    if (first != nullptr)
    //    {
    //        return &first;
    //    }
    //    auto second = _secondChild.GetActivePane();
    //    return &second;
    //}

    void ParentPane::Relayout()
    {
        ResizeContent(Root().ActualSize());
    }

    void ParentPane::FocusPane(uint32_t /*id*/)
    {
    }

    void ParentPane::ResizeContent(const Size& newSize)
    {
        const auto width = newSize.Width;
        const auto height = newSize.Height;

        _CreateRowColDefinitions();

        if (_splitState == SplitState::Vertical)
        {
            const auto paneSizes = _CalcChildrenSizes(width);

            const Size firstSize{ paneSizes.first, height };
            const Size secondSize{ paneSizes.second, height };
            _firstChild.ResizeContent(firstSize);
            _secondChild.ResizeContent(secondSize);
        }
        else if (_splitState == SplitState::Horizontal)
        {
            const auto paneSizes = _CalcChildrenSizes(height);

            const Size firstSize{ width, paneSizes.first };
            const Size secondSize{ width, paneSizes.second };
            _firstChild.ResizeContent(firstSize);
            _secondChild.ResizeContent(secondSize);
        }
    }

    bool ParentPane::ResizePane(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& direction)
    {
        // Check if either our first or second child is the currently focused leaf.
        // If it is, and the requested resize direction matches our separator, then
        // we're the pane that needs to adjust its separator.
        // If our separator is the wrong direction, then we can't handle it.
        const auto firstIsLeaf = _firstChild.try_as<LeafPane>();
        const auto secondIsLeaf = _secondChild.try_as<LeafPane>();
        const bool firstIsFocused = firstIsLeaf && firstIsLeaf.WasLastFocused();
        const bool secondIsFocused = secondIsLeaf && secondIsLeaf.WasLastFocused();
        if (firstIsFocused || secondIsFocused)
        {
            return _Resize(direction);
        }

        // If neither of our children were the focused leaf, then recurse into
        // our children and see if they can handle the resize.
        // For each child, if it has a focused descendant, try having that child
        // handle the resize.
        // If the child wasn't able to handle the resize, it's possible that
        // there were no descendants with a separator the correct direction. If
        // our separator _is_ the correct direction, then we should be the pane
        // to resize. Otherwise, just return false, as we couldn't handle it
        // either.
        const auto firstIsParent = _firstChild.try_as<ParentPane>();
        if (firstIsParent)
        {
            //if (firstIsParent->GetActivePane())
            //{
                return firstIsParent->ResizePane(direction) || _Resize(direction);
            //}
        }

        const auto secondIsParent = _secondChild.try_as<ParentPane>();
        if (secondIsParent)
        {
            //if (secondIsParent->GetActivePane())
            //{
                return secondIsParent->ResizePane(direction) || _Resize(direction);
            //}
        }

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
        const auto first = _desiredSplitPosition * 100.0f;
        const auto second = 100.0f - first;
        if (_splitState == SplitState::Vertical)
        {
            Root().ColumnDefinitions().Clear();

            // Create two columns in this grid: one for each pane

            auto firstColDef = Controls::ColumnDefinition();
            firstColDef.Width(GridLengthHelper::FromValueAndType(first, GridUnitType::Star));

            auto secondColDef = Controls::ColumnDefinition();
            secondColDef.Width(GridLengthHelper::FromValueAndType(second, GridUnitType::Star));

            Root().ColumnDefinitions().Append(firstColDef);
            Root().ColumnDefinitions().Append(secondColDef);
        }
        else if (_splitState == SplitState::Horizontal)
        {
            Root().RowDefinitions().Clear();

            // Create two rows in this grid: one for each pane

            auto firstRowDef = Controls::RowDefinition();
            firstRowDef.Height(GridLengthHelper::FromValueAndType(first, GridUnitType::Star));

            auto secondRowDef = Controls::RowDefinition();
            secondRowDef.Height(GridLengthHelper::FromValueAndType(second, GridUnitType::Star));

            Root().RowDefinitions().Append(firstRowDef);
            Root().RowDefinitions().Append(secondRowDef);
        }
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

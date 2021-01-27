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

    void ParentPane::FocusPane(uint32_t id)
    {
        _firstChild.FocusPane(id);
        _secondChild.FocusPane(id);
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

    bool ParentPane::NavigateFocus(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction)
    {
        // Check if either our first or second child is the currently focused leaf.
        // If it is, and the requested move direction matches our separator, then
        // we're the pane that needs to handle this focus move.
        const auto firstIsLeaf = _firstChild.try_as<LeafPane>();
        const auto secondIsLeaf = _secondChild.try_as<LeafPane>();
        const bool firstIsFocused = firstIsLeaf && firstIsLeaf.WasLastFocused();
        const bool secondIsFocused = secondIsLeaf && secondIsLeaf.WasLastFocused();
        if (firstIsFocused || secondIsFocused)
        {
            return _NavigateFocus(direction);
        }

        // If neither of our children were the focused leaf, then recurse into
        // our children and see if they can handle the focus move.
        // For each child, if it has a focused descendant, try having that child
        // handle the focus move.
        // If the child wasn't able to handle the focus move, it's possible that
        // there were no descendants with a separator the correct direction. If
        // our separator _is_ the correct direction, then we should be the pane
        // to move focus into our other child. Otherwise, just return false, as
        // we couldn't handle it either.
        const auto firstIsParent = _firstChild.try_as<ParentPane>();
        if (firstIsParent)
        {
            //if (firstIsParent->GetActivePane())
            //{
                return firstIsParent->NavigateFocus(direction) || _NavigateFocus(direction);
            //}
        }

        const auto secondIsParent = _secondChild.try_as<ParentPane>();
        if (secondIsParent)
        {
            //if (secondIsParent->GetActivePane())
            //{
                return secondIsParent->NavigateFocus(direction) || _NavigateFocus(direction);
            //}
        }

        return false;
    }

    float ParentPane::CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
    {
        const auto [lower, higher] = _CalcSnappedDimension(widthOrHeight, dimension);
        return dimension - lower < higher - dimension ? lower : higher;
    }

    int ParentPane::GetLeafPaneCount() const noexcept
    {
        return _firstChild.GetLeafPaneCount() + _secondChild.GetLeafPaneCount();
    }

    Size ParentPane::GetMinSize() const
    {
        const auto firstSize = _firstChild.GetMinSize();
        const auto secondSize = _secondChild.GetMinSize();

        const auto minWidth = _splitState == SplitState::Vertical ?
                                  firstSize.Width + secondSize.Width :
                                  std::max(firstSize.Width, secondSize.Width);
        const auto minHeight = _splitState == SplitState::Horizontal ?
                                   firstSize.Height + secondSize.Height :
                                   std::max(firstSize.Height, secondSize.Height);

        return { minWidth, minHeight };
    }

    LeafPane* FindFirstLeaf()
    {
        //return _firstChild.FindFirstLeaf();
        return {};
    }

    void ParentPane::PropagateToLeaves(std::function<void(LeafPane&)> action)
    {
        //_firstChild.PropagateToLeaves(action);
        //_secondChild.PropagateToLeaves(action);
    }

    void ParentPane::PropagateToLeavesOnEdge(const ResizeDirection& edge, std::function<void(LeafPane&)> action)
    {
        if (DirectionMatchesSplit(edge, _splitState))
        {
            const auto adjacentChild = (_splitState == SplitState::Vertical && edge == ResizeDirection::Left ||
                                        _splitState == SplitState::Horizontal && edge == ResizeDirection::Up) ?
                                           _firstChild :
                                           _secondChild;
            //adjacentChild.PropagateToLeavesOnEdge(edge, action);
        }
        else
        {
            //_firstChild.PropagateToLeavesOnEdge(edge, action);
            //_secondChild.PropagateToLeavesOnEdge(edge, action);
        }
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

    bool ParentPane::_Resize(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& direction)
    {
        if (!DirectionMatchesSplit(direction, _splitState))
        {
            return false;
        }

        float amount = .05f;
        if (direction == ResizeDirection::Right || direction == ResizeDirection::Down)
        {
            amount = -amount;
        }

        // Make sure we're not making a pane explode here by resizing it to 0 characters.
        const bool changeWidth = _splitState == SplitState::Vertical;

        const Size actualSize{ gsl::narrow_cast<float>(Root().ActualWidth()),
                               gsl::narrow_cast<float>(Root().ActualHeight()) };
        // actualDimension is the size in DIPs of this pane in the direction we're
        // resizing.
        const auto actualDimension = changeWidth ? actualSize.Width : actualSize.Height;

        _desiredSplitPosition = _ClampSplitPosition(changeWidth, _desiredSplitPosition - amount, actualDimension);

        // Resize our columns to match the new percentages.
        ResizeContent(actualSize);

        return true;
    }

    bool ParentPane::_NavigateFocus(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction)
    {
        if (!DirectionMatchesSplit(direction, _splitState))
        {
            return false;
        }

        const bool focusSecond = (direction == FocusDirection::Right) || (direction == FocusDirection::Down);

        const auto newlyFocusedChild = focusSecond ? _secondChild : _firstChild;
        return false;
        // TODO: Implement find first leaf
        // If the child we want to move focus to is _already_ focused, return false,
        // to try and let our parent figure it out.
        //if (newlyFocusedChild.GetActivePane())
        //{
        //    return false;
        //}

        // Transfer focus to our child, and update the focus of our tree.
        //newlyFocusedChild->_FocusFirstChild();

        //return true;
    }

    void ParentPane::_CloseChild(const bool closeFirst)
    {
        // The closed child must always be a leaf.
        const auto closedChild = (closeFirst ? _firstChild.try_as<LeafPane>() : _secondChild.try_as<LeafPane>());
        THROW_HR_IF_NULL(E_FAIL, closedChild);

        const auto remainingChild = closeFirst ? _secondChild.try_as<LeafPane>() : _firstChild.try_as<LeafPane>();

        // Detach all the controls form our grid, so they can be attached later.
        Root().Children().Clear();

        const auto closedChildDir = (_splitState == SplitState::Vertical) ?
                                        (closeFirst ? ResizeDirection::Left : ResizeDirection::Right) :
                                        (closeFirst ? ResizeDirection::Up : ResizeDirection::Down);

        // On all the leaf descendants that were adjacent to the closed child, update its
        // border, so that it matches the border of the closed child.
        //remainingChild.PropagateToLeavesOnEdge(closedChildDir, [=](LeafPane& paneOnEdge) {
        //    paneOnEdge.UpdateBorderWithClosedNeighbour(closedChild, closedChildDir);
        //});

        _ChildClosedHandlers(remainingChild);

        // If any children of closed pane was previously active, we move the focus to the remaining
        // child. We do that after we invoke the ChildClosed event, because it attaches that child's
        // control to xaml tree and only then can it properly gain focus.
        //if (closedChild.FindActivePane())
        //{
        //    remainingChild._FindFirstLeaf()->SetActive(true);
        //}
    }

    std::pair<float, float> ParentPane::_CalcChildrenSizes(const float fullSize) const
    {
        const auto widthOrHeight = _splitState == SplitState::Vertical;
        const auto snappedSizes = _CalcSnappedChildrenSizes(widthOrHeight, fullSize).lower;

        // Keep the first pane snapped and give the second pane all remaining size
        return {
            snappedSizes.first,
            fullSize - snappedSizes.first
        };
    }

    ParentPane::SnapChildrenSizeResult ParentPane::_CalcSnappedChildrenSizes(const bool /*widthOrHeight*/, const float /*fullSize*/) const
    {
        //auto sizeTree = _CreateMinSizeTree(widthOrHeight);
        //LayoutSizeNode lastSizeTree{ sizeTree };

        //while (sizeTree.size < fullSize)
        //{
        //    lastSizeTree = sizeTree;
        //    _AdvanceSnappedDimension(widthOrHeight, sizeTree);

        //    if (sizeTree.size == fullSize)
        //    {
        //        // If we just hit exactly the requested value, then just return the
        //        // current state of children.
        //        return { { sizeTree.firstChild->size, sizeTree.secondChild->size },
        //                 { sizeTree.firstChild->size, sizeTree.secondChild->size } };
        //    }
        //}

        //// We exceeded the requested size in the loop above, so lastSizeTree will have
        //// the last good sizes (so that children fit in) and sizeTree has the next possible
        //// snapped sizes. Return them as lower and higher snap possibilities.
        //return { { lastSizeTree.firstChild->size, lastSizeTree.secondChild->size },
        //         { sizeTree.firstChild->size, sizeTree.secondChild->size } };
        return {};
    }

    ParentPane::SnapSizeResult ParentPane::_CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
    {
        if (_splitState == (widthOrHeight ? SplitState::Horizontal : SplitState::Vertical))
        {
            // If we're resizing along separator axis, snap to the closest possibility
            // given by our children panes.

            //const auto firstSnapped = _firstChild._CalcSnappedDimension(widthOrHeight, dimension);
            //const auto secondSnapped = _secondChild._CalcSnappedDimension(widthOrHeight, dimension);
            //return {
            //    std::max(firstSnapped.lower, secondSnapped.lower),
            //    std::min(firstSnapped.higher, secondSnapped.higher)
            //};
            return {};
        }
        else
        {
            // If we're resizing perpendicularly to separator axis, calculate the sizes
            // of child panes that would fit the given size. We use same algorithm that
            // is used for real resize routine, but exclude the remaining empty space that
            // would appear after the second pane. This will be the 'downward' snap possibility,
            // while the 'upward' will be given as a side product of the layout function.

            const auto childSizes = _CalcSnappedChildrenSizes(widthOrHeight, dimension);
            return {
                childSizes.lower.first + childSizes.lower.second,
                childSizes.higher.first + childSizes.higher.second
            };
        }
    }

    void ParentPane::_AdvanceSnappedDimension(const bool /*widthOrHeight*/, LayoutSizeNode& /*sizeNode*/) const
    {
        //// We're a parent pane, so we have to advance dimension of our children panes. In
        //// fact, we advance only one child (chosen later) to keep the growth fine-grained.

        //// To choose which child pane to advance, we actually need to know their advanced sizes
        //// in advance (oh), to see which one would 'fit' better. Often, this is already cached
        //// by the previous invocation of this function in nextFirstChild and nextSecondChild
        //// fields of given node. If not, we need to calculate them now.
        //if (sizeNode.nextFirstChild == nullptr)
        //{
        //    //sizeNode.nextFirstChild = LayoutSizeNode(*sizeNode.firstChild);
        //    //_firstChild._AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextFirstChild);
        //}
        //if (sizeNode.nextSecondChild == nullptr)
        //{
        //    //sizeNode.nextSecondChild = LayoutSizeNode(*sizeNode.secondChild);
        //    //_secondChild._AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextSecondChild);
        //}

        //const auto nextFirstSize = sizeNode.nextFirstChild->size;
        //const auto nextSecondSize = sizeNode.nextSecondChild->size;

        //// Choose which child pane to advance.
        //bool advanceFirstOrSecond;
        //if (_splitState == (widthOrHeight ? SplitState::Horizontal : SplitState::Vertical))
        //{
        //    // If we're growing along separator axis, choose the child that
        //    // wants to be smaller than the other, so that the resulting size
        //    // will be the smallest.
        //    advanceFirstOrSecond = nextFirstSize < nextSecondSize;
        //}
        //else
        //{
        //    // If we're growing perpendicularly to separator axis, choose a
        //    // child so that their size ratio is closer to that we're trying
        //    // to maintain (this is, the relative separator position is closer
        //    // to the _desiredSplitPosition field).

        //    const auto firstSize = sizeNode.firstChild->size;
        //    const auto secondSize = sizeNode.secondChild->size;

        //    // Because we rely on equality check, these calculations have to be
        //    // immune to floating point errors. In common situation where both panes
        //    // have the same character sizes and _desiredSplitPosition is 0.5 (or
        //    // some simple fraction) both ratios will often be the same, and if so
        //    // we always take the left child. It could be right as well, but it's
        //    // important that it's consistent: that it would always go
        //    // 1 -> 2 -> 1 -> 2 -> 1 -> 2 and not like 1 -> 1 -> 2 -> 2 -> 2 -> 1
        //    // which would look silly to the user but which occur if there was
        //    // a non-floating-point-safe math.
        //    const auto deviation1 = nextFirstSize - (nextFirstSize + secondSize) * _desiredSplitPosition;
        //    const auto deviation2 = -1 * (firstSize - (firstSize + nextSecondSize) * _desiredSplitPosition);
        //    advanceFirstOrSecond = deviation1 <= deviation2;
        //}

        //// Here we advance one of our children. Because we already know the appropriate
        //// (advanced) size that given child would need to have, we simply assign that size
        //// to it. We then advance its 'next*' size (nextFirstChild or nextSecondChild) so
        //// the invariant holds (as it will likely be used by the next invocation of this
        //// function). The other child's next* size remains unchanged because its size
        //// haven't changed either.
        //if (advanceFirstOrSecond)
        //{
        //    *sizeNode.firstChild = *sizeNode.nextFirstChild;
        //    //_firstChild._AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextFirstChild);
        //}
        //else
        //{
        //    *sizeNode.secondChild = *sizeNode.nextSecondChild;
        //    //_secondChild._AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextSecondChild);
        //}

        //// Since the size of one of our children has changed we need to update our size as well.
        //if (_splitState == (widthOrHeight ? SplitState::Horizontal : SplitState::Vertical))
        //{
        //    sizeNode.size = std::max(sizeNode.firstChild->size, sizeNode.secondChild->size);
        //}
        //else
        //{
        //    sizeNode.size = sizeNode.firstChild->size + sizeNode.secondChild->size;
        //}

        //// Because we have grown, we're certainly no longer of our
        //// minimal size (if we've ever been).
        //sizeNode.isMinimumSize = false;
    }

    //ParentPane::LayoutSizeNode ParentPane::_CreateMinSizeTree(const bool widthOrHeight) const
    //{
    //    const auto size = GetMinSize();
    //    LayoutSizeNode node(widthOrHeight ? size.Width : size.Height);
    //    //node.firstChild = LayoutSizeNode(_firstChild._CreateMinSizeTree(widthOrHeight));
    //    //node.secondChild = LayoutSizeNode(_secondChild._CreateMinSizeTree(widthOrHeight));

    //    return node;
    //}

    float ParentPane::_ClampSplitPosition(const bool widthOrHeight, const float requestedValue, const float totalSize) const
    {
        const auto firstMinSize = _firstChild.GetMinSize();
        const auto secondMinSize = _secondChild.GetMinSize();

        const auto firstMinDimension = widthOrHeight ? firstMinSize.Width : firstMinSize.Height;
        const auto secondMinDimension = widthOrHeight ? secondMinSize.Width : secondMinSize.Height;

        const auto minSplitPosition = firstMinDimension / totalSize;
        const auto maxSplitPosition = 1.0f - (secondMinDimension / totalSize);

        return std::clamp(requestedValue, minSplitPosition, maxSplitPosition);
    }

    DEFINE_EVENT(ParentPane, ChildClosed, _ChildClosedHandlers, winrt::delegate<LeafPane>);
}

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ParentPane.h"
#include "Profile.h"
#include "CascadiaSettings.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::TerminalApp;
using namespace TerminalApp;

ParentPane::ParentPane(std::shared_ptr<LeafPane> firstChild, std::shared_ptr<LeafPane> secondChild, SplitState splitState, float splitPosition, Size currentSize) :
    _firstChild(std::static_pointer_cast<Pane>(firstChild)),
    _secondChild(std::static_pointer_cast<Pane>(secondChild)),
    _splitState(splitState),
    _desiredSplitPosition(splitPosition)

{
    _CreateRowColDefinitions(currentSize);

    _GetGridSetColOrRowFunc()(firstChild->GetRootElement(), 0);
    _GetGridSetColOrRowFunc()(secondChild->GetRootElement(), 1);
}

// Method Description:
// - Sets up row/column definitions for this pane. There are two total row/cols, 
//   one for each children and are given a size in pixels, based off the
//   available space, and the percent of the space they respectively consume,
//   which is stored in _desiredSplitPosition
// Arguments:
// - rootSize: The dimensions in pixels that this pane (and its children should consume.)
// Return Value:
// - <none>
void ParentPane::_CreateRowColDefinitions(const Size& rootSize)
{
    if (_splitState == SplitState::Vertical)
    {
        _root.ColumnDefinitions().Clear();

        // Create two columns in this grid: one for each pane
        const auto paneSizes = _CalcChildrenSizes(rootSize.Width);

        auto firstColDef = Controls::ColumnDefinition();
        firstColDef.Width(GridLengthHelper::FromPixels(paneSizes.first));

        auto secondColDef = Controls::ColumnDefinition();
        secondColDef.Width(GridLengthHelper::FromPixels(paneSizes.second));

        _root.ColumnDefinitions().Append(firstColDef);
        _root.ColumnDefinitions().Append(secondColDef);
    }
    else
    {
        _root.RowDefinitions().Clear();

        // Create two rows in this grid: one for each pane
        const auto paneSizes = _CalcChildrenSizes(rootSize.Height);

        auto firstRowDef = Controls::RowDefinition();
        firstRowDef.Height(GridLengthHelper::FromPixels(paneSizes.first));

        auto secondRowDef = Controls::RowDefinition();
        secondRowDef.Height(GridLengthHelper::FromPixels(paneSizes.second));

        _root.RowDefinitions().Append(firstRowDef);
        _root.RowDefinitions().Append(secondRowDef);
    }
}

// Method Description:
// - Called after ctor to start listening to our children and append their xaml 
//   elements. Call this only when both children can be attached (ep. they are not 
//   already attached somewhere).
// Arguments:
// - <none>
// Return Value:
// - <none>
void ParentPane::InitializeChildren()
{
    _root.Children().Append(_firstChild->GetRootElement());
    _root.Children().Append(_secondChild->GetRootElement());
    _SetupChildEventHandlers(true);
    _SetupChildEventHandlers(false);
}

// Method Description:
// - Setups all the event handlers we care about for a given child.
// - It is called on initialization, for both children, and when one of our children
//   changes (this is, it gets splitted or collapsed after split).
// Arguments:
// - firstChild - true to deal with first child, false for second child
// Return Value:
// - <none>
void ParentPane::_SetupChildEventHandlers(bool firstChild)
{
    auto& child = firstChild ? _firstChild : _secondChild;
    auto& closedToken = firstChild ? _firstClosedToken : _secondClosedToken;
    auto& typeChangedToken = firstChild ? _firstTypeChangedToken : _secondTypeChangedToken;

    if (const auto childAsLeaf = std::dynamic_pointer_cast<LeafPane>(child))
    {
        // When our child is a leaf and got closed, we, well, just close him.
        closedToken = childAsLeaf->Closed([=, &child](auto&& /*s*/, auto&& /*e*/) {
            // Unsubscribe from events of both our children, as we ourself will also
            // get closed when our child does.
            _RemoveAllChildEventHandlers(true);
            _RemoveAllChildEventHandlers(false);

            _CloseChild(firstChild);
        });

        // When our child is a leaf and got splitted, it produces the new parent pane that contains 
        // both him and the new leaf near him. We then replace that child with the new parent pane.
        typeChangedToken = childAsLeaf->Splitted([=, &child](std::shared_ptr<ParentPane> splittedChild) {
            // Unsub form all the events of the child. It will now have a new parent and we
            // don't care about him anymore.
            _RemoveAllChildEventHandlers(firstChild);

            child = splittedChild;
            _root.Children().SetAt(firstChild ? 0 : 1, child->GetRootElement());
            _GetGridSetColOrRowFunc()(child->GetRootElement(), firstChild ? 0 : 1);

            // The child is now a ParentPane. Setup events appropriate for him.
            _SetupChildEventHandlers(firstChild);
        });
    }
    else if (const auto childAsParent = std::dynamic_pointer_cast<ParentPane>(child))
    {
        // When our child is a parent and one of its children got closed (and so the parent collapses),
        // we take in its remaining, orphaned child as our own.
        typeChangedToken = childAsParent->ChildClosed([=, &child](std::shared_ptr<Pane> collapsedChild) {
            // Unsub form all the events of the parent child. It will get destroyed, so don't
            // leak its ref count.
            _RemoveAllChildEventHandlers(firstChild);

            child = collapsedChild;
            _root.Children().SetAt(firstChild ? 0 : 1, child->GetRootElement());
            _GetGridSetColOrRowFunc()(child->GetRootElement(), firstChild ? 0 : 1);

            // The child is now a LeafPane. Setup events appropriate for him.
            _SetupChildEventHandlers(firstChild);
        });
    }
}

// Method Description:
// - Unsubscribes from all the events of a given child that we're subscribed to.
// - Called when the child gets splitted/collapsed (because it is now our child then),
//   when a child closes and in dtor.
// Arguments:
// - firstChild - true to deal with first child, false for second child
// Return Value:
// - <none>
void ParentPane::_RemoveAllChildEventHandlers(bool firstChild)
{
    const auto child = firstChild ? _firstChild : _secondChild;
    const auto closedToken = firstChild ? _firstClosedToken : _secondClosedToken;
    const auto typeChangedToken = firstChild ? _firstTypeChangedToken : _secondTypeChangedToken;

    if (const auto childAsLeaf = std::dynamic_pointer_cast<LeafPane>(child))
    {
        childAsLeaf->Closed(closedToken);
        childAsLeaf->Splitted(typeChangedToken);
    }
    else if (const auto childAsParent = std::dynamic_pointer_cast<ParentPane>(child))
    {
        childAsParent->ChildClosed(typeChangedToken);
    }
}

// Method Description:
// - Returns an appropriate function to set a row or column on grid, depending on _splitState.
// Arguments:
// - <none>
// Return Value:
// - Controls::Grid::SetColumn when _splitState is Vertical
// - Controls::Grid::SetRow when _splitState is Horizontal
std::function<void(winrt::Windows::UI::Xaml::FrameworkElement const&, int32_t)> ParentPane::_GetGridSetColOrRowFunc() const noexcept
{
    if (_splitState == SplitState::Vertical)
    {
        return Controls::Grid::SetColumn;
    }
    else
    {
        return Controls::Grid::SetRow;
    }
}

std::shared_ptr<LeafPane> ParentPane::FindActivePane()
{
    auto firstFocused = _firstChild->FindActivePane();
    if (firstFocused != nullptr)
    {
        return firstFocused;
    }
    return _secondChild->FindActivePane();
}

std::shared_ptr<LeafPane> ParentPane::_FindFirstLeaf()
{
    return _firstChild->_FindFirstLeaf();
}

void ParentPane::PropagateToLeaves(std::function<void(LeafPane&)> action)
{
    _firstChild->PropagateToLeaves(action);
    _secondChild->PropagateToLeaves(action);
}

void ParentPane::PropagateToLeavesOnEdge(const winrt::TerminalApp::Direction& edge, std::function<void(LeafPane&)> action)
{
    if (DirectionMatchesSplit(edge, _splitState))
    {
        const auto adjacentChild = (_splitState == SplitState::Vertical && edge == Direction::Left ||
                                    _splitState == SplitState::Horizontal && edge == Direction::Up) ?
                                       _firstChild :
                                       _secondChild;
        adjacentChild->PropagateToLeavesOnEdge(edge, action);
    }
    else
    {
        _firstChild->PropagateToLeavesOnEdge(edge, action);
        _secondChild->PropagateToLeavesOnEdge(edge, action);
    }
}

void ParentPane::UpdateSettings(const TerminalSettings& settings, const GUID& profile)
{
    _firstChild->UpdateSettings(settings, profile);
    _secondChild->UpdateSettings(settings, profile);
}

// Method Description:
// - Update the size of this pane. Resizes each of our columns so they have the
//   same relative sizes, given the newSize.
// - Because we're just manually setting the row/column sizes in pixels, we have
//   to be told our new size, we can't just use our own OnSized event, because
//   that _won't fire when we get smaller_.
// Arguments:
// - newSize: the amount of space that this pane has to fill now.
// Return Value:
// - <none>
void ParentPane::ResizeContent(const Size& newSize)
{
    const auto width = newSize.Width;
    const auto height = newSize.Height;

    _CreateRowColDefinitions(newSize);

    Size firstSize, secondSize;
    if (_splitState == SplitState::Vertical)
    {
        const auto paneSizes = _CalcChildrenSizes(width);

        firstSize = { paneSizes.first, height };
        secondSize = { paneSizes.second, height };
    }
    else if (_splitState == SplitState::Horizontal)
    {
        const auto paneSizes = _CalcChildrenSizes(height);

        firstSize = { width, paneSizes.first };
        secondSize = { width, paneSizes.second };
    }

    _firstChild->ResizeContent(firstSize);
    _secondChild->ResizeContent(secondSize);
}

// Method Description:
// - Recalculates and reapplies sizes of all descendant panes.
// Arguments:
// - <none>
// Return Value:
// - <none>
void ParentPane::Relayout()
{
    ResizeContent(_root.ActualSize());
}

// Method Description:
// - Changes the relative sizes of our children, so that the separation point moves
//   in given direction. Chooses ParentPane that's closest depth-wise to the currently 
//   focused LeafPane, that's also in the correct direction to be moved. If it didn't
//   find one in the tree, then this method returns false, as we couldn't handle the resize.
// Arguments:
// - direction: The direction to move the separation point in.
// Return Value:
// - true if we or a child handled this resize request.
bool ParentPane::ResizeChild(const Direction& direction)
{
    // Check if either our first or second child is the currently focused leaf.
    // If it is, and the requested resize direction matches our separator, then
    // we're the pane that needs to adjust its separator.
    // If our separator is the wrong direction, then we can't handle it.
    const auto firstChildAsLeaf = std::dynamic_pointer_cast<LeafPane>(_firstChild);
    const auto secondChildAsLeaf = std::dynamic_pointer_cast<LeafPane>(_secondChild);
    const bool firstIsFocused = firstChildAsLeaf && firstChildAsLeaf->WasLastActive();
    const bool secondIsFocused = secondChildAsLeaf && secondChildAsLeaf->WasLastActive();
    if (firstIsFocused || secondIsFocused)
    {
        return _ResizeChild(direction);
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
    if (auto firstChildAsParent = std::dynamic_pointer_cast<ParentPane>(_firstChild))
    {
        if (_firstChild->FindActivePane())
        {
            return firstChildAsParent->ResizeChild(direction) || _ResizeChild(direction);
        }
    }

    if (auto secondChildAsParent = std::dynamic_pointer_cast<ParentPane>(_secondChild))
    {
        if (_secondChild->FindActivePane())
        {
            return secondChildAsParent->ResizeChild(direction) || _ResizeChild(direction);
        }
    }

    return false;
}

// Method Description:
// - Adjust our child percentages to increase the size of one of our children
//   and decrease the size of the other.
// - Adjusts the separation amount by 5%
// Arguments:
// - direction: the direction to move our separator. If it's down or right,
//   we'll be increasing the size of the first of our children. Else, we'll be
//   decreasing the size of our first child.
// Return Value:
// - false if we couldn't resize this pane in the given direction, else true.
bool ParentPane::_ResizeChild(const Direction& direction)
{
    if (!DirectionMatchesSplit(direction, _splitState))
    {
        return false;
    }

    float amount = .05f;
    if (direction == Direction::Left || direction == Direction::Up)
    {
        amount = -amount;
    }

    const bool changeWidth = _splitState == SplitState::Vertical;

    const Size actualSize{ gsl::narrow_cast<float>(_root.ActualWidth()),
                           gsl::narrow_cast<float>(_root.ActualHeight()) };
    // actualDimension is the size in DIPs of this pane in the direction we're
    // resizing.
    const auto actualDimension = changeWidth ? actualSize.Width : actualSize.Height;

    // Make sure we're not making a pane explode here by resizing it to 0 characters.
    _desiredSplitPosition = _ClampSplitPosition(changeWidth, _desiredSplitPosition + amount, actualDimension);

    // Resize our columns to match the new percentages.
    Relayout();

    return true;
}

// Method Description:
// - Attempts to move focus to one of our children. If we have a focused child,
//   we'll try to move the focus in the direction requested.
//   - If there isn't a pane that exists as a child of this pane in the correct
//     direction, we'll return false. This will indicate to our parent that they
//     should try and move the focus themselves. In this way, the focus can move
//     up and down the tree to the correct pane.
// - This method is _very_ similar to ResizeChild. Both are trying to find the
//   right separator to move (focus) in a direction.
// Arguments:
// - direction: The direction to move the focus in.
// Return Value:
// - true if we or a child handled this focus move request.
bool ParentPane::NavigateFocus(const Direction& direction)
{
    // Check if either our first or second child is the currently focused leaf.
    // If it is, and the requested move direction matches our separator, then
    // we're the pane that needs to handle this focus move.
    const auto firstChildAsLeaf = std::dynamic_pointer_cast<LeafPane>(_firstChild);
    const auto secondChildAsLeaf = std::dynamic_pointer_cast<LeafPane>(_secondChild);
    const bool firstIsFocused = firstChildAsLeaf && firstChildAsLeaf->WasLastActive();
    const bool secondIsFocused = secondChildAsLeaf && secondChildAsLeaf->WasLastActive();
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
    if (auto firstChildAsParent = std::dynamic_pointer_cast<ParentPane>(_firstChild))
    {
        if (_firstChild->FindActivePane())
        {
            return firstChildAsParent->NavigateFocus(direction) || _NavigateFocus(direction);
        }
    }

    if (auto secondChildAsParent = std::dynamic_pointer_cast<ParentPane>(_secondChild))
    {
        if (_secondChild->FindActivePane())
        {
            return secondChildAsParent->NavigateFocus(direction) || _NavigateFocus(direction);
        }
    }

    return false;
}

// Method Description:
// - Attempts to handle moving focus to one of our children. If our split
//   direction isn't appropriate for the move direction, then we'll return
//   false, to try and let our parent handle the move. If our child we'd move
//   focus to is already focused, we'll also return false, to again let our
//   parent try and handle the focus movement.
// Arguments:
// - direction: The direction to move the focus in.
// Return Value:
// - true if we handled this focus move request.
bool ParentPane::_NavigateFocus(const Direction& direction)
{
    if (!DirectionMatchesSplit(direction, _splitState))
    {
        return false;
    }

    const bool focusSecond = (direction == Direction::Right) || (direction == Direction::Down);
    const auto newlyFocusedChild = focusSecond ? _secondChild : _firstChild;

    // If the child we want to move focus to is _already_ focused, return false,
    // to try and let our parent figure it out.
    if (newlyFocusedChild->FindActivePane())
    {
        return false;
    }

    // Make sure to clear the focus from all the children that aren't going
    // to be focused.
    const auto notFocusedChild = focusSecond ? _firstChild : _secondChild;
    notFocusedChild->PropagateToLeaves([](LeafPane& pane) {
        pane.ClearActive();
    });

    // Transfer focus to our child.
    newlyFocusedChild->_FindFirstLeaf()->SetActive(true);

    return true;
}

// Method Description:
// - Closes one of our children. After that, this parent pare is useless as
//   it only has one child, so it should be replaced by the remaining child
//   in the ChildClosed event, which this function rises.
// Arguments:
// - closeFirst: if true, the first child should be closed, and the second
//   should be preserved, and vice-versa for false.
// Return Value:
// - <none>
void ParentPane::_CloseChild(const bool closeFirst)
{
    // The closed child must always be a leaf.
    const auto closedChild = std::dynamic_pointer_cast<LeafPane>(closeFirst ? _firstChild : _secondChild);
    THROW_HR_IF_NULL(E_FAIL, closedChild);

    const auto remainingChild = closeFirst ? _secondChild : _firstChild;

    // Detach all the controls form our grid, so they can be attached later.
    _root.Children().Clear();

    const auto closedChildDir = (_splitState == SplitState::Vertical) ?
                                    (closeFirst ? Direction::Left : Direction::Right) :
                                    (closeFirst ? Direction::Up : Direction::Down);

    // On all the leaf descendants that were adjacent to the closed child, update its
    // border, so that it matches the border of the closed child.
    remainingChild->PropagateToLeavesOnEdge(closedChildDir, [=](LeafPane& paneOnEdge) {
        paneOnEdge.UpdateBorderWithClosedNeightbour(closedChild, closedChildDir);
    });

    // When we invoke the ChildClosed event, our reference count might (and should) drop
    // to 0 and we'd become freed, so to prevent that we capture one more ref for the 
    // duration of this function.
    const auto lifeSaver = shared_from_this();

    _ChildClosedHandlers(remainingChild);

    // If any children of closed pane was previously active, we move the focus to the remaining
    // child. We do that after we invoke the ChildClosed event, because it attaches that child's
    // control to xaml tree and only then can it properly gain focus.
    if (closedChild->FindActivePane())
    {
        remainingChild->_FindFirstLeaf()->SetActive(true);
    }
}

// Method Description:
// - Gets the size in pixels of each of our children, given the full size they
//   should fill. Since these children own their own separators (borders), this
//   size is their portion of our _entire_ size. If specified size is lower than
//   required then children will be of minimum size. Snaps first child to grid
//   but not the second.
// Arguments:
// - fullSize: the amount of space in pixels that should be filled by our
//   children. Can be arbitrarily low.
// Return Value:
// - a pair with the size of our first child and the size of our second child,
//   respectively.
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

// Method Description:
// - Gets the size in pixels of each of our children, given the full size they should
//   fill. Each child is snapped to char grid as close as possible. If called multiple
//   times with fullSize argument growing, then both returned sizes are guaranteed to be
//   non-decreasing (it's a monotonically increasing function). This is important, so that
//   user doesn't get any pane shrank when they actually expand the window or a parent pane.
//   That is also required by the layout algorithm.
// Arguments:
// - widthOrHeight: if true, operates on width, otherwise on height.
// - fullSize: the amount of space in pixels that should be filled by our children and
//   their separator. Can be arbitrarily low.
// Return Value:
// - a structure holding the result of this calculation. The 'lower' field represents the
//   children sizes that would fit in the fullSize, but might (and usually do) not fill it
//   completely. The 'higher' field represents the size of the children if they slightly exceed
//   the fullSize, but are snapped. If the children can be snapped and also exactly match
//   the fullSize, then both this fields have the same value that represent this situation.
Pane::SnapChildrenSizeResult ParentPane::_CalcSnappedChildrenSizes(const bool widthOrHeight, const float fullSize) const
{
    //   First we build a tree of nodes corresponding to the tree of our descendant panes.
    // Each node represents a size of given pane. At the beginning, each node has the minimum
    // size that the corresponding pane can have; so has the our (root) node. We then gradually
    // expand our node (which in turn expands some of the child nodes) until we hit the desired
    // size. Since each expand step (done in _AdvanceSnappedDimension()) guarantees that all the
    // sizes will be snapped, our return values is also snapped.
    //   Why do we do it this, iterative way? Why can't we just split the given size by
    // _desiredSplitPosition and snap it latter? Because it's hardly doable, if possible, to also
    // fulfill the monotonicity requirement that way. As the fullSize increases, the proportional
    // point that separates children panes also moves and cells sneak in the available area in
    // unpredictable way, regardless which child has the snap priority or whether we snap them
    // upward, downward or to nearest.
    //   With present way we run the same sequence of actions regardless to the fullSize value and
    // only just stop at various moments when the built sizes reaches it.  Eventually, this could
    // be optimized for simple cases like when both children are both leaves with the same character
    // size, but it doesn't seem to be beneficial.

    auto sizeTree = _CreateMinSizeTree(widthOrHeight);
    LayoutSizeNode lastSizeTree{ sizeTree };

    while (sizeTree.size < fullSize)
    {
        lastSizeTree = sizeTree;
        _AdvanceSnappedDimension(widthOrHeight, sizeTree);

        if (sizeTree.size == fullSize)
        {
            // If we just hit exactly the requested value, then just return the
            // current state of children.
            return { { sizeTree.firstChild->size, sizeTree.secondChild->size },
                     { sizeTree.firstChild->size, sizeTree.secondChild->size } };
        }
    }

    // We exceeded the requested size in the loop above, so lastSizeTree will have
    // the last good sizes (so that children fit in) and sizeTree has the next possible
    // snapped sizes. Return them as lower and higher snap possibilities.
    return { { lastSizeTree.firstChild->size, lastSizeTree.secondChild->size },
             { sizeTree.firstChild->size, sizeTree.secondChild->size } };
}

Pane::SnapSizeResult ParentPane::_CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
{
    if (_splitState == (widthOrHeight ? SplitState::Horizontal : SplitState::Vertical))
    {
        // If we're resizing along separator axis, snap to the closest possibility
        // given by our children panes.

        const auto firstSnapped = _firstChild->_CalcSnappedDimension(widthOrHeight, dimension);
        const auto secondSnapped = _secondChild->_CalcSnappedDimension(widthOrHeight, dimension);
        return {
            std::max(firstSnapped.lower, secondSnapped.lower),
            std::min(firstSnapped.higher, secondSnapped.higher)
        };
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

void ParentPane::_AdvanceSnappedDimension(const bool widthOrHeight, LayoutSizeNode& sizeNode) const
{
    // We're a parent pane, so we have to advance dimension of our children panes. In
    // fact, we advance only one child (chosen later) to keep the growth fine-grained.

    // To choose which child pane to advance, we actually need to know their advanced sizes
    // in advance (oh), to see which one would 'fit' better. Often, this is already cached
    // by the previous invocation of this function in nextFirstChild and nextSecondChild
    // fields of given node. If not, we need to calculate them now.
    if (sizeNode.nextFirstChild == nullptr)
    {
        sizeNode.nextFirstChild = std::make_unique<LayoutSizeNode>(*sizeNode.firstChild);
        _firstChild->_AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextFirstChild);
    }
    if (sizeNode.nextSecondChild == nullptr)
    {
        sizeNode.nextSecondChild = std::make_unique<LayoutSizeNode>(*sizeNode.secondChild);
        _secondChild->_AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextSecondChild);
    }

    const auto nextFirstSize = sizeNode.nextFirstChild->size;
    const auto nextSecondSize = sizeNode.nextSecondChild->size;

    // Choose which child pane to advance.
    bool advanceFirstOrSecond;
    if (_splitState == (widthOrHeight ? SplitState::Horizontal : SplitState::Vertical))
    {
        // If we're growing along separator axis, choose the child that
        // wants to be smaller than the other, so that the resulting size
        // will be the smallest.
        advanceFirstOrSecond = nextFirstSize < nextSecondSize;
    }
    else
    {
        // If we're growing perpendicularly to separator axis, choose a
        // child so that their size ratio is closer to that we're trying
        // to maintain (this is, the relative separator position is closer
        // to the _desiredSplitPosition field).

        const auto firstSize = sizeNode.firstChild->size;
        const auto secondSize = sizeNode.secondChild->size;

        // Because we rely on equality check, these calculations have to be
        // immune to floating point errors. In common situation where both panes
        // have the same character sizes and _desiredSplitPosition is 0.5 (or
        // some simple fraction) both ratios will often be the same, and if so
        // we always take the left child. It could be right as well, but it's
        // important that it's consistent: that it would always go
        // 1 -> 2 -> 1 -> 2 -> 1 -> 2 and not like 1 -> 1 -> 2 -> 2 -> 2 -> 1
        // which would look silly to the user but which occur if there was
        // a non-floating-point-safe math.
        const auto deviation1 = nextFirstSize - (nextFirstSize + secondSize) * _desiredSplitPosition;
        const auto deviation2 = -1 * (firstSize - (firstSize + nextSecondSize) * _desiredSplitPosition);
        advanceFirstOrSecond = deviation1 <= deviation2;
    }

    // Here we advance one of our children. Because we already know the appropriate
    // (advanced) size that given child would need to have, we simply assign that size
    // to it. We then advance its 'next*' size (nextFirstChild or nextSecondChild) so
    // the invariant holds (as it will likely be used by the next invocation of this
    // function). The other child's next* size remains unchanged because its size
    // haven't changed either.
    if (advanceFirstOrSecond)
    {
        *sizeNode.firstChild = *sizeNode.nextFirstChild;
        _firstChild->_AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextFirstChild);
    }
    else
    {
        *sizeNode.secondChild = *sizeNode.nextSecondChild;
        _secondChild->_AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextSecondChild);
    }

    // Since the size of one of our children has changed we need to update our size as well.
    if (_splitState == (widthOrHeight ? SplitState::Horizontal : SplitState::Vertical))
    {
        sizeNode.size = std::max(sizeNode.firstChild->size, sizeNode.secondChild->size);
    }
    else
    {
        sizeNode.size = sizeNode.firstChild->size + sizeNode.secondChild->size;
    }

    // Because we have grown, we're certainly no longer of our
    // minimal size (if we've ever been).
    sizeNode.isMinimumSize = false;
}

Size ParentPane::_GetMinSize() const
{
    const auto firstSize = _firstChild->_GetMinSize();
    const auto secondSize = _secondChild->_GetMinSize();

    const auto minWidth = _splitState == SplitState::Vertical ?
                              firstSize.Width + secondSize.Width :
                              std::max(firstSize.Width, secondSize.Width);
    const auto minHeight = _splitState == SplitState::Horizontal ?
                               firstSize.Height + secondSize.Height :
                               std::max(firstSize.Height, secondSize.Height);

    return { minWidth, minHeight };
}

Pane::LayoutSizeNode ParentPane::_CreateMinSizeTree(const bool widthOrHeight) const
{
    LayoutSizeNode node = Pane::_CreateMinSizeTree(widthOrHeight);

    // Pane::_CreateMinSizeTree only sets the size of this node, but since we are parent,
    // we have children, so include them in the size node.
    node.firstChild = std::make_unique<LayoutSizeNode>(_firstChild->_CreateMinSizeTree(widthOrHeight));
    node.secondChild = std::make_unique<LayoutSizeNode>(_secondChild->_CreateMinSizeTree(widthOrHeight));

    return node;
}

// Method Description:
// - Adjusts split position so that no child pane is smaller then its
//   minimum size
// Arguments:
// - widthOrHeight: if true, operates on width, otherwise on height.
// - requestedValue: split position value to be clamped
// - totalSize: size (width or height) of the parent pane
// Return Value:
// - split position (value in range <0.0, 1.0>)
float ParentPane::_ClampSplitPosition(const bool widthOrHeight, const float requestedValue, const float totalSize) const
{
    const auto firstMinSize = _firstChild->_GetMinSize();
    const auto secondMinSize = _secondChild->_GetMinSize();

    const auto firstMinDimension = widthOrHeight ? firstMinSize.Width : firstMinSize.Height;
    const auto secondMinDimension = widthOrHeight ? secondMinSize.Width : secondMinSize.Height;

    const auto minSplitPosition = firstMinDimension / totalSize;
    const auto maxSplitPosition = 1.0f - (secondMinDimension / totalSize);

    return std::clamp(requestedValue, minSplitPosition, maxSplitPosition);
}

DEFINE_EVENT(ParentPane, ChildClosed, _ChildClosedHandlers, winrt::delegate<std::shared_ptr<Pane>>);

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Pane.h"
#include "Profile.h"
#include "CascadiaSettings.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::TerminalApp;
using namespace TerminalApp;

Pane::Pane()
{
}

// Method Description:
// - Get the root UIElement of this pane. There may be a single TermControl as a
//   child, or an entire tree of grids and panes as children of this element.
// Arguments:
// - <none>
// Return Value:
// - the Grid acting as the root of this pane.
Controls::Grid Pane::GetRootElement() const
{
    return _root;
}

// Method Description:
// - Adjusts given dimension (width or height) so that all descendant terminals
//   align with their character grids as close as possible. Snaps to closes match
//   (either upward or downward). Also makes sure to fit in minimal sizes of the panes.
// Arguments:
// - widthOrHeight: if true operates on width, otherwise on height
// - dimension: a dimension (width or height) to snap
// Return Value:
// - A value corresponding to the next closest snap size for this Pane, either upward or downward
float Pane::CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
{
    const auto [lower, higher] = _CalcSnappedDimension(widthOrHeight, dimension);
    return dimension - lower < higher - dimension ? lower : higher;
}

// Method Description:
// - Builds a tree of LayoutSizeNode that matches the tree of panes. Each node
//   has minimum size that the corresponding pane can have.
// Arguments:
// - widthOrHeight: if true operates on width, otherwise on height
// Return Value:
// - Root node of built tree that matches this pane.
Pane::LayoutSizeNode Pane::_CreateMinSizeTree(const bool widthOrHeight) const
{
    const auto size = _GetMinSize();
    return LayoutSizeNode(widthOrHeight ? size.Width : size.Height);
}

//
//
//
//// Method Description:
//// - Fire our Closed event to tell our parent that we should be removed.
//// Arguments:
//// - <none>
//// Return Value:
//// - <none>
//void Pane::Close()
//{
//    // Fire our Closed event to tell our parent that we should be removed.
//    _ClosedHandlers(nullptr, nullptr);
//}
//
//// Method Description:
//// - If this is the last focused pane, returns itself. Returns nullptr if this
////   is a leaf and it's not focused. If it's a parent, it returns nullptr if no
////   children of this pane were the last pane to be focused, or the Pane that
////   _was_ the last pane to be focused (if there was one).
//// - This Pane's control might not currently be focused, if the tab itself is
////   not currently focused.
//// Return Value:
//// - nullptr if we're a leaf and unfocused, or no children were marked
////   `_lastActive`, else returns this
//std::shared_ptr<Pane> Pane::GetActivePane()
//{
//    if (_IsLeaf())
//    {
//        return _lastActive ? shared_from_this() : nullptr;
//    }
//
//    auto firstFocused = _firstChild->GetActivePane();
//    if (firstFocused != nullptr)
//    {
//        return firstFocused;
//    }
//    return _secondChild->GetActivePane();
//}
//
//// Method Description:
//// - Recursively remove the "Active" state from this Pane and all it's children.
//// - Updates our visuals to match our new state, including highlighting our borders.
//// Arguments:
//// - <none>
//// Return Value:
//// - <none>
//void Pane::ClearActive()
//{
//    _lastActive = false;
//    if (!_IsLeaf())
//    {
//        _firstChild->ClearActive();
//        _secondChild->ClearActive();
//    }
//    UpdateVisuals();
//}
//
//// Method Description:
//// - Returns true if this pane is currently focused, or there is a pane which is
////   a child of this pane that is actively focused
//// Arguments:
//// - <none>
//// Return Value:
//// - true if the currently focused pane is either this pane, or one of this
////   pane's descendants
//bool Pane::_HasFocusedChild() const noexcept
//{
//    // We're intentionally making this one giant expression, so the compiler
//    // will skip the following lookups if one of the lookups before it returns
//    // true
//    return (_control && _lastActive) ||
//           (_firstChild && _firstChild->_HasFocusedChild()) ||
//           (_secondChild && _secondChild->_HasFocusedChild());
//}
//
//// Method Description:
//// - Focuses this control if we're a leaf, or attempts to focus the first leaf
////   of our first child, recursively.
//// Arguments:
//// - <none>
//// Return Value:
//// - <none>
//void Pane::_FocusFirstChild()
//{
//    if (_IsLeaf())
//    {
//        _control.Focus(FocusState::Programmatic);
//    }
//    else
//    {
//        _firstChild->_FocusFirstChild();
//    }
//}
//
//// Method Description:
//// - Attempts to update the settings of this pane or any children of this pane.
////   * If this pane is a leaf, and our profile guid matches the parameter, then
////     we'll apply the new settings to our control.
////   * If we're not a leaf, we'll recurse on our children.
//// Arguments:
//// - settings: The new TerminalSettings to apply to any matching controls
//// - profile: The GUID of the profile these settings should apply to.
//// Return Value:
//// - <none>
//void Pane::UpdateSettings(const TerminalSettings& settings, const GUID& profile)
//{
//    if (!_IsLeaf())
//    {
//        _firstChild->UpdateSettings(settings, profile);
//        _secondChild->UpdateSettings(settings, profile);
//    }
//    else
//    {
//        if (profile == _profile)
//        {
//            _control.UpdateSettings(settings);
//        }
//    }
//}
//
//
//// Method Description:
//// - Sets up row/column definitions for this pane. There are three total
////   row/cols. The middle one is for the separator. The first and third are for
////   each of the child panes, and are given a size in pixels, based off the
////   availiable space, and the percent of the space they respectively consume,
////   which is stored in _desiredSplitPosition
//// - Does nothing if our split state is currently set to SplitState::None
//// Arguments:
//// - rootSize: The dimensions in pixels that this pane (and its children should consume.)
//// Return Value:
//// - <none>
//void Pane::_CreateRowColDefinitions(const Size& rootSize)
//{
//    if (_splitState == SplitState::Vertical)
//    {
//        _root.ColumnDefinitions().Clear();
//
//        // Create two columns in this grid: one for each pane
//        const auto paneSizes = _CalcChildrenSizes(rootSize.Width);
//
//        auto firstColDef = Controls::ColumnDefinition();
//        firstColDef.Width(GridLengthHelper::FromPixels(paneSizes.first));
//
//        auto secondColDef = Controls::ColumnDefinition();
//        secondColDef.Width(GridLengthHelper::FromPixels(paneSizes.second));
//
//        _root.ColumnDefinitions().Append(firstColDef);
//        _root.ColumnDefinitions().Append(secondColDef);
//    }
//    else if (_splitState == SplitState::Horizontal)
//    {
//        _root.RowDefinitions().Clear();
//
//        // Create two rows in this grid: one for each pane
//        const auto paneSizes = _CalcChildrenSizes(rootSize.Height);
//
//        auto firstRowDef = Controls::RowDefinition();
//        firstRowDef.Height(GridLengthHelper::FromPixels(paneSizes.first));
//
//        auto secondRowDef = Controls::RowDefinition();
//        secondRowDef.Height(GridLengthHelper::FromPixels(paneSizes.second));
//
//        _root.RowDefinitions().Append(firstRowDef);
//        _root.RowDefinitions().Append(secondRowDef);
//    }
//}
//
//// Method Description:
//// - Determines whether the pane can be split
//// Arguments:
//// - splitType: what type of split we want to create.
//// Return Value:
//// - True if the pane can be split. False otherwise.
//bool Pane::CanSplit(SplitState splitType)
//{
//    if (_IsLeaf())
//    {
//        return _CanSplit(splitType);
//    }
//
//    if (_firstChild->_HasFocusedChild())
//    {
//        return _firstChild->CanSplit(splitType);
//    }
//
//    if (_secondChild->_HasFocusedChild())
//    {
//        return _secondChild->CanSplit(splitType);
//    }
//
//    return false;
//}
//
//// Method Description:
//// - Split the focused pane in our tree of panes, and place the given
////   TermControl into the newly created pane. If we're the focused pane, then
////   we'll create two new children, and place them side-by-side in our Grid.
//// Arguments:
//// - splitType: what type of split we want to create.
//// - profile: The profile GUID to associate with the newly created pane.
//// - control: A TermControl to use in the new pane.
//// Return Value:
//// - The two newly created Panes
//std::pair<std::shared_ptr<Pane>, std::shared_ptr<Pane>> Pane::Split(SplitState splitType, const GUID& profile, const TermControl& control)
//{
//    if (!_IsLeaf())
//    {
//        if (_firstChild->_HasFocusedChild())
//        {
//            return _firstChild->Split(splitType, profile, control);
//        }
//        else if (_secondChild->_HasFocusedChild())
//        {
//            return _secondChild->Split(splitType, profile, control);
//        }
//
//        return { nullptr, nullptr };
//    }
//
//    return _Split(splitType, profile, control);
//}
//
//// Method Description:
//// - Does the bulk of the work of creating a new split. Initializes our UI,
////   creates a new Pane to host the control, registers event handlers.
//// Arguments:
//// - splitType: what type of split we should create.
//// - profile: The profile GUID to associate with the newly created pane.
//// - control: A TermControl to use in the new pane.
//// Return Value:
//// - The two newly created Panes
//std::pair<std::shared_ptr<Pane>, std::shared_ptr<Pane>> Pane::_Split(SplitState splitType, const GUID& profile, const TermControl& control)
//{
//    // Lock the create/close lock so that another operation won't concurrently
//    // modify our tree
//    std::unique_lock lock{ _createCloseLock };
//
//    // revoke our handler - the child will take care of the control now.
//    _control.ConnectionStateChanged(_connectionStateChangedToken);
//    _connectionStateChangedToken.value = 0;
//
//    // Remove our old GotFocus handler from the control. We don't what the
//    // control telling us that it's now focused, we want it telling its new
//    // parent.
//    _gotFocusRevoker.revoke();
//
//    _splitState = splitType;
//    _desiredSplitPosition = Half;
//
//    // Remove any children we currently have. We can't add the existing
//    // TermControl to a new grid until we do this.
//    _root.Children().Clear();
//    _border.Child(nullptr);
//
//    // Create two new Panes
//    //   Move our control, guid into the first one.
//    //   Move the new guid, control into the second.
//    _firstChild = std::make_shared<Pane>(_profile.value(), _control);
//    _profile = std::nullopt;
//    _control = { nullptr };
//    _secondChild = std::make_shared<Pane>(profile, control);
//
//    _CreateSplitContent();
//
//    _root.Children().Append(_firstChild->GetRootElement());
//    _root.Children().Append(_secondChild->GetRootElement());
//
//    _ApplySplitDefinitions();
//
//    // Register event handlers on our children to handle their Close events
//    _SetupChildCloseHandlers();
//
//    _lastActive = false;
//
//    return { _firstChild, _secondChild };
//}
//
//// Method Description:
//// - Adjusts given dimension (width or height) so that all descendant terminals
////   align with their character grids as close as possible. Also makes sure to
////   fit in minimal sizes of the panes.
//// Arguments:
//// - widthOrHeight: if true operates on width, otherwise on height
//// - dimension: a dimension (width or height) to be snapped
//// Return Value:
//// - pair of floats, where first value is the size snapped downward (not greater then
////   requested size) and second is the size snapped upward (not lower than requested size).
////   If requested size is already snapped, then both returned values equal this value.
//Pane::SnapSizeResult Pane::_CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
//{
//    if (_IsLeaf())
//    {
//        // If we're a leaf pane, align to the grid of controlling terminal
//
//        const auto minSize = _GetMinSize();
//        const auto minDimension = widthOrHeight ? minSize.Width : minSize.Height;
//
//        if (dimension <= minDimension)
//        {
//            return { minDimension, minDimension };
//        }
//
//        float lower = _control.SnapDimensionToGrid(widthOrHeight, dimension);
//        if (widthOrHeight)
//        {
//            lower += WI_IsFlagSet(_borders, Borders::Left) ? PaneBorderSize : 0;
//            lower += WI_IsFlagSet(_borders, Borders::Right) ? PaneBorderSize : 0;
//        }
//        else
//        {
//            lower += WI_IsFlagSet(_borders, Borders::Top) ? PaneBorderSize : 0;
//            lower += WI_IsFlagSet(_borders, Borders::Bottom) ? PaneBorderSize : 0;
//        }
//
//        if (lower == dimension)
//        {
//            // If we happen to be already snapped, then just return this size
//            // as both lower and higher values.
//            return { lower, lower };
//        }
//        else
//        {
//            const auto cellSize = _control.CharacterDimensions();
//            const auto higher = lower + (widthOrHeight ? cellSize.Width : cellSize.Height);
//            return { lower, higher };
//        }
//    }
//    else if (_splitState == (widthOrHeight ? SplitState::Horizontal : SplitState::Vertical))
//    {
//        // If we're resizing along separator axis, snap to the closest possibility
//        // given by our children panes.
//
//        const auto firstSnapped = _firstChild->_CalcSnappedDimension(widthOrHeight, dimension);
//        const auto secondSnapped = _secondChild->_CalcSnappedDimension(widthOrHeight, dimension);
//        return {
//            std::max(firstSnapped.lower, secondSnapped.lower),
//            std::min(firstSnapped.higher, secondSnapped.higher)
//        };
//    }
//    else
//    {
//        // If we're resizing perpendicularly to separator axis, calculate the sizes
//        // of child panes that would fit the given size. We use same algorithm that
//        // is used for real resize routine, but exclude the remaining empty space that
//        // would appear after the second pane. This will be the 'downward' snap possibility,
//        // while the 'upward' will be given as a side product of the layout function.
//
//        const auto childSizes = _CalcSnappedChildrenSizes(widthOrHeight, dimension);
//        return {
//            childSizes.lower.first + childSizes.lower.second,
//            childSizes.higher.first + childSizes.higher.second
//        };
//    }
//}
//
//// Method Description:
//// - Increases size of given LayoutSizeNode to match next possible 'snap'. In case of leaf
////   pane this means the next cell of the terminal. Otherwise it means that one of its children
////   advances (recursively). It expects the given node and its descendants to have either
////   already snapped or minimum size.
//// Arguments:
//// - widthOrHeight: if true operates on width, otherwise on height.
//// - sizeNode: a layouting node that corresponds to this pane.
//// Return Value:
//// - <none>
//void Pane::_AdvanceSnappedDimension(const bool widthOrHeight, LayoutSizeNode& sizeNode) const
//{
//    if (_IsLeaf())
//    {
//        // We're a leaf pane, so just add one more row or column (unless isMinimumSize
//        // is true, see below).
//
//        if (sizeNode.isMinimumSize)
//        {
//            // If the node is of its minimum size, this size might not be snapped (it might
//            // be, say, half a character, or fixed 10 pixels), so snap it upward. It might
//            // however be already snapped, so add 1 to make sure it really increases
//            // (not strictly necessary but to avoid surprises).
//            sizeNode.size = _CalcSnappedDimension(widthOrHeight, sizeNode.size + 1).higher;
//        }
//        else
//        {
//            const auto cellSize = _control.CharacterDimensions();
//            sizeNode.size += widthOrHeight ? cellSize.Width : cellSize.Height;
//        }
//    }
//    else
//    {
//        // We're a parent pane, so we have to advance dimension of our children panes. In
//        // fact, we advance only one child (chosen later) to keep the growth fine-grained.
//
//        // To choose which child pane to advance, we actually need to know their advanced sizes
//        // in advance (oh), to see which one would 'fit' better. Often, this is already cached
//        // by the previous invocation of this function in nextFirstChild and nextSecondChild
//        // fields of given node. If not, we need to calculate them now.
//        if (sizeNode.nextFirstChild == nullptr)
//        {
//            sizeNode.nextFirstChild = std::make_unique<LayoutSizeNode>(*sizeNode.firstChild);
//            _firstChild->_AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextFirstChild);
//        }
//        if (sizeNode.nextSecondChild == nullptr)
//        {
//            sizeNode.nextSecondChild = std::make_unique<LayoutSizeNode>(*sizeNode.secondChild);
//            _secondChild->_AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextSecondChild);
//        }
//
//        const auto nextFirstSize = sizeNode.nextFirstChild->size;
//        const auto nextSecondSize = sizeNode.nextSecondChild->size;
//
//        // Choose which child pane to advance.
//        bool advanceFirstOrSecond;
//        if (_splitState == (widthOrHeight ? SplitState::Horizontal : SplitState::Vertical))
//        {
//            // If we're growing along separator axis, choose the child that
//            // wants to be smaller than the other, so that the resulting size
//            // will be the smallest.
//            advanceFirstOrSecond = nextFirstSize < nextSecondSize;
//        }
//        else
//        {
//            // If we're growing perpendicularly to separator axis, choose a
//            // child so that their size ratio is closer to that we're trying
//            // to maintain (this is, the relative separator position is closer
//            // to the _desiredSplitPosition field).
//
//            const auto firstSize = sizeNode.firstChild->size;
//            const auto secondSize = sizeNode.secondChild->size;
//
//            // Because we rely on equality check, these calculations have to be
//            // immune to floating point errors. In common situation where both panes
//            // have the same character sizes and _desiredSplitPosition is 0.5 (or
//            // some simple fraction) both ratios will often be the same, and if so
//            // we always take the left child. It could be right as well, but it's
//            // important that it's consistent: that it would always go
//            // 1 -> 2 -> 1 -> 2 -> 1 -> 2 and not like 1 -> 1 -> 2 -> 2 -> 2 -> 1
//            // which would look silly to the user but which occur if there was
//            // a non-floating-point-safe math.
//            const auto deviation1 = nextFirstSize - (nextFirstSize + secondSize) * _desiredSplitPosition;
//            const auto deviation2 = -1 * (firstSize - (firstSize + nextSecondSize) * _desiredSplitPosition);
//            advanceFirstOrSecond = deviation1 <= deviation2;
//        }
//
//        // Here we advance one of our children. Because we already know the appropriate
//        // (advanced) size that given child would need to have, we simply assign that size
//        // to it. We then advance its 'next*' size (nextFirstChild or nextSecondChild) so
//        // the invariant holds (as it will likely be used by the next invocation of this
//        // function). The other child's next* size remains unchanged because its size
//        // haven't changed either.
//        if (advanceFirstOrSecond)
//        {
//            *sizeNode.firstChild = *sizeNode.nextFirstChild;
//            _firstChild->_AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextFirstChild);
//        }
//        else
//        {
//            *sizeNode.secondChild = *sizeNode.nextSecondChild;
//            _secondChild->_AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextSecondChild);
//        }
//
//        // Since the size of one of our children has changed we need to update our size as well.
//        if (_splitState == (widthOrHeight ? SplitState::Horizontal : SplitState::Vertical))
//        {
//            sizeNode.size = std::max(sizeNode.firstChild->size, sizeNode.secondChild->size);
//        }
//        else
//        {
//            sizeNode.size = sizeNode.firstChild->size + sizeNode.secondChild->size;
//        }
//    }
//
//    // Because we have grown, we're certainly no longer of our
//    // minimal size (if we've ever been).
//    sizeNode.isMinimumSize = false;
//}
//
//// Method Description:
//// - Get the absolute minimum size that this pane can be resized to and still
////   have 1x1 character visible, in each of its children. If we're a leaf, we'll
////   include the space needed for borders _within_ us.
//// Arguments:
//// - <none>
//// Return Value:
//// - The minimum size that this pane can be resized to and still have a visible
////   character.
//Size Pane::_GetMinSize() const
//{
//    if (_IsLeaf())
//    {
//        auto controlSize = _control.MinimumSize();
//        auto newWidth = controlSize.Width;
//        auto newHeight = controlSize.Height;
//
//        newWidth += WI_IsFlagSet(_borders, Borders::Left) ? PaneBorderSize : 0;
//        newWidth += WI_IsFlagSet(_borders, Borders::Right) ? PaneBorderSize : 0;
//        newHeight += WI_IsFlagSet(_borders, Borders::Top) ? PaneBorderSize : 0;
//        newHeight += WI_IsFlagSet(_borders, Borders::Bottom) ? PaneBorderSize : 0;
//
//        return { newWidth, newHeight };
//    }
//    else
//    {
//        const auto firstSize = _firstChild->_GetMinSize();
//        const auto secondSize = _secondChild->_GetMinSize();
//
//        const auto minWidth = _splitState == SplitState::Vertical ?
//                                  firstSize.Width + secondSize.Width :
//                                  std::max(firstSize.Width, secondSize.Width);
//        const auto minHeight = _splitState == SplitState::Horizontal ?
//                                   firstSize.Height + secondSize.Height :
//                                   std::max(firstSize.Height, secondSize.Height);
//
//        return { minWidth, minHeight };
//    }
//}

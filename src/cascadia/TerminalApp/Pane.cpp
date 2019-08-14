// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Pane.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::TerminalApp;

static const int PaneSeparatorSize = 4;
static const float Half = 0.50f;

Pane::Pane(const GUID& profile, const TermControl& control, const bool lastFocused) :
    _control{ control },
    _lastFocused{ lastFocused },
    _profile{ profile }
{
    _root.Children().Append(_control);
    _connectionClosedToken = _control.ConnectionClosed({ this, &Pane::_ControlClosedHandler });

    // Set the background of the pane to match that of the theme's default grid
    // background. This way, we'll match the small underline under the tabs, and
    // the UI will be consistent on bot light and dark modes.
    const auto res = Application::Current().Resources();
    const auto key = winrt::box_value(L"BackgroundGridThemeStyle");
    if (res.HasKey(key))
    {
        const auto g = res.Lookup(key);
        const auto style = g.try_as<winrt::Windows::UI::Xaml::Style>();
        // try_as fails by returning nullptr
        if (style)
        {
            _root.Style(style);
        }
    }
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
void Pane::ResizeContent(const Size& newSize)
{
    const auto width = newSize.Width;
    const auto height = newSize.Height;

    _CreateRowColDefinitions(newSize);

    if (_splitState == SplitState::Vertical)
    {
        const auto paneSizes = _GetPaneSizes(width);

        const Size firstSize{ paneSizes.first, height };
        const Size secondSize{ paneSizes.second, height };
        _firstChild->ResizeContent(firstSize);
        _secondChild->ResizeContent(secondSize);
    }
    else if (_splitState == SplitState::Horizontal)
    {
        const auto paneSizes = _GetPaneSizes(height);

        const Size firstSize{ width, paneSizes.first };
        const Size secondSize{ width, paneSizes.second };
        _firstChild->ResizeContent(firstSize);
        _secondChild->ResizeContent(secondSize);
    }
}

// Method Description:
// - Adjust our child percentages to increase the size of one of our children
//   and decrease the size of the other.
// - Adjusts the separation amount by 5%
// - Does nothing if the direction doesn't match our current split direction
// Arguments:
// - direction: the direction to move our separator. If it's down or right,
//   we'll be increasing the size of the first of our children. Else, we'll be
//   decreasing the size of our first child.
// Return Value:
// - false if we couldn't resize this pane in the given direction, else true.
bool Pane::_Resize(const Direction& direction)
{
    if (!DirectionMatchesSplit(direction, _splitState))
    {
        return false;
    }

    float amount = .05f;
    if (direction == Direction::Right || direction == Direction::Down)
    {
        amount = -amount;
    }

    // Make sure we're not making a pane explode here by resizing it to 0 characters.
    const bool changeWidth = _splitState == SplitState::Vertical;

    const Size actualSize{ gsl::narrow_cast<float>(_root.ActualWidth()),
                           gsl::narrow_cast<float>(_root.ActualHeight()) };
    // actualDimension is the size in DIPs of this pane in the direction we're
    // resizing.
    auto actualDimension = changeWidth ? actualSize.Width : actualSize.Height;
    actualDimension -= PaneSeparatorSize;

    const auto firstMinSize = _firstChild->_GetMinSize();
    const auto secondMinSize = _secondChild->_GetMinSize();

    // These are the minimum amount of space we need for each of our children
    const auto firstMinDimension = changeWidth ? firstMinSize.Width : firstMinSize.Height;
    const auto secondMinDimension = changeWidth ? secondMinSize.Width : secondMinSize.Height;

    const auto firstMinPercent = firstMinDimension / actualDimension;
    const auto secondMinPercent = secondMinDimension / actualDimension;

    // Make sure that the first pane doesn't get bigger than the space we need
    // to reserve for the second.
    const auto firstMaxPercent = 1.0f - secondMinPercent;

    _firstPercent = std::clamp(_firstPercent.value() - amount, firstMinPercent, firstMaxPercent);
    // Update the other child to fill the remaining percent
    _secondPercent = 1.0f - _firstPercent.value();

    // Resize our columns to match the new percentages.
    ResizeContent(actualSize);

    return true;
}

// Method Description:
// - Moves the separator between panes, as to resize each child on either size
//   of the separator. Tries to move a separator in the given direction. The
//   separator moved is the separator that's closest depth-wise to the
//   currently focused pane, that's also in the correct direction to be moved.
//   If there isn't such a separator, then this method returns false, as we
//   couldn't handle the resize.
// Arguments:
// - direction: The direction to move the separator in.
// Return Value:
// - true if we or a child handled this resize request.
bool Pane::ResizePane(const Direction& direction)
{
    // If we're a leaf, do nothing. We can't possibly have a descendant with a
    // separator the correct direction.
    if (_IsLeaf())
    {
        return false;
    }

    // Check if either our first or second child is the currently focused leaf.
    // If it is, and the requested resize direction matches our separator, then
    // we're the pane that needs to adjust its separator.
    // If our separator is the wrong direction, then we can't handle it.
    const bool firstIsFocused = _firstChild->_IsLeaf() && _firstChild->_lastFocused;
    const bool secondIsFocused = _secondChild->_IsLeaf() && _secondChild->_lastFocused;
    if (firstIsFocused || secondIsFocused)
    {
        return _Resize(direction);
    }
    else
    {
        // If neither of our children were the focused leaf, then recurse into
        // our children and see if they can handle the resize.
        // For each child, if it has a focused descendant, try having that child
        // handle the resize.
        // If the child wasn't able to handle the resize, it's possible that
        // there were no descendants with a separator the correct direction. If
        // our separator _is_ the correct direction, then we should be the pane
        // to resize. Otherwise, just return false, as we couldn't handle it
        // either.
        if ((!_firstChild->_IsLeaf()) && _firstChild->_HasFocusedChild())
        {
            return _firstChild->ResizePane(direction) || _Resize(direction);
        }
        else if ((!_secondChild->_IsLeaf()) && _secondChild->_HasFocusedChild())
        {
            return _secondChild->ResizePane(direction) || _Resize(direction);
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
bool Pane::_NavigateFocus(const Direction& direction)
{
    if (!DirectionMatchesSplit(direction, _splitState))
    {
        return false;
    }

    const bool focusSecond = (direction == Direction::Right) || (direction == Direction::Down);

    const auto newlyFocusedChild = focusSecond ? _secondChild : _firstChild;

    // If the child we want to move focus to is _already_ focused, return false,
    // to try and let our parent figure it out.
    if (newlyFocusedChild->WasLastFocused())
    {
        return false;
    }

    // Transfer focus to our child, and update the focus of our tree.
    newlyFocusedChild->_FocusFirstChild();
    UpdateFocus();

    return true;
}

// Method Description:
// - Attempts to move focus to one of our children. If we have a focused child,
//   we'll try to move the focus in the direction requested.
//   - If there isn't a pane that exists as a child of this pane in the correct
//     direction, we'll return false. This will indicate to our parent that they
//     should try and move the focus themselves. In this way, the focus can move
//     up and down the tree to the correct pane.
// - This method is _very_ similar to ResizePane. Both are trying to find the
//   right separator to move (focus) in a direction.
// Arguments:
// - direction: The direction to move the focus in.
// Return Value:
// - true if we or a child handled this focus move request.
bool Pane::NavigateFocus(const Direction& direction)
{
    // If we're a leaf, do nothing. We can't possibly have a descendant with a
    // separator the correct direction.
    if (_IsLeaf())
    {
        return false;
    }

    // Check if either our first or second child is the currently focused leaf.
    // If it is, and the requested move direction matches our separator, then
    // we're the pane that needs to handle this focus move.
    const bool firstIsFocused = _firstChild->_IsLeaf() && _firstChild->_lastFocused;
    const bool secondIsFocused = _secondChild->_IsLeaf() && _secondChild->_lastFocused;
    if (firstIsFocused || secondIsFocused)
    {
        return _NavigateFocus(direction);
    }
    else
    {
        // If neither of our children were the focused leaf, then recurse into
        // our children and see if they can handle the focus move.
        // For each child, if it has a focused descendant, try having that child
        // handle the focus move.
        // If the child wasn't able to handle the focus move, it's possible that
        // there were no descendants with a separator the correct direction. If
        // our separator _is_ the correct direction, then we should be the pane
        // to move focus into our other child. Otherwise, just return false, as
        // we couldn't handle it either.
        if ((!_firstChild->_IsLeaf()) && _firstChild->_HasFocusedChild())
        {
            return _firstChild->NavigateFocus(direction) || _NavigateFocus(direction);
        }
        else if ((!_secondChild->_IsLeaf()) && _secondChild->_HasFocusedChild())
        {
            return _secondChild->NavigateFocus(direction) || _NavigateFocus(direction);
        }
    }
    return false;
}

// Method Description:
// - Called when our attached control is closed. Triggers listeners to our close
//   event, if we're a leaf pane.
// - If this was called, and we became a parent pane (due to work on another
//   thread), this function will do nothing (allowing the control's new parent
//   to handle the event instead).
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_ControlClosedHandler()
{
    std::unique_lock lock{ _createCloseLock };
    // It's possible that this event handler started being executed, then before
    // we got the lock, another thread created another child. So our control is
    // actually no longer _our_ control, and instead could be a descendant.
    //
    // When the control's new Pane takes ownership of the control, the new
    // parent will register it's own event handler. That event handler will get
    // fired after this handler returns, and will properly cleanup state.
    if (!_IsLeaf())
    {
        return;
    }

    if (_control.ShouldCloseOnExit())
    {
        // Fire our Closed event to tell our parent that we should be removed.
        _closedHandlers();
    }
}

// Method Description:
// - Fire our Closed event to tell our parent that we should be removed.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::Close()
{
    // Fire our Closed event to tell our parent that we should be removed.
    _closedHandlers();
}

// Method Description:
// - Get the root UIElement of this pane. There may be a single TermControl as a
//   child, or an entire tree of grids and panes as children of this element.
// Arguments:
// - <none>
// Return Value:
// - the Grid acting as the root of this pane.
Controls::Grid Pane::GetRootElement()
{
    return _root;
}

// Method Description:
// - If this is the last focused pane, returns itself. Returns nullptr if this
//   is a leaf and it's not focused. If it's a parent, it returns nullptr if no
//   children of this pane were the last pane to be focused, or the Pane that
//   _was_ the last pane to be focused (if there was one).
// - This Pane's control might not currently be focused, if the tab itself is
//   not currently focused.
// Return Value:
// - nullptr if we're a leaf and unfocused, or no children were marked
//   `_lastFocused`, else returns this
std::shared_ptr<Pane> Pane::GetFocusedPane()
{
    if (_IsLeaf())
    {
        return _lastFocused ? shared_from_this() : nullptr;
    }
    else
    {
        auto firstFocused = _firstChild->GetFocusedPane();
        if (firstFocused != nullptr)
        {
            return firstFocused;
        }
        return _secondChild->GetFocusedPane();
    }
}

// Method Description:
// - Returns nullptr if no children of this pane were the last control to be
//   focused, or the TermControl that _was_ the last control to be focused (if
//   there was one).
// - This control might not currently be focused, if the tab itself is not
//   currently focused.
// Arguments:
// - <none>
// Return Value:
// - nullptr if no children were marked `_lastFocused`, else the TermControl
//   that was last focused.
TermControl Pane::GetFocusedTerminalControl()
{
    auto lastFocused = GetFocusedPane();
    return lastFocused ? lastFocused->_control : nullptr;
}

// Method Description:
// - Returns nullopt if no children of this pane were the last control to be
//   focused, or the GUID of the profile of the last control to be focused (if
//   there was one).
// Arguments:
// - <none>
// Return Value:
// - nullopt if no children of this pane were the last control to be
//   focused, else the GUID of the profile of the last control to be focused
std::optional<GUID> Pane::GetFocusedProfile()
{
    auto lastFocused = GetFocusedPane();
    return lastFocused ? lastFocused->_profile : std::nullopt;
}

// Method Description:
// - Returns true if this pane was the last pane to be focused in a tree of panes.
// Arguments:
// - <none>
// Return Value:
// - true iff we were the last pane focused in this tree of panes.
bool Pane::WasLastFocused() const noexcept
{
    return _lastFocused;
}

// Method Description:
// - Returns true iff this pane has no child panes.
// Arguments:
// - <none>
// Return Value:
// - true iff this pane has no child panes.
bool Pane::_IsLeaf() const noexcept
{
    return _splitState == SplitState::None;
}

// Method Description:
// - Returns true if this pane is currently focused, or there is a pane which is
//   a child of this pane that is actively focused
// Arguments:
// - <none>
// Return Value:
// - true if the currently focused pane is either this pane, or one of this
//   pane's descendants
bool Pane::_HasFocusedChild() const noexcept
{
    // We're intentionally making this one giant expression, so the compiler
    // will skip the following lookups if one of the lookups before it returns
    // true
    return (_control && _control.FocusState() != FocusState::Unfocused) ||
           (_firstChild && _firstChild->_HasFocusedChild()) ||
           (_secondChild && _secondChild->_HasFocusedChild());
}

// Method Description:
// - Update the focus state of this pane, and all its descendants.
//   * If this is a leaf node, and our control is actively focused, we'll mark
//     ourselves as the _lastFocused.
//   * If we're not a leaf, we'll recurse on our children to check them.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::UpdateFocus()
{
    if (_IsLeaf())
    {
        const auto controlFocused = _control &&
                                    _control.FocusState() != FocusState::Unfocused;

        _lastFocused = controlFocused;
    }
    else
    {
        _lastFocused = false;
        _firstChild->UpdateFocus();
        _secondChild->UpdateFocus();
    }
}

// Method Description:
// - Focuses this control if we're a leaf, or attempts to focus the first leaf
//   of our first child, recursively.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_FocusFirstChild()
{
    if (_IsLeaf())
    {
        _control.Focus(FocusState::Programmatic);
    }
    else
    {
        _firstChild->_FocusFirstChild();
    }
}

// Method Description:
// - Attempts to update the settings of this pane or any children of this pane.
//   * If this pane is a leaf, and our profile guid matches the parameter, then
//     we'll apply the new settings to our control.
//   * If we're not a leaf, we'll recurse on our children.
// Arguments:
// - settings: The new TerminalSettings to apply to any matching controls
// - profile: The GUID of the profile these settings should apply to.
// Return Value:
// - <none>
void Pane::UpdateSettings(const TerminalSettings& settings, const GUID& profile)
{
    if (!_IsLeaf())
    {
        _firstChild->UpdateSettings(settings, profile);
        _secondChild->UpdateSettings(settings, profile);
    }
    else
    {
        if (profile == _profile)
        {
            _control.UpdateSettings(settings);
        }
    }
}

// Method Description:
// - Closes one of our children. In doing so, takes the control from the other
//   child, and makes this pane a leaf node again.
// Arguments:
// - closeFirst: if true, the first child should be closed, and the second
//   should be preserved, and vice-versa for false.
// Return Value:
// - <none>
void Pane::_CloseChild(const bool closeFirst)
{
    // Lock the create/close lock so that another operation won't concurrently
    // modify our tree
    std::unique_lock lock{ _createCloseLock };

    // If we're a leaf, then chances are both our children closed in close
    // succession. We waited on the lock while the other child was closed, so
    // now we don't have a child to close anymore. Return here. When we moved
    // the non-closed child into us, we also set up event handlers that will be
    // triggered when we return from this.
    if (_IsLeaf())
    {
        return;
    }

    auto closedChild = closeFirst ? _firstChild : _secondChild;
    auto remainingChild = closeFirst ? _secondChild : _firstChild;

    // If the only child left is a leaf, that means we're a leaf now.
    if (remainingChild->_IsLeaf())
    {
        // take the control and profile of the pane that _wasn't_ closed.
        _control = remainingChild->_control;
        _profile = remainingChild->_profile;

        // Add our new event handler before revoking the old one.
        _connectionClosedToken = _control.ConnectionClosed({ this, &Pane::_ControlClosedHandler });

        // Revoke the old event handlers. Remove both the handlers for the panes
        // themselves closing, and remove their handlers for their controls
        // closing. At this point, if the remaining child's control is closed,
        // they'll trigger only our event handler for the control's close.
        _firstChild->Closed(_firstClosedToken);
        _secondChild->Closed(_secondClosedToken);
        closedChild->_control.ConnectionClosed(closedChild->_connectionClosedToken);
        remainingChild->_control.ConnectionClosed(remainingChild->_connectionClosedToken);

        // If either of our children was focused, we want to take that focus from
        // them.
        _lastFocused = _firstChild->_lastFocused || _secondChild->_lastFocused;

        // Remove all the ui elements of our children. This'll make sure we can
        // re-attach the TermControl to our Grid.
        _firstChild->_root.Children().Clear();
        _secondChild->_root.Children().Clear();

        // Reset our UI:
        _root.Children().Clear();
        _root.ColumnDefinitions().Clear();
        _root.RowDefinitions().Clear();
        _separatorRoot = { nullptr };

        // Reattach the TermControl to our grid.
        _root.Children().Append(_control);

        if (_lastFocused)
        {
            _control.Focus(FocusState::Programmatic);
        }

        _splitState = SplitState::None;

        // Release our children.
        _firstChild = nullptr;
        _secondChild = nullptr;
    }
    else
    {
        // First stash away references to the old panes and their tokens
        const auto oldFirstToken = _firstClosedToken;
        const auto oldSecondToken = _secondClosedToken;
        const auto oldFirst = _firstChild;
        const auto oldSecond = _secondChild;

        // Steal all the state from our child
        _splitState = remainingChild->_splitState;
        _separatorRoot = remainingChild->_separatorRoot;
        _firstChild = remainingChild->_firstChild;
        _secondChild = remainingChild->_secondChild;

        // Set up new close handlers on the children
        _SetupChildCloseHandlers();

        // Revoke the old event handlers on our new children
        _firstChild->Closed(remainingChild->_firstClosedToken);
        _secondChild->Closed(remainingChild->_secondClosedToken);

        // Revoke event handlers on old panes and controls
        oldFirst->Closed(oldFirstToken);
        oldSecond->Closed(oldSecondToken);
        closedChild->_control.ConnectionClosed(closedChild->_connectionClosedToken);

        // Reset our UI:
        _root.Children().Clear();
        _root.ColumnDefinitions().Clear();
        _root.RowDefinitions().Clear();

        // Copy the old UI over to our grid.
        // Start by copying the row/column definitions. Iterate over the
        // rows/cols, and remove each one from the old grid, and attach it to
        // our grid instead.
        while (remainingChild->_root.ColumnDefinitions().Size() > 0)
        {
            auto col = remainingChild->_root.ColumnDefinitions().GetAt(0);
            remainingChild->_root.ColumnDefinitions().RemoveAt(0);
            _root.ColumnDefinitions().Append(col);
        }
        while (remainingChild->_root.RowDefinitions().Size() > 0)
        {
            auto row = remainingChild->_root.RowDefinitions().GetAt(0);
            remainingChild->_root.RowDefinitions().RemoveAt(0);
            _root.RowDefinitions().Append(row);
        }

        // Remove the child's UI elements from the child's grid, so we can
        // attach them to us instead.
        remainingChild->_root.Children().Clear();

        _root.Children().Append(_firstChild->GetRootElement());
        _root.Children().Append(_separatorRoot);
        _root.Children().Append(_secondChild->GetRootElement());

        // If the closed child was focused, transfer the focus to it's first sibling.
        if (closedChild->_lastFocused)
        {
            _FocusFirstChild();
        }

        // Release the pointers that the child was holding.
        remainingChild->_firstChild = nullptr;
        remainingChild->_secondChild = nullptr;
        remainingChild->_separatorRoot = { nullptr };
    }
}

// Method Description:
// - Adds event handlers to our children to handle their close events.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_SetupChildCloseHandlers()
{
    _firstClosedToken = _firstChild->Closed([this]() {
        _root.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=]() {
            _CloseChild(true);
        });
    });

    _secondClosedToken = _secondChild->Closed([this]() {
        _root.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=]() {
            _CloseChild(false);
        });
    });
}

// Method Description:
// - Sets up row/column definitions for this pane. There are three total
//   row/cols. The middle one is for the separator. The first and third are for
//   each of the child panes, and are given a size in pixels, based off the
//   availiable space, and the percent of the space they respectively consume,
//   which is stored in _firstPercent and _secondPercent.
// - Does nothing if our split state is currently set to SplitState::None
// Arguments:
// - rootSize: The dimensions in pixels that this pane (and its children should consume.)
// Return Value:
// - <none>
void Pane::_CreateRowColDefinitions(const Size& rootSize)
{
    if (_splitState == SplitState::Vertical)
    {
        _root.ColumnDefinitions().Clear();

        // Create three columns in this grid: one for each pane, and one for the separator.
        auto separatorColDef = Controls::ColumnDefinition();
        separatorColDef.Width(GridLengthHelper::Auto());

        const auto paneSizes = _GetPaneSizes(rootSize.Width);

        auto firstColDef = Controls::ColumnDefinition();
        firstColDef.Width(GridLengthHelper::FromPixels(paneSizes.first));

        auto secondColDef = Controls::ColumnDefinition();
        secondColDef.Width(GridLengthHelper::FromPixels(paneSizes.second));

        _root.ColumnDefinitions().Append(firstColDef);
        _root.ColumnDefinitions().Append(separatorColDef);
        _root.ColumnDefinitions().Append(secondColDef);
    }
    else if (_splitState == SplitState::Horizontal)
    {
        _root.RowDefinitions().Clear();

        // Create three rows in this grid: one for each pane, and one for the separator.
        auto separatorRowDef = Controls::RowDefinition();
        separatorRowDef.Height(GridLengthHelper::Auto());

        const auto paneSizes = _GetPaneSizes(rootSize.Height);

        auto firstRowDef = Controls::RowDefinition();
        firstRowDef.Height(GridLengthHelper::FromPixels(paneSizes.first));

        auto secondRowDef = Controls::RowDefinition();
        secondRowDef.Height(GridLengthHelper::FromPixels(paneSizes.second));

        _root.RowDefinitions().Append(firstRowDef);
        _root.RowDefinitions().Append(separatorRowDef);
        _root.RowDefinitions().Append(secondRowDef);
    }
}

// Method Description:
// - Initializes our UI for a new split in this pane. Sets up row/column
//   definitions, and initializes the separator grid. Does nothing if our split
//   state is currently set to SplitState::None
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_CreateSplitContent()
{
    Size actualSize{ gsl::narrow_cast<float>(_root.ActualWidth()),
                     gsl::narrow_cast<float>(_root.ActualHeight()) };

    _CreateRowColDefinitions(actualSize);

    if (_splitState == SplitState::Vertical)
    {
        // Create the pane separator
        _separatorRoot = Controls::Grid{};
        _separatorRoot.Width(PaneSeparatorSize);
        // NaN is the special value XAML uses for "Auto" sizing.
        _separatorRoot.Height(NAN);
    }
    else if (_splitState == SplitState::Horizontal)
    {
        // Create the pane separator
        _separatorRoot = Controls::Grid{};
        _separatorRoot.Height(PaneSeparatorSize);
        // NaN is the special value XAML uses for "Auto" sizing.
        _separatorRoot.Width(NAN);
    }
}

// Method Description:
// - Sets the row/column of our child UI elements, to match our current split type.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_ApplySplitDefinitions()
{
    if (_splitState == SplitState::Vertical)
    {
        Controls::Grid::SetColumn(_firstChild->GetRootElement(), 0);
        Controls::Grid::SetColumn(_separatorRoot, 1);
        Controls::Grid::SetColumn(_secondChild->GetRootElement(), 2);
    }
    else if (_splitState == SplitState::Horizontal)
    {
        Controls::Grid::SetRow(_firstChild->GetRootElement(), 0);
        Controls::Grid::SetRow(_separatorRoot, 1);
        Controls::Grid::SetRow(_secondChild->GetRootElement(), 2);
    }
}

// Method Description:
// - Vertically split the focused pane in our tree of panes, and place the given
//   TermControl into the newly created pane. If we're the focused pane, then
//   we'll create two new children, and place them side-by-side in our Grid.
// Arguments:
// - profile: The profile GUID to associate with the newly created pane.
// - control: A TermControl to use in the new pane.
// Return Value:
// - <none>
void Pane::SplitVertical(const GUID& profile, const TermControl& control)
{
    // If we're not the leaf, recurse into our children to split them.
    if (!_IsLeaf())
    {
        if (_firstChild->_HasFocusedChild())
        {
            _firstChild->SplitVertical(profile, control);
        }
        else if (_secondChild->_HasFocusedChild())
        {
            _secondChild->SplitVertical(profile, control);
        }

        return;
    }

    _Split(SplitState::Vertical, profile, control);
}

// Method Description:
// - Horizontally split the focused pane in our tree of panes, and place the given
//   TermControl into the newly created pane. If we're the focused pane, then
//   we'll create two new children, and place them side-by-side in our Grid.
// Arguments:
// - profile: The profile GUID to associate with the newly created pane.
// - control: A TermControl to use in the new pane.
// Return Value:
// - <none>
void Pane::SplitHorizontal(const GUID& profile, const TermControl& control)
{
    if (!_IsLeaf())
    {
        if (_firstChild->_HasFocusedChild())
        {
            _firstChild->SplitHorizontal(profile, control);
        }
        else if (_secondChild->_HasFocusedChild())
        {
            _secondChild->SplitHorizontal(profile, control);
        }

        return;
    }

    _Split(SplitState::Horizontal, profile, control);
}

// Method Description:
// - Does the bulk of the work of creating a new split. Initializes our UI,
//   creates a new Pane to host the control, registers event handlers.
// Arguments:
// - splitType: what type of split we should create.
// - profile: The profile GUID to associate with the newly created pane.
// - control: A TermControl to use in the new pane.
// Return Value:
// - <none>
void Pane::_Split(SplitState splitType, const GUID& profile, const TermControl& control)
{
    // Lock the create/close lock so that another operation won't concurrently
    // modify our tree
    std::unique_lock lock{ _createCloseLock };

    // revoke our handler - the child will take care of the control now.
    _control.ConnectionClosed(_connectionClosedToken);
    _connectionClosedToken.value = 0;

    _splitState = splitType;

    _firstPercent = { Half };
    _secondPercent = { Half };

    _CreateSplitContent();

    // Remove any children we currently have. We can't add the existing
    // TermControl to a new grid until we do this.
    _root.Children().Clear();

    // Create two new Panes
    //   Move our control, guid into the first one.
    //   Move the new guid, control into the second.
    _firstChild = std::make_shared<Pane>(_profile.value(), _control);
    _profile = std::nullopt;
    _control = { nullptr };
    _secondChild = std::make_shared<Pane>(profile, control);

    _root.Children().Append(_firstChild->GetRootElement());
    _root.Children().Append(_separatorRoot);
    _root.Children().Append(_secondChild->GetRootElement());

    _ApplySplitDefinitions();

    // Register event handlers on our children to handle their Close events
    _SetupChildCloseHandlers();

    _lastFocused = false;
}

// Method Description:
// - Gets the size in pixels of each of our children, given the full size they
//   should fill. Accounts for the size of the separator that should be between
//   them as well.
// Arguments:
// - fullSize: the amount of space in pixels that should be filled by our
//   children and their separator
// Return Value:
// - a pair with the size of our first child and the size of our second child,
//   respectively.
std::pair<float, float> Pane::_GetPaneSizes(const float& fullSize)
{
    if (_IsLeaf())
    {
        THROW_HR(E_FAIL);
    }

    const auto sizeMinusSeparator = fullSize - PaneSeparatorSize;
    const auto firstSize = sizeMinusSeparator * _firstPercent.value();
    const auto secondSize = sizeMinusSeparator * _secondPercent.value();
    return { firstSize, secondSize };
}

// Method Description:
// - Get the absolute minimum size that this pane can be resized to and still
//   have 1x1 character visible, in each of its children. This includes the
//   space needed for the separator.
// Arguments:
// - <none>
// Return Value:
// - The minimum size that this pane can be resized to and still have a visible
//   character.
Size Pane::_GetMinSize() const
{
    if (_IsLeaf())
    {
        return _control.MinimumSize();
    }
    else
    {
        const auto firstSize = _firstChild->_GetMinSize();
        const auto secondSize = _secondChild->_GetMinSize();
        const auto newWidth = firstSize.Width + secondSize.Width + (_splitState == SplitState::Vertical ? PaneSeparatorSize : 0);
        const auto newHeight = firstSize.Height + secondSize.Height + (_splitState == SplitState::Horizontal ? PaneSeparatorSize : 0);
        return { newWidth, newHeight };
    }
}

DEFINE_EVENT(Pane, Closed, _closedHandlers, ConnectionClosedEventArgs);

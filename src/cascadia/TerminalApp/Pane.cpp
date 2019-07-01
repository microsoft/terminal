// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Pane.h"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;

static const int PaneSeparatorSize = 4;

Pane::Pane(const GUID& profile, const TermControl& control, const bool lastFocused) :
    _control{ control },
    _lastFocused{ lastFocused },
    _profile{ profile }
{
    _root.Children().Append(_control.GetControl());
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
    return (_control && _control.GetControl().FocusState() != FocusState::Unfocused) ||
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
                                    _control.GetControl().FocusState() != FocusState::Unfocused;

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
        _control.GetControl().Focus(FocusState::Programmatic);
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
        _root.Children().Append(_control.GetControl());

        if (_lastFocused)
        {
            _control.GetControl().Focus(FocusState::Programmatic);
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
// - Initializes our UI for a new split in this pane. Sets up row/column
//   definitions, and initializes the separator grid. Does nothing if our split
//   state is currently set to SplitState::None
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_CreateSplitContent()
{
    if (_splitState == SplitState::Vertical)
    {
        // Create three columns in this grid: one for each pane, and one for the separator.
        auto separatorColDef = Controls::ColumnDefinition();
        separatorColDef.Width(GridLengthHelper::Auto());

        _root.ColumnDefinitions().Append(Controls::ColumnDefinition{});
        _root.ColumnDefinitions().Append(separatorColDef);
        _root.ColumnDefinitions().Append(Controls::ColumnDefinition{});

        // Create the pane separator
        _separatorRoot = Controls::Grid{};
        _separatorRoot.Width(PaneSeparatorSize);
        // NaN is the special value XAML uses for "Auto" sizing.
        _separatorRoot.Height(NAN);
    }
    else if (_splitState == SplitState::Horizontal)
    {
        // Create three rows in this grid: one for each pane, and one for the separator.
        auto separatorRowDef = Controls::RowDefinition();
        separatorRowDef.Height(GridLengthHelper::Auto());

        _root.RowDefinitions().Append(Controls::RowDefinition{});
        _root.RowDefinitions().Append(separatorRowDef);
        _root.RowDefinitions().Append(Controls::RowDefinition{});

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

    _DoSplit(SplitState::Vertical, profile, control);
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

    _DoSplit(SplitState::Horizontal, profile, control);
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
void Pane::_DoSplit(SplitState splitType, const GUID& profile, const TermControl& control)
{
    // Lock the create/close lock so that another operation won't concurrently
    // modify our tree
    std::unique_lock lock{ _createCloseLock };

    // revoke our handler - the child will take care of the control now.
    _control.ConnectionClosed(_connectionClosedToken);
    _connectionClosedToken.value = 0;

    _splitState = splitType;

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

DEFINE_EVENT(Pane, Closed, _closedHandlers, ConnectionClosedEventArgs);

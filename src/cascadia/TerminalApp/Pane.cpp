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
    _AddControlToRoot(_control);

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
// - Adds a given Terminal Control to our root Grid. Also registers an event
//   handler to know when that control closed.
// Arguments:
// - control: The new TermControl to use as the content of our root Grid.
// Return Value:
// - <none>
void Pane::_AddControlToRoot(TermControl control)
{
    _root.Children().Append(control.GetControl());

    _connectionClosedToken = control.ConnectionClosed([this]() {
        if (_control.ShouldCloseOnExit())
        {
            // Fire our Closed event to tell our parent that we should be removed.
            _closedHandlers();
        }
    });
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
    auto closedChild = closeFirst ? _firstChild : _secondChild;
    auto remainingChild = closeFirst ? _secondChild : _firstChild;

    // If the only child left is a leaf, that means we're a leaf now.
    if (remainingChild->_IsLeaf())
    {
        // Revoke the old event handlers.
        _firstChild->Closed(_firstClosedToken);
        _secondChild->Closed(_secondClosedToken);
        closedChild->_control.ConnectionClosed(closedChild->_connectionClosedToken);
        remainingChild->_control.ConnectionClosed(remainingChild->_connectionClosedToken);

        // take the control and profile of the pane that _wasn't_ closed.
        _control = closeFirst ? _secondChild->_control : _firstChild->_control;
        _profile = closeFirst ? _secondChild->_profile : _firstChild->_profile;

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
        _AddControlToRoot(_control);

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
        // Revoke the old event handlers.
        _firstChild->Closed(_firstClosedToken);
        _secondChild->Closed(_secondClosedToken);

        // Steal all the state from our child
        _splitState = remainingChild->_splitState;
        _separatorRoot = remainingChild->_separatorRoot;
        _firstChild = remainingChild->_firstChild;
        _secondChild = remainingChild->_secondChild;

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

        // Set up new close handlers on the children, so thay'll notify us
        // instead of their old parent.
        _SetupChildCloseHandlers();

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
// - Adds event handlers to our chilcren to handle their close events.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_SetupChildCloseHandlers()
{
    _firstClosedToken = _firstChild->Closed([this](){
        _root.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=](){
            _CloseChild(true);
        });
    });

    _secondClosedToken = _secondChild->Closed([this](){
        _root.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=](){
            _CloseChild(false);
        });
    });
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

    // revoke our handler - the child will take care of the control now.
    _control.ConnectionClosed(_connectionClosedToken);
    _connectionClosedToken.value = 0;

    _splitState = SplitState::Vertical;

    // Create three columns in this grid: one for each pane, and one for the separator.
    auto separatorColDef = Controls::ColumnDefinition();
    separatorColDef.Width(GridLengthHelper::Auto());

    _root.ColumnDefinitions().Append(Controls::ColumnDefinition{});
    _root.ColumnDefinitions().Append(separatorColDef);
    _root.ColumnDefinitions().Append(Controls::ColumnDefinition{});

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

    // add the first pane to row 0
    _root.Children().Append(_firstChild->GetRootElement());
    Controls::Grid::SetColumn(_firstChild->GetRootElement(), 0);

    // Create the pane separator, and place it in column 1
    _separatorRoot = Controls::Grid{};
    _separatorRoot.Width(PaneSeparatorSize);
    // NaN is the special value XAML uses for "Auto" sizing.
    _separatorRoot.Height(NAN);
    _root.Children().Append(_separatorRoot);
    Controls::Grid::SetColumn(_separatorRoot, 1);

    // add the second pane to column 2
    _root.Children().Append(_secondChild->GetRootElement());
    Controls::Grid::SetColumn(_secondChild->GetRootElement(), 2);

    // Register event handlers on our children to handle their Close events
    _SetupChildCloseHandlers();

    _lastFocused = false;
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

    // revoke our handler - the child will take care of the control now.
    _control.ConnectionClosed(_connectionClosedToken);
    _connectionClosedToken.value = 0;

    _splitState = SplitState::Horizontal;

    // Create three rows in this grid: one for each pane, and one for the separator.
    auto separatorRowDef = Controls::RowDefinition();
    separatorRowDef.Height(GridLengthHelper::Auto());

    _root.RowDefinitions().Append(Controls::RowDefinition{});
    _root.RowDefinitions().Append(separatorRowDef);
    _root.RowDefinitions().Append(Controls::RowDefinition{});

    _root.Children().Clear();

    // Create two new Panes
    //   Move our control, guid into the first one.
    //   Move the new guid, control into the second.
    _firstChild = std::make_shared<Pane>(_profile.value(), _control);
    _profile = std::nullopt;
    _control = { nullptr };
    _secondChild = std::make_shared<Pane>(profile, control);

    // add the first pane to row 0
    _root.Children().Append(_firstChild->GetRootElement());
    Controls::Grid::SetRow(_firstChild->GetRootElement(), 0);

    _separatorRoot = Controls::Grid{};
    _separatorRoot.Height(PaneSeparatorSize);
    // NaN is the special value XAML uses for "Auto" sizing.
    _separatorRoot.Width(NAN);
    _root.Children().Append(_separatorRoot);
    Controls::Grid::SetRow(_separatorRoot, 1);

    // add the second pane to row 1
    _root.Children().Append(_secondChild->GetRootElement());
    Controls::Grid::SetRow(_secondChild->GetRootElement(), 2);

    _SetupChildCloseHandlers();

    _lastFocused = false;
}

DEFINE_EVENT(Pane, Closed, _closedHandlers, ConnectionClosedEventArgs);

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Pane.h"
#include "AppLogic.h"

#include <Mmsystem.h>

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::TerminalApp;
using namespace TerminalApp;

static const int PaneBorderSize = 2;
static const int CombinedPaneBorderSize = 2 * PaneBorderSize;

// WARNING: Don't do this! This won't work
//   Duration duration{ std::chrono::milliseconds{ 200 } };
// Instead, make a duration from a TimeSpan from the time in millis
//
// 200ms was chosen because it's quick enough that it doesn't break your
// flow, but not too quick to see
static const int AnimationDurationInMilliseconds = 200;
static const Duration AnimationDuration = DurationHelper::FromTimeSpan(winrt::Windows::Foundation::TimeSpan(std::chrono::milliseconds(AnimationDurationInMilliseconds)));

winrt::Windows::UI::Xaml::Media::SolidColorBrush Pane::s_focusedBorderBrush = { nullptr };
winrt::Windows::UI::Xaml::Media::SolidColorBrush Pane::s_unfocusedBorderBrush = { nullptr };

Pane::Pane(const GUID& profile, const TermControl& control, const bool lastFocused) :
    _control{ control },
    _lastActive{ lastFocused },
    _profile{ profile }
{
    _root.Children().Append(_border);
    _border.Child(_control);

    _connectionStateChangedToken = _control.ConnectionStateChanged({ this, &Pane::_ControlConnectionStateChangedHandler });
    _warningBellToken = _control.WarningBell({ this, &Pane::_ControlWarningBellHandler });

    // On the first Pane's creation, lookup resources we'll use to theme the
    // Pane, including the brushed to use for the focused/unfocused border
    // color.
    if (s_focusedBorderBrush == nullptr || s_unfocusedBorderBrush == nullptr)
    {
        _SetupResources();
    }

    // Use the unfocused border color as the pane background, so an actual color
    // appears behind panes as we animate them sliding in.
    _root.Background(s_unfocusedBorderBrush);

    // Register an event with the control to have it inform us when it gains focus.
    _gotFocusRevoker = control.GotFocus(winrt::auto_revoke, { this, &Pane::_ControlGotFocusHandler });
    _lostFocusRevoker = control.LostFocus(winrt::auto_revoke, { this, &Pane::_ControlLostFocusHandler });

    // When our border is tapped, make sure to transfer focus to our control.
    // LOAD-BEARING: This will NOT work if the border's BorderBrush is set to
    // Colors::Transparent! The border won't get Tapped events, and they'll fall
    // through to something else.
    _border.Tapped([this](auto&, auto& e) {
        _FocusFirstChild();
        e.Handled(true);
    });
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

    _CreateRowColDefinitions();

    if (_splitState == SplitState::Vertical)
    {
        const auto paneSizes = _CalcChildrenSizes(width);

        const Size firstSize{ paneSizes.first, height };
        const Size secondSize{ paneSizes.second, height };
        _firstChild->ResizeContent(firstSize);
        _secondChild->ResizeContent(secondSize);
    }
    else if (_splitState == SplitState::Horizontal)
    {
        const auto paneSizes = _CalcChildrenSizes(height);

        const Size firstSize{ width, paneSizes.first };
        const Size secondSize{ width, paneSizes.second };
        _firstChild->ResizeContent(firstSize);
        _secondChild->ResizeContent(secondSize);
    }
}

// Method Description:
// - Recalculates and reapplies sizes of all descendant panes.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::Relayout()
{
    ResizeContent(_root.ActualSize());
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
bool Pane::_Resize(const ResizeDirection& direction)
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

    const Size actualSize{ gsl::narrow_cast<float>(_root.ActualWidth()),
                           gsl::narrow_cast<float>(_root.ActualHeight()) };
    // actualDimension is the size in DIPs of this pane in the direction we're
    // resizing.
    const auto actualDimension = changeWidth ? actualSize.Width : actualSize.Height;

    _desiredSplitPosition = _ClampSplitPosition(changeWidth, _desiredSplitPosition - amount, actualDimension);

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
bool Pane::ResizePane(const ResizeDirection& direction)
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
    const bool firstIsFocused = _firstChild->_IsLeaf() && _firstChild->_lastActive;
    const bool secondIsFocused = _secondChild->_IsLeaf() && _secondChild->_lastActive;
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
    if ((!_firstChild->_IsLeaf()) && _firstChild->_HasFocusedChild())
    {
        return _firstChild->ResizePane(direction) || _Resize(direction);
    }

    if ((!_secondChild->_IsLeaf()) && _secondChild->_HasFocusedChild())
    {
        return _secondChild->ResizePane(direction) || _Resize(direction);
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
bool Pane::_NavigateFocus(const FocusDirection& direction)
{
    if (!DirectionMatchesSplit(direction, _splitState))
    {
        return false;
    }

    const bool focusSecond = (direction == FocusDirection::Right) || (direction == FocusDirection::Down);

    const auto newlyFocusedChild = focusSecond ? _secondChild : _firstChild;

    // If the child we want to move focus to is _already_ focused, return false,
    // to try and let our parent figure it out.
    if (newlyFocusedChild->_HasFocusedChild())
    {
        return false;
    }

    // Transfer focus to our child, and update the focus of our tree.
    newlyFocusedChild->_FocusFirstChild();
    UpdateVisuals();

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
bool Pane::NavigateFocus(const FocusDirection& direction)
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
    const bool firstIsFocused = _firstChild->_IsLeaf() && _firstChild->_lastActive;
    const bool secondIsFocused = _secondChild->_IsLeaf() && _secondChild->_lastActive;
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
    if ((!_firstChild->_IsLeaf()) && _firstChild->_HasFocusedChild())
    {
        return _firstChild->NavigateFocus(direction) || _NavigateFocus(direction);
    }

    if ((!_secondChild->_IsLeaf()) && _secondChild->_HasFocusedChild())
    {
        return _secondChild->NavigateFocus(direction) || _NavigateFocus(direction);
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
void Pane::_ControlConnectionStateChangedHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                                 const winrt::Windows::Foundation::IInspectable& /*args*/)
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

    const auto newConnectionState = _control.ConnectionState();

    if (newConnectionState < ConnectionState::Closed)
    {
        // Pane doesn't care if the connection isn't entering a terminal state.
        return;
    }

    const auto settings{ winrt::TerminalApp::implementation::AppLogic::CurrentAppSettings() };
    auto paneProfile = settings.FindProfile(_profile.value());
    if (paneProfile)
    {
        auto mode = paneProfile.CloseOnExit();
        if ((mode == CloseOnExitMode::Always) ||
            (mode == CloseOnExitMode::Graceful && newConnectionState == ConnectionState::Closed))
        {
            Close();
        }
    }
}

// Method Description:
// - Plays a warning note when triggered by the BEL control character,
//   using the sound configured for the "Critical Stop" system event.`
//   This matches the behavior of the Windows Console host.
// - Will also flash the taskbar if the bellStyle setting for this profile
//   has the 'visual' flag set
// Arguments:
// - <unused>
void Pane::_ControlWarningBellHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                      const winrt::Windows::Foundation::IInspectable& /*eventArgs*/)
{
    if (!_IsLeaf())
    {
        return;
    }
    const auto settings{ winrt::TerminalApp::implementation::AppLogic::CurrentAppSettings() };
    auto paneProfile = settings.FindProfile(_profile.value());
    if (paneProfile)
    {
        // We don't want to do anything if nothing is set, so check for that first
        if (static_cast<int>(paneProfile.BellStyle()) != 0)
        {
            if (WI_IsFlagSet(paneProfile.BellStyle(), winrt::Microsoft::Terminal::Settings::Model::BellStyle::Audible))
            {
                // Audible is set, play the sound
                const auto soundAlias = reinterpret_cast<LPCTSTR>(SND_ALIAS_SYSTEMHAND);
                PlaySound(soundAlias, NULL, SND_ALIAS_ID | SND_ASYNC | SND_SENTRY);
            }

            // raise the event with the bool value corresponding to the visual flag
            _PaneRaiseBellHandlers(nullptr, WI_IsFlagSet(paneProfile.BellStyle(), winrt::Microsoft::Terminal::Settings::Model::BellStyle::Visual));
        }
    }
}

// Event Description:
// - Called when our control gains focus. We'll use this to trigger our GotFocus
//   callback. The tab that's hosting us should have registered a callback which
//   can be used to mark us as active.
// Arguments:
// - <unused>
// Return Value:
// - <none>
void Pane::_ControlGotFocusHandler(winrt::Windows::Foundation::IInspectable const& /* sender */,
                                   RoutedEventArgs const& /* args */)
{
    _GotFocusHandlers(shared_from_this());
}

// Event Description:
// - Called when our control loses focus. We'll use this to trigger our LostFocus
//   callback. The tab that's hosting us should have registered a callback which
//   can be used to update its own internal focus state
void Pane::_ControlLostFocusHandler(winrt::Windows::Foundation::IInspectable const& /* sender */,
                                    RoutedEventArgs const& /* args */)
{
    _LostFocusHandlers(shared_from_this());
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
    _ClosedHandlers(nullptr, nullptr);
}

// Method Description:
// - Prepare this pane to be removed from the UI hierarchy by closing all controls
//   and connections beneath it.
void Pane::Shutdown()
{
    // Lock the create/close lock so that another operation won't concurrently
    // modify our tree
    std::unique_lock lock{ _createCloseLock };
    if (_IsLeaf())
    {
        _control.Close();
    }
    else
    {
        _firstChild->Shutdown();
        _secondChild->Shutdown();
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
//   `_lastActive`, else returns this
std::shared_ptr<Pane> Pane::GetActivePane()
{
    if (_IsLeaf())
    {
        return _lastActive ? shared_from_this() : nullptr;
    }

    auto firstFocused = _firstChild->GetActivePane();
    if (firstFocused != nullptr)
    {
        return firstFocused;
    }
    return _secondChild->GetActivePane();
}

// Method Description:
// - Gets the TermControl of this pane. If this Pane is not a leaf, this will return nullptr.
// Arguments:
// - <none>
// Return Value:
// - nullptr if this Pane is a parent, otherwise the TermControl of this Pane.
TermControl Pane::GetTerminalControl()
{
    return _IsLeaf() ? _control : nullptr;
}

// Method Description:
// - Recursively remove the "Active" state from this Pane and all it's children.
// - Updates our visuals to match our new state, including highlighting our borders.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::ClearActive()
{
    _lastActive = false;
    if (!_IsLeaf())
    {
        _firstChild->ClearActive();
        _secondChild->ClearActive();
    }
    UpdateVisuals();
}

// Method Description:
// - Sets the "Active" state on this Pane. Only one Pane in a tree of Panes
//   should be "active", and that pane should be a leaf.
// - Updates our visuals to match our new state, including highlighting our borders.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::SetActive()
{
    _lastActive = true;
    UpdateVisuals();
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
    auto lastFocused = GetActivePane();
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
    return _lastActive;
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
    return (_control && _lastActive) ||
           (_firstChild && _firstChild->_HasFocusedChild()) ||
           (_secondChild && _secondChild->_HasFocusedChild());
}

// Method Description:
// - Update the focus state of this pane. We'll make sure to colorize our
//   borders depending on if we are the active pane or not.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::UpdateVisuals()
{
    _border.BorderBrush(_lastActive ? s_focusedBorderBrush : s_unfocusedBorderBrush);
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
        if (_root.ActualWidth() == 0 && _root.ActualHeight() == 0)
        {
            // When these sizes are 0, then the pane might still be in startup,
            // and doesn't yet have a real size. In that case, the control.Focus
            // event won't be handled until _after_ the startup events are all
            // processed. This will lead to the Tab not being notified that the
            // focus moved to a different Pane.
            //
            // In that scenario, trigger the event manually here, to correctly
            // inform the Tab that we're now focused.
            _GotFocusHandlers(shared_from_this());
        }

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
void Pane::UpdateSettings(const TerminalSettingsCreateResult& settings, const GUID& profile)
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
            auto controlSettings = _control.Settings().as<TerminalSettings>();
            // Update the parent of the control's settings object (and not the object itself) so
            // that any overrides made by the control don't get affected by the reload
            controlSettings.SetParent(settings.DefaultSettings());
            auto unfocusedSettings{ settings.UnfocusedSettings() };
            if (unfocusedSettings)
            {
                // Note: the unfocused settings needs to be entirely unchanged _except_ we need to
                // set its parent to the settings object that lives in the control. This is because
                // the overrides made by the control live in that settings object, so we want to make
                // sure the unfocused settings inherit from that.
                unfocusedSettings.SetParent(controlSettings);
            }
            _control.UnfocusedAppearance(unfocusedSettings);
            _control.UpdateSettings();
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
        // When the remaining child is a leaf, that means both our children were
        // previously leaves, and the only difference in their borders is the
        // border that we gave them. Take a bitwise AND of those two children to
        // remove that border. Other borders the children might have, they
        // inherited from us, so the flag will be set for both children.
        _borders = _firstChild->_borders & _secondChild->_borders;

        // take the control, profile and id of the pane that _wasn't_ closed.
        _control = remainingChild->_control;
        _profile = remainingChild->_profile;
        _id = remainingChild->Id();

        // Add our new event handler before revoking the old one.
        _connectionStateChangedToken = _control.ConnectionStateChanged({ this, &Pane::_ControlConnectionStateChangedHandler });
        _warningBellToken = _control.WarningBell({ this, &Pane::_ControlWarningBellHandler });

        // Revoke the old event handlers. Remove both the handlers for the panes
        // themselves closing, and remove their handlers for their controls
        // closing. At this point, if the remaining child's control is closed,
        // they'll trigger only our event handler for the control's close.
        _firstChild->Closed(_firstClosedToken);
        _secondChild->Closed(_secondClosedToken);
        closedChild->_control.ConnectionStateChanged(closedChild->_connectionStateChangedToken);
        remainingChild->_control.ConnectionStateChanged(remainingChild->_connectionStateChangedToken);
        closedChild->_control.WarningBell(closedChild->_warningBellToken);
        remainingChild->_control.WarningBell(remainingChild->_warningBellToken);

        // If either of our children was focused, we want to take that focus from
        // them.
        _lastActive = _firstChild->_lastActive || _secondChild->_lastActive;

        // Remove all the ui elements of our children. This'll make sure we can
        // re-attach the TermControl to our Grid.
        _firstChild->_root.Children().Clear();
        _secondChild->_root.Children().Clear();
        _firstChild->_border.Child(nullptr);
        _secondChild->_border.Child(nullptr);

        // Reset our UI:
        _root.Children().Clear();
        _border.Child(nullptr);
        _root.ColumnDefinitions().Clear();
        _root.RowDefinitions().Clear();

        // Reattach the TermControl to our grid.
        _root.Children().Append(_border);
        _border.Child(_control);

        // Make sure to set our _splitState before focusing the control. If you
        // fail to do this, when the tab handles the GotFocus event and asks us
        // what our active control is, we won't technically be a "leaf", and
        // GetTerminalControl will return null.
        _splitState = SplitState::None;

        // re-attach our handler for the control's GotFocus event.
        _gotFocusRevoker = _control.GotFocus(winrt::auto_revoke, { this, &Pane::_ControlGotFocusHandler });
        _lostFocusRevoker = _control.LostFocus(winrt::auto_revoke, { this, &Pane::_ControlLostFocusHandler });

        // If we're inheriting the "last active" state from one of our children,
        // focus our control now. This should trigger our own GotFocus event.
        if (_lastActive)
        {
            _control.Focus(FocusState::Programmatic);

            // See GH#7252
            // Manually fire off the GotFocus event. Typically, this is done
            // automatically when the control gets focused. However, if we're
            // `exit`ing a zoomed pane, then the other sibling isn't in the UI
            // tree currently. So the above call to Focus won't actually focus
            // the control. Because Tab is relying on GotFocus to know who the
            // active pane in the tree is, without this call, _no one_ will be
            // the active pane any longer.
            _GotFocusHandlers(shared_from_this());
        }

        _UpdateBorders();

        // Release our children.
        _firstChild = nullptr;
        _secondChild = nullptr;
    }
    else
    {
        // Determine which border flag we gave to the child when we first split
        // it, so that we can take just that flag away from them.
        Borders clearBorderFlag = Borders::None;
        if (_splitState == SplitState::Horizontal)
        {
            clearBorderFlag = closeFirst ? Borders::Top : Borders::Bottom;
        }
        else if (_splitState == SplitState::Vertical)
        {
            clearBorderFlag = closeFirst ? Borders::Left : Borders::Right;
        }

        // First stash away references to the old panes and their tokens
        const auto oldFirstToken = _firstClosedToken;
        const auto oldSecondToken = _secondClosedToken;
        const auto oldFirst = _firstChild;
        const auto oldSecond = _secondChild;

        // Steal all the state from our child
        _splitState = remainingChild->_splitState;
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
        closedChild->_control.ConnectionStateChanged(closedChild->_connectionStateChangedToken);
        closedChild->_control.WarningBell(closedChild->_warningBellToken);

        // Reset our UI:
        _root.Children().Clear();
        _border.Child(nullptr);
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
        remainingChild->_border.Child(nullptr);

        _root.Children().Append(_firstChild->GetRootElement());
        _root.Children().Append(_secondChild->GetRootElement());

        // Take the flag away from the children that they inherited from their
        // parent, and update their borders to visually match
        WI_ClearAllFlags(_firstChild->_borders, clearBorderFlag);
        WI_ClearAllFlags(_secondChild->_borders, clearBorderFlag);
        _UpdateBorders();
        _firstChild->_UpdateBorders();
        _secondChild->_UpdateBorders();

        // If the closed child was focused, transfer the focus to it's first sibling.
        if (closedChild->_lastActive)
        {
            _FocusFirstChild();
        }

        // Release the pointers that the child was holding.
        remainingChild->_firstChild = nullptr;
        remainingChild->_secondChild = nullptr;
    }
}

winrt::fire_and_forget Pane::_CloseChildRoutine(const bool closeFirst)
{
    auto weakThis{ shared_from_this() };

    co_await winrt::resume_foreground(_root.Dispatcher());

    if (auto pane{ weakThis.get() })
    {
        // This will query if animations are enabled via the "Show animations in
        // Windows" setting in the OS
        winrt::Windows::UI::ViewManagement::UISettings uiSettings;
        const auto animationsEnabledInOS = uiSettings.AnimationsEnabled();
        const auto animationsEnabledInApp = Media::Animation::Timeline::AllowDependentAnimations();

        // GH#7252: If either child is zoomed, just skip the animation. It won't work.
        const bool eitherChildZoomed = pane->_firstChild->_zoomed || pane->_secondChild->_zoomed;
        // If animations are disabled, just skip this and go straight to
        // _CloseChild. Curiously, the pane opening animation doesn't need this,
        // and will skip straight to Completed when animations are disabled, but
        // this one doesn't seem to.
        if (!animationsEnabledInOS || !animationsEnabledInApp || eitherChildZoomed)
        {
            pane->_CloseChild(closeFirst);
            co_return;
        }

        // Setup the animation

        auto removedChild = closeFirst ? _firstChild : _secondChild;
        auto remainingChild = closeFirst ? _secondChild : _firstChild;
        const bool splitWidth = _splitState == SplitState::Vertical;
        const auto totalSize = splitWidth ? _root.ActualWidth() : _root.ActualHeight();

        Size removedOriginalSize{
            ::base::saturated_cast<float>(removedChild->_root.ActualWidth()),
            ::base::saturated_cast<float>(removedChild->_root.ActualHeight())
        };
        Size remainingOriginalSize{
            ::base::saturated_cast<float>(remainingChild->_root.ActualWidth()),
            ::base::saturated_cast<float>(remainingChild->_root.ActualHeight())
        };

        // Remove both children from the grid
        _root.Children().Clear();
        // Add the remaining child back to the grid, in the right place.
        _root.Children().Append(remainingChild->GetRootElement());
        if (_splitState == SplitState::Vertical)
        {
            Controls::Grid::SetColumn(remainingChild->GetRootElement(), closeFirst ? 1 : 0);
        }
        else if (_splitState == SplitState::Horizontal)
        {
            Controls::Grid::SetRow(remainingChild->GetRootElement(), closeFirst ? 1 : 0);
        }

        // Create the dummy grid. This grid will be the one we actually animate,
        // in the place of the closed pane.
        Controls::Grid dummyGrid;
        dummyGrid.Background(s_unfocusedBorderBrush);
        // It should be the size of the closed pane.
        dummyGrid.Width(removedOriginalSize.Width);
        dummyGrid.Height(removedOriginalSize.Height);
        // Put it where the removed child is
        if (_splitState == SplitState::Vertical)
        {
            Controls::Grid::SetColumn(dummyGrid, closeFirst ? 0 : 1);
        }
        else if (_splitState == SplitState::Horizontal)
        {
            Controls::Grid::SetRow(dummyGrid, closeFirst ? 0 : 1);
        }
        // Add it to the tree
        _root.Children().Append(dummyGrid);

        // Set up the rows/cols as auto/auto, so they'll only use the size of
        // the elements in the grid.
        //
        // * For the closed pane, we want to make that row/col "auto" sized, so
        //   it takes up as much space as is available.
        // * For the remaining pane, we'll make that row/col "*" sized, so it
        //   takes all the remaining space. As the dummy grid is resized down,
        //   the remaining pane will expand to take the rest of the space.
        _root.ColumnDefinitions().Clear();
        _root.RowDefinitions().Clear();
        if (_splitState == SplitState::Vertical)
        {
            auto firstColDef = Controls::ColumnDefinition();
            auto secondColDef = Controls::ColumnDefinition();
            firstColDef.Width(!closeFirst ? GridLengthHelper::FromValueAndType(1, GridUnitType::Star) : GridLengthHelper::Auto());
            secondColDef.Width(closeFirst ? GridLengthHelper::FromValueAndType(1, GridUnitType::Star) : GridLengthHelper::Auto());
            _root.ColumnDefinitions().Append(firstColDef);
            _root.ColumnDefinitions().Append(secondColDef);
        }
        else if (_splitState == SplitState::Horizontal)
        {
            auto firstRowDef = Controls::RowDefinition();
            auto secondRowDef = Controls::RowDefinition();
            firstRowDef.Height(!closeFirst ? GridLengthHelper::FromValueAndType(1, GridUnitType::Star) : GridLengthHelper::Auto());
            secondRowDef.Height(closeFirst ? GridLengthHelper::FromValueAndType(1, GridUnitType::Star) : GridLengthHelper::Auto());
            _root.RowDefinitions().Append(firstRowDef);
            _root.RowDefinitions().Append(secondRowDef);
        }

        // Animate the dummy grid from its current size down to 0
        Media::Animation::DoubleAnimation animation{};
        animation.Duration(AnimationDuration);
        animation.From(splitWidth ? removedOriginalSize.Width : removedOriginalSize.Height);
        animation.To(0.0);
        // This easing is the same as the entrance animation.
        animation.EasingFunction(Media::Animation::QuadraticEase{});
        animation.EnableDependentAnimation(true);

        Media::Animation::Storyboard s;
        s.Duration(AnimationDuration);
        s.Children().Append(animation);
        s.SetTarget(animation, dummyGrid);
        s.SetTargetProperty(animation, splitWidth ? L"Width" : L"Height");

        // Start the animation.
        s.Begin();

        std::weak_ptr<Pane> weakThis{ shared_from_this() };

        // When the animation is completed, reparent the child's content up to
        // us, and remove the child nodes from the tree.
        animation.Completed([weakThis, closeFirst](auto&&, auto&&) {
            if (auto pane{ weakThis.lock() })
            {
                // We don't need to manually undo any of the above trickiness.
                // We're going to re-parent the child's content into us anyways
                pane->_CloseChild(closeFirst);
            }
        });
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
    _firstClosedToken = _firstChild->Closed([this](auto&& /*s*/, auto&& /*e*/) {
        _CloseChildRoutine(true);
    });

    _secondClosedToken = _secondChild->Closed([this](auto&& /*s*/, auto&& /*e*/) {
        _CloseChildRoutine(false);
    });
}

// Method Description:
// - Sets up row/column definitions for this pane. There are three total
//   row/cols. The middle one is for the separator. The first and third are for
//   each of the child panes, and are given a size in pixels, based off the
//   available space, and the percent of the space they respectively consume,
//   which is stored in _desiredSplitPosition
// - Does nothing if our split state is currently set to SplitState::None
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_CreateRowColDefinitions()
{
    const auto first = _desiredSplitPosition * 100.0f;
    const auto second = 100.0f - first;
    if (_splitState == SplitState::Vertical)
    {
        _root.ColumnDefinitions().Clear();

        // Create two columns in this grid: one for each pane

        auto firstColDef = Controls::ColumnDefinition();
        firstColDef.Width(GridLengthHelper::FromValueAndType(first, GridUnitType::Star));

        auto secondColDef = Controls::ColumnDefinition();
        secondColDef.Width(GridLengthHelper::FromValueAndType(second, GridUnitType::Star));

        _root.ColumnDefinitions().Append(firstColDef);
        _root.ColumnDefinitions().Append(secondColDef);
    }
    else if (_splitState == SplitState::Horizontal)
    {
        _root.RowDefinitions().Clear();

        // Create two rows in this grid: one for each pane

        auto firstRowDef = Controls::RowDefinition();
        firstRowDef.Height(GridLengthHelper::FromValueAndType(first, GridUnitType::Star));

        auto secondRowDef = Controls::RowDefinition();
        secondRowDef.Height(GridLengthHelper::FromValueAndType(second, GridUnitType::Star));

        _root.RowDefinitions().Append(firstRowDef);
        _root.RowDefinitions().Append(secondRowDef);
    }
}

// Method Description:
// - Sets the thickness of each side of our borders to match our _borders state.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_UpdateBorders()
{
    double top = 0, bottom = 0, left = 0, right = 0;

    Thickness newBorders{ 0 };
    if (_zoomed)
    {
        // When the pane is zoomed, manually show all the borders around the window.
        top = bottom = right = left = PaneBorderSize;
    }
    else
    {
        if (WI_IsFlagSet(_borders, Borders::Top))
        {
            top = PaneBorderSize;
        }
        if (WI_IsFlagSet(_borders, Borders::Bottom))
        {
            bottom = PaneBorderSize;
        }
        if (WI_IsFlagSet(_borders, Borders::Left))
        {
            left = PaneBorderSize;
        }
        if (WI_IsFlagSet(_borders, Borders::Right))
        {
            right = PaneBorderSize;
        }
    }
    _border.BorderThickness(ThicknessHelper::FromLengths(left, top, right, bottom));
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
        Controls::Grid::SetColumn(_secondChild->GetRootElement(), 1);

        _firstChild->_borders = _borders | Borders::Right;
        _secondChild->_borders = _borders | Borders::Left;
        _borders = Borders::None;

        _UpdateBorders();
        _firstChild->_UpdateBorders();
        _secondChild->_UpdateBorders();
    }
    else if (_splitState == SplitState::Horizontal)
    {
        Controls::Grid::SetRow(_firstChild->GetRootElement(), 0);
        Controls::Grid::SetRow(_secondChild->GetRootElement(), 1);

        _firstChild->_borders = _borders | Borders::Bottom;
        _secondChild->_borders = _borders | Borders::Top;
        _borders = Borders::None;

        _UpdateBorders();
        _firstChild->_UpdateBorders();
        _secondChild->_UpdateBorders();
    }
}

// Method Description:
// - Create a pair of animations when a new control enters this pane. This
//   should _ONLY_ be called in _Split, AFTER the first and second child panes
//   have been set up.
void Pane::_SetupEntranceAnimation()
{
    // This will query if animations are enabled via the "Show animations in
    // Windows" setting in the OS
    winrt::Windows::UI::ViewManagement::UISettings uiSettings;
    const auto animationsEnabledInOS = uiSettings.AnimationsEnabled();
    const auto animationsEnabledInApp = Media::Animation::Timeline::AllowDependentAnimations();

    const bool splitWidth = _splitState == SplitState::Vertical;
    const auto totalSize = splitWidth ? _root.ActualWidth() : _root.ActualHeight();
    // If we don't have a size yet, it's likely that we're in startup, or we're
    // being executed as a sequence of actions. In that case, just skip the
    // animation.
    if (totalSize <= 0 || !animationsEnabledInOS || !animationsEnabledInApp)
    {
        return;
    }

    const auto [firstSize, secondSize] = _CalcChildrenSizes(::base::saturated_cast<float>(totalSize));

    // This is safe to capture this, because it's only being called in the
    // context of this method (not on another thread)
    auto setupAnimation = [&](const auto& size, const bool isFirstChild) {
        auto child = isFirstChild ? _firstChild : _secondChild;
        auto childGrid = child->_root;
        auto control = child->_control;
        // Build up our animation:
        // * it'll take as long as our duration (200ms)
        // * it'll change the value of our property from 0 to secondSize
        // * it'll animate that value using a quadratic function (like f(t) = t^2)
        // * IMPORTANT! We'll manually tell the animation that "yes we know what
        //   we're doing, we want an animation here."
        Media::Animation::DoubleAnimation animation{};
        animation.Duration(AnimationDuration);
        if (isFirstChild)
        {
            // If we're animating the first pane, the size should decrease, from
            // the full size down to the given size.
            animation.From(totalSize);
            animation.To(size);
        }
        else
        {
            // Otherwise, we want to show the pane getting larger, so animate
            // from 0 to the requested size.
            animation.From(0.0);
            animation.To(size);
        }
        animation.EasingFunction(Media::Animation::QuadraticEase{});
        animation.EnableDependentAnimation(true);

        // Now we're going to set up the Storyboard. This is a unit that uses the
        // Animation from above, and actually applies it to a property.
        // * we'll set it up for the same duration as the animation we have
        // * Apply the animation to the grid of the new pane we're adding to the tree.
        // * apply the animation to the Width or Height property.
        Media::Animation::Storyboard s;
        s.Duration(AnimationDuration);
        s.Children().Append(animation);
        s.SetTarget(animation, childGrid);
        s.SetTargetProperty(animation, splitWidth ? L"Width" : L"Height");

        // BE TRICKY:
        // We're animating the width or height of our child pane's grid.
        //
        // We DON'T want to change the size of the control itself, because the
        // terminal has to reflow the buffer every time the control changes size. So
        // what we're going to do there is manually set the control's size to how
        // big we _actually know_ the control will be.
        //
        // We're also going to be changing alignment of our child pane and the
        // control. This way, we'll be able to have the control stick to the inside
        // of the child pane's grid (the side that's moving), while we also have the
        // pane's grid stick to "outside" of the grid (the side that's not moving)
        if (splitWidth)
        {
            // If we're animating the first child, then stick to the top/left of
            // the parent pane, otherwise use the bottom/right. This is always
            // the "outside" of the parent pane.
            childGrid.HorizontalAlignment(isFirstChild ? HorizontalAlignment::Left : HorizontalAlignment::Right);
            control.HorizontalAlignment(HorizontalAlignment::Left);
            control.Width(isFirstChild ? totalSize : size);

            // When the animation is completed, undo the trickiness from before, to
            // restore the controls to the behavior they'd usually have.
            animation.Completed([childGrid, control](auto&&, auto&&) {
                control.Width(NAN);
                childGrid.Width(NAN);
                childGrid.HorizontalAlignment(HorizontalAlignment::Stretch);
                control.HorizontalAlignment(HorizontalAlignment::Stretch);
            });
        }
        else
        {
            // If we're animating the first child, then stick to the top/left of
            // the parent pane, otherwise use the bottom/right. This is always
            // the "outside" of the parent pane.
            childGrid.VerticalAlignment(isFirstChild ? VerticalAlignment::Top : VerticalAlignment::Bottom);
            control.VerticalAlignment(VerticalAlignment::Top);
            control.Height(isFirstChild ? totalSize : size);

            // When the animation is completed, undo the trickiness from before, to
            // restore the controls to the behavior they'd usually have.
            animation.Completed([childGrid, control](auto&&, auto&&) {
                control.Height(NAN);
                childGrid.Height(NAN);
                childGrid.VerticalAlignment(VerticalAlignment::Stretch);
                control.VerticalAlignment(VerticalAlignment::Stretch);
            });
        }

        // Start the animation.
        s.Begin();
    };

    // TODO: GH#7365 - animating the first child right now doesn't _really_ do
    // anything. We could do better though.
    setupAnimation(firstSize, true);
    setupAnimation(secondSize, false);
}

// Method Description:
// - This is a helper to determine if a given Pane can be split, but without
//   using the ActualWidth() and ActualHeight() methods. This is used during
//   processing of many "split-pane" commands, which could happen _before_ we've
//   laid out a Pane for the first time. When this happens, the Pane's don't
//   have an actual size yet. However, we'd still like to figure out if the pane
//   could be split, once they're all laid out.
// - This method assumes that the Pane we're attempting to split is `target`,
//   and this method should be called on the root of a tree of Panes.
// - We'll walk down the tree attempting to find `target`. As we traverse the
//   tree, we'll reduce the size passed to each subsequent recursive call. The
//   size passed to this method represents how much space this Pane _will_ have
//   to use.
//   * If this pane is a leaf, and it's the pane we're looking for, use the
//     available space to calculate which direction to split in.
//   * If this pane is _any other leaf_, then just return nullopt, to indicate
//     that the `target` Pane is not down this branch.
//   * If this pane is a parent, calculate how much space our children will be
//     able to use, and recurse into them.
// Arguments:
// - target: The Pane we're attempting to split.
// - splitType: The direction we're attempting to split in.
// - availableSpace: The theoretical space that's available for this pane to be able to split.
// Return Value:
// - nullopt if `target` is not this pane or a child of this pane, otherwise
//   true iff we could split this pane, given `availableSpace`
// Note:
// - This method is highly similar to Pane::PreCalculateAutoSplit
std::optional<bool> Pane::PreCalculateCanSplit(const std::shared_ptr<Pane> target,
                                               SplitState splitType,
                                               const float splitSize,
                                               const winrt::Windows::Foundation::Size availableSpace) const
{
    if (_IsLeaf())
    {
        if (target.get() == this)
        {
            const auto firstPrecent = 1.0f - splitSize;
            const auto secondPercent = splitSize;
            // If this pane is a leaf, and it's the pane we're looking for, use
            // the available space to calculate which direction to split in.
            const Size minSize = _GetMinSize();

            if (splitType == SplitState::None)
            {
                return { false };
            }

            else if (splitType == SplitState::Vertical)
            {
                const auto widthMinusSeparator = availableSpace.Width - CombinedPaneBorderSize;
                const auto newFirstWidth = widthMinusSeparator * firstPrecent;
                const auto newSecondWidth = widthMinusSeparator * secondPercent;

                return { newFirstWidth > minSize.Width && newSecondWidth > minSize.Width };
            }

            else if (splitType == SplitState::Horizontal)
            {
                const auto heightMinusSeparator = availableSpace.Height - CombinedPaneBorderSize;
                const auto newFirstHeight = heightMinusSeparator * firstPrecent;
                const auto newSecondHeight = heightMinusSeparator * secondPercent;

                return { newFirstHeight > minSize.Height && newSecondHeight > minSize.Height };
            }
        }
        else
        {
            // If this pane is _any other leaf_, then just return nullopt, to
            // indicate that the `target` Pane is not down this branch.
            return std::nullopt;
        }
    }
    else
    {
        // If this pane is a parent, calculate how much space our children will
        // be able to use, and recurse into them.

        const bool isVerticalSplit = _splitState == SplitState::Vertical;
        const float firstWidth = isVerticalSplit ?
                                     (availableSpace.Width * _desiredSplitPosition) - PaneBorderSize :
                                     availableSpace.Width;
        const float secondWidth = isVerticalSplit ?
                                      (availableSpace.Width - firstWidth) - PaneBorderSize :
                                      availableSpace.Width;
        const float firstHeight = !isVerticalSplit ?
                                      (availableSpace.Height * _desiredSplitPosition) - PaneBorderSize :
                                      availableSpace.Height;
        const float secondHeight = !isVerticalSplit ?
                                       (availableSpace.Height - firstHeight) - PaneBorderSize :
                                       availableSpace.Height;

        const auto firstResult = _firstChild->PreCalculateCanSplit(target, splitType, splitSize, { firstWidth, firstHeight });
        return firstResult.has_value() ? firstResult : _secondChild->PreCalculateCanSplit(target, splitType, splitSize, { secondWidth, secondHeight });
    }

    // We should not possibly be getting here - both the above branches should
    // return a value.
    FAIL_FAST();
}

// Method Description:
// - Split the focused pane in our tree of panes, and place the given
//   TermControl into the newly created pane. If we're the focused pane, then
//   we'll create two new children, and place them side-by-side in our Grid.
// Arguments:
// - splitType: what type of split we want to create.
// - profile: The profile GUID to associate with the newly created pane.
// - control: A TermControl to use in the new pane.
// Return Value:
// - The two newly created Panes
std::pair<std::shared_ptr<Pane>, std::shared_ptr<Pane>> Pane::Split(SplitState splitType,
                                                                    const float splitSize,
                                                                    const GUID& profile,
                                                                    const TermControl& control)
{
    if (!_IsLeaf())
    {
        if (_firstChild->_HasFocusedChild())
        {
            return _firstChild->Split(splitType, splitSize, profile, control);
        }
        else if (_secondChild->_HasFocusedChild())
        {
            return _secondChild->Split(splitType, splitSize, profile, control);
        }

        return { nullptr, nullptr };
    }

    return _Split(splitType, splitSize, profile, control);
}

// Method Description:
// - Converts an "automatic" split type into either Vertical or Horizontal,
//   based upon the current dimensions of the Pane.
// - If any of the other SplitState values are passed in, they're returned
//   unmodified.
// Arguments:
// - splitType: The SplitState to attempt to convert
// Return Value:
// - None if splitType was None, otherwise one of Horizontal or Vertical
SplitState Pane::_convertAutomaticSplitState(const SplitState& splitType) const
{
    // Careful here! If the pane doesn't yet have a size, these dimensions will
    // be 0, and we'll always return Vertical.

    if (splitType == SplitState::Automatic)
    {
        // If the requested split type was "auto", determine which direction to
        // split based on our current dimensions
        const Size actualSize{ gsl::narrow_cast<float>(_root.ActualWidth()),
                               gsl::narrow_cast<float>(_root.ActualHeight()) };
        return actualSize.Width >= actualSize.Height ? SplitState::Vertical : SplitState::Horizontal;
    }
    return splitType;
}

// Method Description:
// - Does the bulk of the work of creating a new split. Initializes our UI,
//   creates a new Pane to host the control, registers event handlers.
// Arguments:
// - splitType: what type of split we should create.
// - profile: The profile GUID to associate with the newly created pane.
// - control: A TermControl to use in the new pane.
// Return Value:
// - The two newly created Panes
std::pair<std::shared_ptr<Pane>, std::shared_ptr<Pane>> Pane::_Split(SplitState splitType,
                                                                     const float splitSize,
                                                                     const GUID& profile,
                                                                     const TermControl& control)
{
    if (splitType == SplitState::None)
    {
        return { nullptr, nullptr };
    }

    auto actualSplitType = _convertAutomaticSplitState(splitType);

    // Lock the create/close lock so that another operation won't concurrently
    // modify our tree
    std::unique_lock lock{ _createCloseLock };

    // revoke our handler - the child will take care of the control now.
    _control.ConnectionStateChanged(_connectionStateChangedToken);
    _connectionStateChangedToken.value = 0;
    _control.WarningBell(_warningBellToken);
    _warningBellToken.value = 0;

    // Remove our old GotFocus handler from the control. We don't what the
    // control telling us that it's now focused, we want it telling its new
    // parent.
    _gotFocusRevoker.revoke();
    _lostFocusRevoker.revoke();

    _splitState = actualSplitType;
    _desiredSplitPosition = 1.0f - splitSize;

    // Remove any children we currently have. We can't add the existing
    // TermControl to a new grid until we do this.
    _root.Children().Clear();
    _border.Child(nullptr);

    // Create two new Panes
    //   Move our control, guid into the first one.
    //   Move the new guid, control into the second.
    _firstChild = std::make_shared<Pane>(_profile.value(), _control);
    _profile = std::nullopt;
    _control = { nullptr };
    _secondChild = std::make_shared<Pane>(profile, control);

    _CreateRowColDefinitions();

    _root.Children().Append(_firstChild->GetRootElement());
    _root.Children().Append(_secondChild->GetRootElement());

    _ApplySplitDefinitions();

    // Register event handlers on our children to handle their Close events
    _SetupChildCloseHandlers();

    _lastActive = false;

    _SetupEntranceAnimation();

    // Clear out our ID, only leaves should have IDs
    _id = {};

    return { _firstChild, _secondChild };
}

// Method Description:
// - Recursively attempt to "zoom" the given pane. When the pane is zoomed, it
//   won't be displayed as part of the tab tree, instead it'll take up the full
//   content of the tab. When we find the given pane, we'll need to remove it
//   from the UI tree, so that the caller can re-add it. We'll also set some
//   internal state, so the pane can display all of its borders.
// Arguments:
// - zoomedPane: This is the pane which we're attempting to zoom on.
// Return Value:
// - <none>
void Pane::Maximize(std::shared_ptr<Pane> zoomedPane)
{
    if (_IsLeaf())
    {
        _zoomed = (zoomedPane == shared_from_this());
        _UpdateBorders();
    }
    else
    {
        if (zoomedPane == _firstChild || zoomedPane == _secondChild)
        {
            // When we're zooming the pane, we'll need to remove it from our UI
            // tree. Easy way: just remove both children. We'll re-attach both
            // when we un-zoom.
            _root.Children().Clear();
        }

        // Always recurse into both children. If the (un)zoomed pane was one of
        // our direct children, we'll still want to update it's borders.
        _firstChild->Maximize(zoomedPane);
        _secondChild->Maximize(zoomedPane);
    }
}

// Method Description:
// - Recursively attempt to "un-zoom" the given pane. This does the opposite of
//   Pane::Maximize. When we find the given pane, we should return the pane to our
//   UI tree. We'll also clear the internal state, so the pane can display its
//   borders correctly.
// - The caller should make sure to have removed the zoomed pane from the UI
//   tree _before_ calling this.
// Arguments:
// - zoomedPane: This is the pane which we're attempting to un-zoom.
// Return Value:
// - <none>
void Pane::Restore(std::shared_ptr<Pane> zoomedPane)
{
    if (_IsLeaf())
    {
        _zoomed = false;
        _UpdateBorders();
    }
    else
    {
        if (zoomedPane == _firstChild || zoomedPane == _secondChild)
        {
            // When we're un-zooming the pane, we'll need to re-add it to our UI
            // tree where it originally belonged. easy way: just re-add both.
            _root.Children().Clear();
            _root.Children().Append(_firstChild->GetRootElement());
            _root.Children().Append(_secondChild->GetRootElement());
        }

        // Always recurse into both children. If the (un)zoomed pane was one of
        // our direct children, we'll still want to update it's borders.
        _firstChild->Restore(zoomedPane);
        _secondChild->Restore(zoomedPane);
    }
}

// Method Description:
// - Retrieves the ID of this pane
// - NOTE: The caller should make sure that this pane is a leaf,
//   otherwise the ID value will not make sense (leaves have IDs, parents do not)
// Return Value:
// - The ID of this pane
std::optional<uint16_t> Pane::Id() noexcept
{
    return _id;
}

// Method Description:
// - Sets this pane's ID
// - Panes are given IDs upon creation by TerminalTab
// Arguments:
// - The number to set this pane's ID to
void Pane::Id(uint16_t id) noexcept
{
    _id = id;
}

// Method Description:
// - Recursive function that focuses a pane with the given ID
// Arguments:
// - The ID of the pane we want to focus
void Pane::FocusPane(const uint16_t id)
{
    if (_IsLeaf() && id == _id)
    {
        _control.Focus(FocusState::Programmatic);
    }
    else
    {
        if (_firstChild && _secondChild)
        {
            _firstChild->FocusPane(id);
            _secondChild->FocusPane(id);
        }
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
//   children and their separators. Can be arbitrarily low.
// Return Value:
// - a pair with the size of our first child and the size of our second child,
//   respectively.
std::pair<float, float> Pane::_CalcChildrenSizes(const float fullSize) const
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
//   non-decreasing (it's a monotonically increasing function). This is important so that
//   user doesn't get any pane shrank when they actually expand the window or parent pane.
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
Pane::SnapChildrenSizeResult Pane::_CalcSnappedChildrenSizes(const bool widthOrHeight, const float fullSize) const
{
    if (_IsLeaf())
    {
        THROW_HR(E_FAIL);
    }

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
// - Adjusts given dimension (width or height) so that all descendant terminals
//   align with their character grids as close as possible. Also makes sure to
//   fit in minimal sizes of the panes.
// Arguments:
// - widthOrHeight: if true operates on width, otherwise on height
// - dimension: a dimension (width or height) to be snapped
// Return Value:
// - pair of floats, where first value is the size snapped downward (not greater then
//   requested size) and second is the size snapped upward (not lower than requested size).
//   If requested size is already snapped, then both returned values equal this value.
Pane::SnapSizeResult Pane::_CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
{
    if (_IsLeaf())
    {
        // If we're a leaf pane, align to the grid of controlling terminal

        const auto minSize = _GetMinSize();
        const auto minDimension = widthOrHeight ? minSize.Width : minSize.Height;

        if (dimension <= minDimension)
        {
            return { minDimension, minDimension };
        }

        float lower = _control.SnapDimensionToGrid(widthOrHeight, dimension);
        if (widthOrHeight)
        {
            lower += WI_IsFlagSet(_borders, Borders::Left) ? PaneBorderSize : 0;
            lower += WI_IsFlagSet(_borders, Borders::Right) ? PaneBorderSize : 0;
        }
        else
        {
            lower += WI_IsFlagSet(_borders, Borders::Top) ? PaneBorderSize : 0;
            lower += WI_IsFlagSet(_borders, Borders::Bottom) ? PaneBorderSize : 0;
        }

        if (lower == dimension)
        {
            // If we happen to be already snapped, then just return this size
            // as both lower and higher values.
            return { lower, lower };
        }
        else
        {
            const auto cellSize = _control.CharacterDimensions();
            const auto higher = lower + (widthOrHeight ? cellSize.Width : cellSize.Height);
            return { lower, higher };
        }
    }
    else if (_splitState == (widthOrHeight ? SplitState::Horizontal : SplitState::Vertical))
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

// Method Description:
// - Increases size of given LayoutSizeNode to match next possible 'snap'. In case of leaf
//   pane this means the next cell of the terminal. Otherwise it means that one of its children
//   advances (recursively). It expects the given node and its descendants to have either
//   already snapped or minimum size.
// Arguments:
// - widthOrHeight: if true operates on width, otherwise on height.
// - sizeNode: a layout size node that corresponds to this pane.
// Return Value:
// - <none>
void Pane::_AdvanceSnappedDimension(const bool widthOrHeight, LayoutSizeNode& sizeNode) const
{
    if (_IsLeaf())
    {
        // We're a leaf pane, so just add one more row or column (unless isMinimumSize
        // is true, see below).

        if (sizeNode.isMinimumSize)
        {
            // If the node is of its minimum size, this size might not be snapped (it might
            // be, say, half a character, or fixed 10 pixels), so snap it upward. It might
            // however be already snapped, so add 1 to make sure it really increases
            // (not strictly necessary but to avoid surprises).
            sizeNode.size = _CalcSnappedDimension(widthOrHeight, sizeNode.size + 1).higher;
        }
        else
        {
            const auto cellSize = _control.CharacterDimensions();
            sizeNode.size += widthOrHeight ? cellSize.Width : cellSize.Height;
        }
    }
    else
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
    }

    // Because we have grown, we're certainly no longer of our
    // minimal size (if we've ever been).
    sizeNode.isMinimumSize = false;
}

// Method Description:
// - Get the absolute minimum size that this pane can be resized to and still
//   have 1x1 character visible, in each of its children. If we're a leaf, we'll
//   include the space needed for borders _within_ us.
// Arguments:
// - <none>
// Return Value:
// - The minimum size that this pane can be resized to and still have a visible
//   character.
Size Pane::_GetMinSize() const
{
    if (_IsLeaf())
    {
        auto controlSize = _control.MinimumSize();
        auto newWidth = controlSize.Width;
        auto newHeight = controlSize.Height;

        newWidth += WI_IsFlagSet(_borders, Borders::Left) ? PaneBorderSize : 0;
        newWidth += WI_IsFlagSet(_borders, Borders::Right) ? PaneBorderSize : 0;
        newHeight += WI_IsFlagSet(_borders, Borders::Top) ? PaneBorderSize : 0;
        newHeight += WI_IsFlagSet(_borders, Borders::Bottom) ? PaneBorderSize : 0;

        return { newWidth, newHeight };
    }
    else
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
    LayoutSizeNode node(widthOrHeight ? size.Width : size.Height);
    if (!_IsLeaf())
    {
        node.firstChild = std::make_unique<LayoutSizeNode>(_firstChild->_CreateMinSizeTree(widthOrHeight));
        node.secondChild = std::make_unique<LayoutSizeNode>(_secondChild->_CreateMinSizeTree(widthOrHeight));
    }

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
float Pane::_ClampSplitPosition(const bool widthOrHeight, const float requestedValue, const float totalSize) const
{
    const auto firstMinSize = _firstChild->_GetMinSize();
    const auto secondMinSize = _secondChild->_GetMinSize();

    const auto firstMinDimension = widthOrHeight ? firstMinSize.Width : firstMinSize.Height;
    const auto secondMinDimension = widthOrHeight ? secondMinSize.Width : secondMinSize.Height;

    const auto minSplitPosition = firstMinDimension / totalSize;
    const auto maxSplitPosition = 1.0f - (secondMinDimension / totalSize);

    return std::clamp(requestedValue, minSplitPosition, maxSplitPosition);
}

// Function Description:
// - Attempts to load some XAML resources that the Pane will need. This includes:
//   * The Color we'll use for active Panes's borders - SystemAccentColor
//   * The Brush we'll use for inactive Panes - TabViewBackground (to match the
//     color of the titlebar)
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_SetupResources()
{
    const auto res = Application::Current().Resources();
    const auto accentColorKey = winrt::box_value(L"SystemAccentColor");
    if (res.HasKey(accentColorKey))
    {
        const auto colorFromResources = res.Lookup(accentColorKey);
        // If SystemAccentColor is _not_ a Color for some reason, use
        // Transparent as the color, so we don't do this process again on
        // the next pane (by leaving s_focusedBorderBrush nullptr)
        auto actualColor = winrt::unbox_value_or<Color>(colorFromResources, Colors::Black());
        s_focusedBorderBrush = SolidColorBrush(actualColor);
    }
    else
    {
        // DON'T use Transparent here - if it's "Transparent", then it won't
        // be able to hittest for clicks, and then clicking on the border
        // will eat focus.
        s_focusedBorderBrush = SolidColorBrush{ Colors::Black() };
    }

    const auto tabViewBackgroundKey = winrt::box_value(L"TabViewBackground");
    if (res.HasKey(tabViewBackgroundKey))
    {
        winrt::Windows::Foundation::IInspectable obj = res.Lookup(tabViewBackgroundKey);
        s_unfocusedBorderBrush = obj.try_as<winrt::Windows::UI::Xaml::Media::SolidColorBrush>();
    }
    else
    {
        // DON'T use Transparent here - if it's "Transparent", then it won't
        // be able to hittest for clicks, and then clicking on the border
        // will eat focus.
        s_unfocusedBorderBrush = SolidColorBrush{ Colors::Black() };
    }
}

int Pane::GetLeafPaneCount() const noexcept
{
    return _IsLeaf() ? 1 : (_firstChild->GetLeafPaneCount() + _secondChild->GetLeafPaneCount());
}

// Method Description:
// - This is a helper to determine which direction an "Automatic" split should
//   happen in for a given pane, but without using the ActualWidth() and
//   ActualHeight() methods. This is used during the initialization of the
//   Terminal, when we could be processing many "split-pane" commands _before_
//   we've ever laid out the Terminal for the first time. When this happens, the
//   Pane's don't have an actual size yet. However, we'd still like to figure
//   out how to do an "auto" split when these Panes are all laid out.
// - This method assumes that the Pane we're attempting to split is `target`,
//   and this method should be called on the root of a tree of Panes.
// - We'll walk down the tree attempting to find `target`. As we traverse the
//   tree, we'll reduce the size passed to each subsequent recursive call. The
//   size passed to this method represents how much space this Pane _will_ have
//   to use.
//   * If this pane is a leaf, and it's the pane we're looking for, use the
//     available space to calculate which direction to split in.
//   * If this pane is _any other leaf_, then just return nullopt, to indicate
//     that the `target` Pane is not down this branch.
//   * If this pane is a parent, calculate how much space our children will be
//     able to use, and recurse into them.
// Arguments:
// - target: The Pane we're attempting to split.
// - availableSpace: The theoretical space that's available for this pane to be able to split.
// Return Value:
// - nullopt if `target` is not this pane or a child of this pane, otherwise the
//   SplitState that `target` would use for an `Automatic` split given
//   `availableSpace`
std::optional<SplitState> Pane::PreCalculateAutoSplit(const std::shared_ptr<Pane> target,
                                                      const winrt::Windows::Foundation::Size availableSpace) const
{
    if (_IsLeaf())
    {
        if (target.get() == this)
        {
            //If this pane is a leaf, and it's the pane we're looking for, use
            //the available space to calculate which direction to split in.
            return availableSpace.Width > availableSpace.Height ? SplitState::Vertical : SplitState::Horizontal;
        }
        else
        {
            // If this pane is _any other leaf_, then just return nullopt, to
            // indicate that the `target` Pane is not down this branch.
            return std::nullopt;
        }
    }
    else
    {
        // If this pane is a parent, calculate how much space our children will
        // be able to use, and recurse into them.

        const bool isVerticalSplit = _splitState == SplitState::Vertical;
        const float firstWidth = isVerticalSplit ? (availableSpace.Width * _desiredSplitPosition) : availableSpace.Width;
        const float secondWidth = isVerticalSplit ? (availableSpace.Width - firstWidth) : availableSpace.Width;
        const float firstHeight = !isVerticalSplit ? (availableSpace.Height * _desiredSplitPosition) : availableSpace.Height;
        const float secondHeight = !isVerticalSplit ? (availableSpace.Height - firstHeight) : availableSpace.Height;

        const auto firstResult = _firstChild->PreCalculateAutoSplit(target, { firstWidth, firstHeight });
        return firstResult.has_value() ? firstResult : _secondChild->PreCalculateAutoSplit(target, { secondWidth, secondHeight });
    }

    // We should not possibly be getting here - both the above branches should
    // return a value.
    FAIL_FAST();
}

// Method Description:
// - Returns true if the pane or one of its descendants is read-only
bool Pane::ContainsReadOnly() const
{
    return _IsLeaf() ? _control.ReadOnly() : (_firstChild->ContainsReadOnly() || _secondChild->ContainsReadOnly());
}

DEFINE_EVENT(Pane, GotFocus, _GotFocusHandlers, winrt::delegate<std::shared_ptr<Pane>>);
DEFINE_EVENT(Pane, LostFocus, _LostFocusHandlers, winrt::delegate<std::shared_ptr<Pane>>);
DEFINE_EVENT(Pane, PaneRaiseBell, _PaneRaiseBellHandlers, winrt::Windows::Foundation::EventHandler<bool>);

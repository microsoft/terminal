// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Pane.h"
#include "AppLogic.h"

#include "Utils.h"

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

Pane::Pane(const Profile& profile, const TermControl& control, const bool lastFocused) :
    _control{ control },
    _lastActive{ lastFocused },
    _profile{ profile }
{
    _root.Children().Append(_borderFirst);
    _borderFirst.Child(_control);

    _setupControlEvents();

    // Register an event with the control to have it inform us when it gains focus.
    _gotFocusRevoker = _control.GotFocus(winrt::auto_revoke, { this, &Pane::_ControlGotFocusHandler });
    _lostFocusRevoker = _control.LostFocus(winrt::auto_revoke, { this, &Pane::_ControlLostFocusHandler });

    // When our border is tapped, make sure to transfer focus to our control.
    // LOAD-BEARING: This will NOT work if the border's BorderBrush is set to
    // Colors::Transparent! The border won't get Tapped events, and they'll fall
    // through to something else.
    _borderFirst.Tapped([this](auto&, auto& e) {
        _FocusFirstChild();
        e.Handled(true);
    });
    _borderSecond.Tapped([this](auto&, auto& e) {
        _FocusFirstChild();
        e.Handled(true);
    });
}

Pane::Pane(std::shared_ptr<Pane> first,
           std::shared_ptr<Pane> second,
           const SplitState splitState,
           const float splitPosition,
           const bool lastFocused) :
    _firstChild{ first },
    _secondChild{ second },
    _splitState{ splitState },
    _desiredSplitPosition{ splitPosition },
    _lastActive{ lastFocused }
{
    _CreateRowColDefinitions();
    _borderFirst.Child(_firstChild->GetRootElement());
    _borderSecond.Child(_secondChild->GetRootElement());

    // Use the unfocused border color as the pane background, so an actual color
    // appears behind panes as we animate them sliding in.
    _root.Background(_themeResources.unfocusedBorderBrush);

    _root.Children().Append(_borderFirst);
    _root.Children().Append(_borderSecond);

    _ApplySplitDefinitions();

    // Register event handlers on our children to handle their Close events
    _SetupChildCloseHandlers();

    // When our border is tapped, make sure to transfer focus to our control.
    // LOAD-BEARING: This will NOT work if the border's BorderBrush is set to
    // Colors::Transparent! The border won't get Tapped events, and they'll fall
    // through to something else.
    _borderFirst.Tapped([this](auto&, auto& e) {
        _FocusFirstChild();
        e.Handled(true);
    });
    _borderSecond.Tapped([this](auto&, auto& e) {
        _FocusFirstChild();
        e.Handled(true);
    });
}

void Pane::_setupControlEvents()
{
    _controlEvents._ConnectionStateChanged = _control.ConnectionStateChanged(winrt::auto_revoke, { this, &Pane::_ControlConnectionStateChangedHandler });
    _controlEvents._WarningBell = _control.WarningBell(winrt::auto_revoke, { this, &Pane::_ControlWarningBellHandler });
    _controlEvents._CloseTerminalRequested = _control.CloseTerminalRequested(winrt::auto_revoke, { this, &Pane::_CloseTerminalRequestedHandler });
    _controlEvents._RestartTerminalRequested = _control.RestartTerminalRequested(winrt::auto_revoke, { this, &Pane::_RestartTerminalRequestedHandler });
    _controlEvents._ReadOnlyChanged = _control.ReadOnlyChanged(winrt::auto_revoke, { this, &Pane::_ControlReadOnlyChangedHandler });
}
void Pane::_removeControlEvents()
{
    _controlEvents = {};
}

// Method Description:
// - Extract the terminal settings from the current (leaf) pane's control
//   to be used to create an equivalent control
// Arguments:
// - asContent: when true, we're trying to serialize this pane for moving across
//   windows. In that case, we'll need to fill in the content guid for our new
//   terminal args.
// Return Value:
// - Arguments appropriate for a SplitPane or NewTab action
NewTerminalArgs Pane::GetTerminalArgsForPane(const bool asContent) const
{
    // Leaves are the only things that have controls
    assert(_IsLeaf());

    NewTerminalArgs args{};
    auto controlSettings = _control.Settings();

    args.Profile(controlSettings.ProfileName());
    // If we know the user's working directory use it instead of the profile.
    if (const auto dir = _control.WorkingDirectory(); !dir.empty())
    {
        args.StartingDirectory(dir);
    }
    else
    {
        args.StartingDirectory(controlSettings.StartingDirectory());
    }
    args.TabTitle(controlSettings.StartingTitle());
    args.Commandline(controlSettings.Commandline());
    args.SuppressApplicationTitle(controlSettings.SuppressApplicationTitle());
    if (controlSettings.TabColor() || controlSettings.StartingTabColor())
    {
        til::color c;
        // StartingTabColor is prioritized over other colors
        if (const auto color = controlSettings.StartingTabColor())
        {
            c = til::color(color.Value());
        }
        else
        {
            c = til::color(controlSettings.TabColor().Value());
        }

        args.TabColor(winrt::Windows::Foundation::IReference<winrt::Windows::UI::Color>{ static_cast<winrt::Windows::UI::Color>(c) });
    }

    // TODO:GH#9800 - we used to be able to persist the color scheme that a
    // TermControl was initialized with, by name. With the change to having the
    // control own its own copy of its settings, this isn't possible anymore.
    //
    // We may be able to get around this by storing the Name in the Core::Scheme
    // object. That would work for schemes set by the Terminal, but not ones set
    // by VT, but that seems good enough.

    // Only fill in the ContentId if absolutely needed. If you fill in a number
    // here (even 0), we'll serialize that number, AND treat that action as an
    // "attach existing" rather than a "create"
    if (asContent)
    {
        args.ContentId(_control.ContentId());
    }

    return args;
}

// Method Description:
// - Serializes the state of this tab as a series of commands that can be
//   executed to recreate it.
// - This will always result in the right-most child being the focus
//   after the commands finish executing.
// Arguments:
// - currentId: the id to use for the current/first pane
// - nextId: the id to use for a new pane if we split
// - asContent: We're serializing this set of actions as content actions for
//   moving to other windows, so we need to make sure to include ContentId's
//   in the final actions.
// - asMovePane: only used with asContent. When this is true, we're building
//   these actions as a part of moving the pane to another window, but without
//   the context of the hosting tab. In that case, we'll want to build a
//   splitPane action even if we're just a single leaf, because there's no other
//   parent to try and build an action for us.
// Return Value:
// - The state from building the startup actions, includes a vector of commands,
//   the original root pane, the id of the focused pane, and the number of panes
//   created.
Pane::BuildStartupState Pane::BuildStartupActions(uint32_t currentId,
                                                  uint32_t nextId,
                                                  const bool asContent,
                                                  const bool asMovePane)
{
    // Normally, if we're a leaf, return an empt set of actions, because the
    // parent pane will build the SplitPane action for us. If we're building
    // actions for a movePane action though, we'll still need to include
    // ourselves.
    if (!asMovePane && _IsLeaf())
    {
        if (_lastActive)
        {
            // empty args, this is the first pane, currentId is
            return { .args = {}, .firstPane = shared_from_this(), .focusedPaneId = currentId, .panesCreated = 0 };
        }

        return { .args = {}, .firstPane = shared_from_this(), .focusedPaneId = std::nullopt, .panesCreated = 0 };
    }

    auto buildSplitPane = [&](auto newPane) {
        ActionAndArgs actionAndArgs;
        actionAndArgs.Action(ShortcutAction::SplitPane);
        const auto terminalArgs{ newPane->GetTerminalArgsForPane(asContent) };
        // When creating a pane the split size is the size of the new pane
        // and not position.
        const auto splitDirection = _splitState == SplitState::Horizontal ? SplitDirection::Down : SplitDirection::Right;
        const auto splitSize = (asContent && _IsLeaf() ? .5 : 1. - _desiredSplitPosition);
        SplitPaneArgs args{ SplitType::Manual, splitDirection, splitSize, terminalArgs };
        actionAndArgs.Args(args);

        return actionAndArgs;
    };

    if (asContent && _IsLeaf())
    {
        return {
            .args = { buildSplitPane(shared_from_this()) },
            .firstPane = shared_from_this(),
            .focusedPaneId = currentId,
            .panesCreated = 1
        };
    }

    auto buildMoveFocus = [](auto direction) {
        MoveFocusArgs args{ direction };

        ActionAndArgs actionAndArgs{};
        actionAndArgs.Action(ShortcutAction::MoveFocus);
        actionAndArgs.Args(args);

        return actionAndArgs;
    };

    // Handle simple case of a single split (a minor optimization for clarity)
    // Here we just create the second child (by splitting) and return the first
    // child for the parent to deal with.
    if (_firstChild->_IsLeaf() && _secondChild->_IsLeaf())
    {
        auto actionAndArgs = buildSplitPane(_secondChild);
        std::optional<uint32_t> focusedPaneId = std::nullopt;
        if (_firstChild->_lastActive)
        {
            focusedPaneId = currentId;
        }
        else if (_secondChild->_lastActive)
        {
            focusedPaneId = nextId;
        }

        return {
            .args = { actionAndArgs },
            .firstPane = _firstChild,
            .focusedPaneId = focusedPaneId,
            .panesCreated = 1
        };
    }

    // We now need to execute the commands for each side of the tree
    // We've done one split, so the first-most child will have currentId, and the
    // one after it will be incremented.
    auto firstState = _firstChild->BuildStartupActions(currentId, nextId + 1);
    // the next id for the second branch depends on how many splits were in the
    // first child.
    auto secondState = _secondChild->BuildStartupActions(nextId, nextId + firstState.panesCreated + 1);

    std::vector<ActionAndArgs> actions{};
    actions.reserve(firstState.args.size() + secondState.args.size() + 3);

    // first we make our split
    const auto newSplit = buildSplitPane(secondState.firstPane);
    actions.emplace_back(std::move(newSplit));

    if (firstState.args.size() > 0)
    {
        // Then move to the first child and execute any actions on the left branch
        // then move back
        actions.emplace_back(buildMoveFocus(FocusDirection::PreviousInOrder));
        actions.insert(actions.end(), std::make_move_iterator(std::begin(firstState.args)), std::make_move_iterator(std::end(firstState.args)));
        actions.emplace_back(buildMoveFocus(FocusDirection::NextInOrder));
    }

    // And if there are any commands to run on the right branch do so
    if (secondState.args.size() > 0)
    {
        actions.insert(actions.end(), std::make_move_iterator(secondState.args.begin()), std::make_move_iterator(secondState.args.end()));
    }

    // if the tree is well-formed then f1.has_value and f2.has_value are
    // mutually exclusive.
    const auto focusedPaneId = firstState.focusedPaneId.has_value() ? firstState.focusedPaneId : secondState.focusedPaneId;

    return {
        .args = { actions },
        .firstPane = firstState.firstPane,
        .focusedPaneId = focusedPaneId,
        .panesCreated = firstState.panesCreated + secondState.panesCreated + 1
    };
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

    auto amount = .05f;
    if (direction == ResizeDirection::Right || direction == ResizeDirection::Down)
    {
        amount = -amount;
    }

    // Make sure we're not making a pane explode here by resizing it to 0 characters.
    const auto changeWidth = _splitState == SplitState::Vertical;

    const Size actualSize{ gsl::narrow_cast<float>(_root.ActualWidth()),
                           gsl::narrow_cast<float>(_root.ActualHeight()) };
    // actualDimension is the size in DIPs of this pane in the direction we're
    // resizing.
    const auto actualDimension = changeWidth ? actualSize.Width : actualSize.Height;

    _desiredSplitPosition = _ClampSplitPosition(changeWidth, _desiredSplitPosition - amount, actualDimension);

    // Resize our columns to match the new percentages.
    _CreateRowColDefinitions();

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

    // Check if either our first or second child is the currently focused pane.
    // If it is, and the requested resize direction matches our separator, then
    // we're the pane that needs to adjust its separator.
    // If our separator is the wrong direction, then we can't handle it.
    const auto firstIsFocused = _firstChild->_lastActive;
    const auto secondIsFocused = _secondChild->_lastActive;
    if (firstIsFocused || secondIsFocused)
    {
        return _Resize(direction);
    }

    // If neither of our children were the focused pane, then recurse into
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
// - Attempt to navigate from the sourcePane according to direction.
//   - If the direction is NextInOrder or PreviousInOrder, the next or previous
//     leaf in the tree, respectively, will be returned.
//   - If the direction is {Up, Down, Left, Right} then the visually-adjacent
//     neighbor (if it exists) will be returned. If there are multiple options
//     then the first-most leaf will be selected.
// Arguments:
// - sourcePane: the pane to navigate from
// - direction: which direction to go in
// - mruPanes: the list of most recently used panes, in order
// Return Value:
// - The result of navigating from source according to direction, which may be
//   nullptr (i.e. no pane was found in that direction).
std::shared_ptr<Pane> Pane::NavigateDirection(const std::shared_ptr<Pane> sourcePane, const FocusDirection& direction, const std::vector<uint32_t>& mruPanes)
{
    // Can't navigate anywhere if we are a leaf
    if (_IsLeaf())
    {
        return nullptr;
    }

    if (direction == FocusDirection::None)
    {
        return nullptr;
    }

    // Check if moving up or down the tree
    if (direction == FocusDirection::Parent)
    {
        if (const auto parent = _FindParentOfPane(sourcePane))
        {
            // Keep a reference to which child we came from
            parent->_parentChildPath = sourcePane->weak_from_this();

            return parent;
        }
        return nullptr;
    }

    if (direction == FocusDirection::Child)
    {
        if (!sourcePane->_IsLeaf())
        {
            auto child = sourcePane->_firstChild;
            // If we've recorded path try to go back down it
            if (const auto prevFocus = sourcePane->_parentChildPath.lock())
            {
                child = prevFocus;
            }
            // clean up references
            sourcePane->_parentChildPath.reset();
            return child;
        }
        return nullptr;
    }

    // Previous movement relies on the last used panes
    if (direction == FocusDirection::Previous)
    {
        // If there is actually a previous pane.
        if (mruPanes.size() > 1)
        {
            // This could return nullptr if the id is not actually in the tree.
            return FindPane(mruPanes.at(1));
        }
        return nullptr;
    }

    // Check if we in-order traversal is requested
    if (direction == FocusDirection::NextInOrder)
    {
        return NextPane(sourcePane);
    }

    if (direction == FocusDirection::PreviousInOrder)
    {
        return PreviousPane(sourcePane);
    }

    // Fixed movement
    if (direction == FocusDirection::First)
    {
        // Just get the first leaf pane.
        auto firstPane = _FindPane([](const auto& p) { return p->_IsLeaf(); });

        // Don't need to do any movement if we are the source and target pane.
        if (firstPane == sourcePane)
        {
            return nullptr;
        }
        return firstPane;
    }

    // We are left with directional traversal now
    // If the focus direction does not match the split direction, the source pane
    // and its neighbor must necessarily be contained within the same child.
    if (!DirectionMatchesSplit(direction, _splitState))
    {
        if (const auto p = _firstChild->NavigateDirection(sourcePane, direction, mruPanes))
        {
            return p;
        }
        return _secondChild->NavigateDirection(sourcePane, direction, mruPanes);
    }

    // Since the direction is the same as our split, it is possible that we must
    // move focus from one child to another child.
    // We now must keep track of state while we recurse.
    // If we have it, get the size of this pane.
    const auto scaleX = _root.ActualWidth() > 0 ? gsl::narrow_cast<float>(_root.ActualWidth()) : 1.f;
    const auto scaleY = _root.ActualHeight() > 0 ? gsl::narrow_cast<float>(_root.ActualHeight()) : 1.f;
    const auto paneNeighborPair = _FindPaneAndNeighbor(sourcePane, direction, { 0, 0, scaleX, scaleY });

    if (paneNeighborPair.source && paneNeighborPair.neighbor)
    {
        return paneNeighborPair.neighbor;
    }

    return nullptr;
}

// Method Description:
// - Attempts to find the succeeding pane of the provided pane.
// - NB: If targetPane is not a leaf, then this will return one of its children.
// Arguments:
// - targetPane: The pane to search for.
// Return Value:
// - The next pane in tree order after the target pane (if found)
std::shared_ptr<Pane> Pane::NextPane(const std::shared_ptr<Pane> targetPane)
{
    // if we are a leaf pane there is no next pane.
    if (_IsLeaf())
    {
        return nullptr;
    }

    std::shared_ptr<Pane> firstLeaf = nullptr;
    std::shared_ptr<Pane> nextPane = nullptr;
    auto foundTarget = false;

    auto foundNext = WalkTree([&](auto pane) {
        // If we are a parent pane we don't want to move to one of our children
        if (foundTarget && targetPane->_HasChild(pane))
        {
            return false;
        }
        // In case the target pane is the last pane in the tree, keep a reference
        // to the first leaf so we can wrap around.
        if (firstLeaf == nullptr && pane->_IsLeaf())
        {
            firstLeaf = pane;
        }

        // If we've found the target pane already, get the next leaf pane.
        if (foundTarget && pane->_IsLeaf())
        {
            nextPane = pane;
            return true;
        }

        // Test if we're the target pane so we know to return the next pane.
        if (pane == targetPane)
        {
            foundTarget = true;
        }

        return false;
    });

    // If we found the desired pane just return it
    if (foundNext)
    {
        return nextPane;
    }

    // If we found the target pane, but not the next pane it means we were the
    // last leaf in the tree.
    if (foundTarget)
    {
        return firstLeaf;
    }

    return nullptr;
}

// Method Description:
// - Attempts to find the preceding pane of the provided pane.
// Arguments:
// - targetPane: The pane to search for.
// Return Value:
// - The previous pane in tree order before the target pane (if found)
std::shared_ptr<Pane> Pane::PreviousPane(const std::shared_ptr<Pane> targetPane)
{
    // if we are a leaf pane there is no previous pane.
    if (_IsLeaf())
    {
        return nullptr;
    }

    std::shared_ptr<Pane> lastLeaf = nullptr;
    auto foundTarget = false;

    WalkTree([&](auto pane) {
        if (pane == targetPane)
        {
            foundTarget = true;
            // If we were not the first leaf, then return the previous leaf.
            // Otherwise keep walking the tree to get the last pane.
            if (lastLeaf != nullptr)
            {
                return true;
            }
        }

        if (pane->_IsLeaf())
        {
            lastLeaf = pane;
        }

        return false;
    });

    // If we found the target pane then lastLeaf will either be the preceding
    // pane or the last pane in the tree if targetPane is the first leaf.
    if (foundTarget)
    {
        return lastLeaf;
    }

    return nullptr;
}

// Method Description:
// - Attempts to find the parent pane of the provided pane.
// Arguments:
// - pane: The pane to search for.
// Return Value:
// - the parent of `pane` if pane is in this tree.
std::shared_ptr<Pane> Pane::_FindParentOfPane(const std::shared_ptr<Pane> pane)
{
    return _FindPane([&](const auto& p) {
        return p->_firstChild == pane || p->_secondChild == pane;
    });
}

// Method Description:
// - Attempts to swap the location of the two given panes in the tree.
//   Searches the tree starting at this pane to find the parent pane for each of
//   the arguments, and if both parents are found, replaces the appropriate
//   child in each.
// Arguments:
// - first: A pointer to the first pane to switch.
// - second: A pointer to the second pane to switch.
// Return Value:
// - true if a swap was performed.
bool Pane::SwapPanes(std::shared_ptr<Pane> first, std::shared_ptr<Pane> second)
{
    // If there is nothing to swap, just return.
    if (first == second || _IsLeaf())
    {
        return false;
    }

    // Similarly don't swap if we have a circular reference
    if (first->_HasChild(second) || second->_HasChild(first))
    {
        return false;
    }

    std::unique_lock lock{ _createCloseLock };

    // Recurse through the tree to find the parent panes of each pane that is
    // being swapped.
    auto firstParent = _FindParentOfPane(first);
    auto secondParent = _FindParentOfPane(second);

    // We should have found either no elements, or both elements.
    // If we only found one parent then the pane SwapPane was called on did not
    // contain both panes as leaves, as could happen if the tree was modified
    // after the pointers were found but before we reached this function.
    if (firstParent && secondParent)
    {
        // Before we swap anything get the borders for the parents so that
        // it can be propagated to the swapped child.
        firstParent->_borders = firstParent->_GetCommonBorders();
        secondParent->_borders = secondParent->_GetCommonBorders();

        // Replace the old child with new one, and revoke appropriate event
        // handlers.
        auto replaceChild = [](auto& parent, auto oldChild, auto newChild) {
            // Revoke the old handlers
            if (parent->_firstChild == oldChild)
            {
                parent->_firstChild->Closed(parent->_firstClosedToken);
                parent->_firstChild = newChild;
            }
            else if (parent->_secondChild == oldChild)
            {
                parent->_secondChild->Closed(parent->_secondClosedToken);
                parent->_secondChild = newChild;
            }
            // Clear now to ensure that we can add the child's grid to us later
            parent->_root.Children().Clear();
            parent->_borderFirst.Child(nullptr);
            parent->_borderSecond.Child(nullptr);
        };

        // Make sure that the right event handlers are set, and the children
        // are placed in the appropriate locations in the grid.
        auto updateParent = [](auto& parent) {
            // just always revoke the old helpers since we are making new ones.
            parent->_firstChild->Closed(parent->_firstClosedToken);
            parent->_secondChild->Closed(parent->_secondClosedToken);
            parent->_SetupChildCloseHandlers();
            parent->_root.Children().Clear();
            parent->_borderFirst.Child(nullptr);
            parent->_borderSecond.Child(nullptr);
            parent->_borderFirst.Child(parent->_firstChild->GetRootElement());
            parent->_borderSecond.Child(parent->_secondChild->GetRootElement());

            parent->_root.Children().Append(parent->_borderFirst);
            parent->_root.Children().Append(parent->_borderSecond);

            // reset split definitions to clear any set row/column
            parent->_root.ColumnDefinitions().Clear();
            parent->_root.RowDefinitions().Clear();
            parent->_CreateRowColDefinitions();
        };

        // If the firstParent and secondParent are the same, then we are just
        // swapping the first child and second child of that parent.
        if (firstParent == secondParent)
        {
            firstParent->_firstChild->Closed(firstParent->_firstClosedToken);
            firstParent->_secondChild->Closed(firstParent->_secondClosedToken);
            std::swap(firstParent->_firstChild, firstParent->_secondChild);

            updateParent(firstParent);
            firstParent->_ApplySplitDefinitions();
        }
        else
        {
            // Replace both children before updating display to ensure
            // that the grid elements are not attached to multiple panes
            replaceChild(firstParent, first, second);
            replaceChild(secondParent, second, first);
            updateParent(firstParent);
            updateParent(secondParent);

            // If one of the two parents is a child of the other we only want
            // to apply the split definitions to the greatest parent to make
            // sure that all panes get the correct borders. if this is not done
            // and the ordering happens to be bad one parent's children will lose
            // a border.
            if (firstParent->_HasChild(secondParent))
            {
                firstParent->_ApplySplitDefinitions();
            }
            else if (secondParent->_HasChild(firstParent))
            {
                secondParent->_ApplySplitDefinitions();
            }
            else
            {
                firstParent->_ApplySplitDefinitions();
                secondParent->_ApplySplitDefinitions();
            }
        }

        // Refocus the last pane if there was a pane focused
        if (const auto focus = first->GetActivePane())
        {
            focus->_Focus();
        }

        if (const auto focus = second->GetActivePane())
        {
            focus->_Focus();
        }

        return true;
    }

    return false;
}

// Method Description:
// - Given two panes' offsets, test whether the `direction` side of first is adjacent to second.
// Arguments:
// - firstOffset: The offset for the reference pane
// - secondOffset: the offset to test adjacency with.
// - direction: The direction to search in from the reference pane.
// Return Value:
// - true if the two panes are adjacent.
bool Pane::_IsAdjacent(const PanePoint firstOffset,
                       const PanePoint secondOffset,
                       const FocusDirection& direction) const
{
    // Since float equality is tricky (arithmetic is non-associative, commutative),
    // test if the two numbers are within an epsilon distance of each other.
    auto floatEqual = [](float left, float right) {
        return abs(left - right) < 1e-4F;
    };

    auto getXMax = [](PanePoint offset) {
        return offset.x + offset.scaleX;
    };

    auto getYMax = [](PanePoint offset) {
        return offset.y + offset.scaleY;
    };

    // When checking containment in a range, the range is half-closed, i.e. [x, x+w).
    // If the direction is left test that the left side of the first element is
    // next to the right side of the second element, and that the top left
    // corner of the first element is within the second element's height
    if (direction == FocusDirection::Left)
    {
        const auto sharesBorders = floatEqual(firstOffset.x, getXMax(secondOffset));
        const auto withinHeight = (firstOffset.y >= secondOffset.y) && (firstOffset.y < getYMax(secondOffset));

        return sharesBorders && withinHeight;
    }
    // If the direction is right test that the right side of the first element is
    // next to the left side of the second element, and that the top left
    // corner of the first element is within the second element's height
    else if (direction == FocusDirection::Right)
    {
        const auto sharesBorders = floatEqual(getXMax(firstOffset), secondOffset.x);
        const auto withinHeight = (firstOffset.y >= secondOffset.y) && (firstOffset.y < getYMax(secondOffset));

        return sharesBorders && withinHeight;
    }
    // If the direction is up test that the top side of the first element is
    // next to the bottom side of the second element, and that the top left
    // corner of the first element is within the second element's width
    else if (direction == FocusDirection::Up)
    {
        const auto sharesBorders = floatEqual(firstOffset.y, getYMax(secondOffset));
        const auto withinWidth = (firstOffset.x >= secondOffset.x) && (firstOffset.x < getXMax(secondOffset));

        return sharesBorders && withinWidth;
    }
    // If the direction is down test that the bottom side of the first element is
    // next to the top side of the second element, and that the top left
    // corner of the first element is within the second element's width
    else if (direction == FocusDirection::Down)
    {
        const auto sharesBorders = floatEqual(getYMax(firstOffset), secondOffset.y);
        const auto withinWidth = (firstOffset.x >= secondOffset.x) && (firstOffset.x < getXMax(secondOffset));

        return sharesBorders && withinWidth;
    }
    return false;
}

// Method Description:
// - Gets the offsets for the two children of this parent pane
// - If real dimensions are not available, simulated ones based on the split size
//   will be used instead.
// Arguments:
// - parentOffset the location and scale information of this pane.
// Return Value:
// - the two location/scale points for the children panes.
std::pair<Pane::PanePoint, Pane::PanePoint> Pane::_GetOffsetsForPane(const PanePoint parentOffset) const
{
    assert(!_IsLeaf());
    auto firstOffset = parentOffset;
    auto secondOffset = parentOffset;

    // Make up fake dimensions using an exponential layout. This is useful
    // since we might need to navigate when there are panes not attached  to
    // the ui tree, such as initialization, command running, and zoom.
    // Basically create the tree layout on the fly by partitioning [0,1].
    // This could run into issues if the tree depth is >127 (or other
    // degenerate splits) as a float's mantissa only has so many bits of
    // precision.

    if (_splitState == SplitState::Horizontal)
    {
        secondOffset.y += _desiredSplitPosition * parentOffset.scaleY;
        firstOffset.scaleY *= _desiredSplitPosition;
        secondOffset.scaleY *= (1 - _desiredSplitPosition);
    }
    else
    {
        secondOffset.x += _desiredSplitPosition * parentOffset.scaleX;
        firstOffset.scaleX *= _desiredSplitPosition;
        secondOffset.scaleX *= (1 - _desiredSplitPosition);
    }

    return { firstOffset, secondOffset };
}

// Method Description:
// - Given the source pane, and its relative position in the tree, attempt to
//   find its visual neighbor within the current pane's tree.
//   The neighbor, if it exists, will be a leaf pane.
// Arguments:
// - direction: The direction to search in from the source pane.
// - searchResult: the source pane and its relative position.
// - sourceIsSecondSide: If the source pane is on the "second" side (down/right of split)
//   relative to the branch being searched
// - offset: the offset of the current pane
// Return Value:
// - A tuple of Panes, the first being the focused pane if found, and the second
//   being the adjacent pane if it exists, and a bool that represents if the move
//   goes out of bounds.
Pane::PaneNeighborSearch Pane::_FindNeighborForPane(const FocusDirection& direction,
                                                    PaneNeighborSearch searchResult,
                                                    const bool sourceIsSecondSide,
                                                    const Pane::PanePoint offset)
{
    // Test if the move will go out of boundaries. E.g. if the focus is already
    // on the second child of some pane and it attempts to move right, there
    // can't possibly be a neighbor to be found in the first child.
    if ((sourceIsSecondSide && (direction == FocusDirection::Right || direction == FocusDirection::Down)) ||
        (!sourceIsSecondSide && (direction == FocusDirection::Left || direction == FocusDirection::Up)))
    {
        return searchResult;
    }

    // If we are a leaf node test if we adjacent to the focus node
    if (_IsLeaf())
    {
        if (_IsAdjacent(searchResult.sourceOffset, offset, direction))
        {
            searchResult.neighbor = shared_from_this();
        }
        return searchResult;
    }

    auto [firstOffset, secondOffset] = _GetOffsetsForPane(offset);
    auto sourceNeighborSearch = _firstChild->_FindNeighborForPane(direction, searchResult, sourceIsSecondSide, firstOffset);
    if (sourceNeighborSearch.neighbor)
    {
        return sourceNeighborSearch;
    }

    return _secondChild->_FindNeighborForPane(direction, searchResult, sourceIsSecondSide, secondOffset);
}

// Method Description:
// - Searches the tree to find the source pane, and if it exists, the
//   visually adjacent pane by direction.
// Arguments:
// - sourcePane: The pane to find the neighbor of.
// - direction: The direction to search in from the focused pane.
// - offset: The offset, with the top-left corner being (0,0), that the current pane is relative to the root.
// Return Value:
// - The (partial) search result. If the search was successful, the pane and its neighbor will be returned.
//   Otherwise, the neighbor will be null and the focus will be null/non-null if it was found.
Pane::PaneNeighborSearch Pane::_FindPaneAndNeighbor(const std::shared_ptr<Pane> sourcePane, const FocusDirection& direction, const Pane::PanePoint offset)
{
    // If we are the source pane, return ourselves
    if (this == sourcePane.get())
    {
        return { shared_from_this(), nullptr, offset };
    }

    if (_IsLeaf())
    {
        return { nullptr, nullptr, offset };
    }

    auto [firstOffset, secondOffset] = _GetOffsetsForPane(offset);

    auto sourceNeighborSearch = _firstChild->_FindPaneAndNeighbor(sourcePane, direction, firstOffset);
    // If we have both the focus element and its neighbor, we are done
    if (sourceNeighborSearch.source && sourceNeighborSearch.neighbor)
    {
        return sourceNeighborSearch;
    }
    // if we only found the focus, then we search the second branch for the
    // neighbor.
    if (sourceNeighborSearch.source)
    {
        // If we can possibly have both sides of a direction, check if the sibling has the neighbor
        if (DirectionMatchesSplit(direction, _splitState))
        {
            return _secondChild->_FindNeighborForPane(direction, sourceNeighborSearch, false, secondOffset);
        }
        return sourceNeighborSearch;
    }

    // If we didn't find the focus at all, we need to search the second branch
    // for the focus (and possibly its neighbor).
    sourceNeighborSearch = _secondChild->_FindPaneAndNeighbor(sourcePane, direction, secondOffset);
    // We found both so we are done.
    if (sourceNeighborSearch.source && sourceNeighborSearch.neighbor)
    {
        return sourceNeighborSearch;
    }
    // We only found the focus, which means that its neighbor might be in the
    // first branch.
    if (sourceNeighborSearch.source)
    {
        // If we can possibly have both sides of a direction, check if the sibling has the neighbor
        if (DirectionMatchesSplit(direction, _splitState))
        {
            return _firstChild->_FindNeighborForPane(direction, sourceNeighborSearch, true, firstOffset);
        }
        return sourceNeighborSearch;
    }

    return { nullptr, nullptr, offset };
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
    // parent will register its own event handler. That event handler will get
    // fired after this handler returns, and will properly cleanup state.
    if (!_IsLeaf())
    {
        return;
    }

    const auto newConnectionState = _control.ConnectionState();
    const auto previousConnectionState = std::exchange(_connectionState, newConnectionState);

    if (newConnectionState < ConnectionState::Closed)
    {
        // Pane doesn't care if the connection isn't entering a terminal state.
        return;
    }

    if (previousConnectionState < ConnectionState::Connected && newConnectionState >= ConnectionState::Failed)
    {
        // A failure to complete the connection (before it has _connected_) is not covered by "closeOnExit".
        // This is to prevent a misconfiguration (closeOnExit: always, startingDirectory: garbage) resulting
        // in Terminal flashing open and immediately closed.
        return;
    }

    if (_profile)
    {
        if (_isDefTermSession && _profile.CloseOnExit() == CloseOnExitMode::Automatic)
        {
            // For 'automatic', we only care about the connection state if we were launched by Terminal
            // Since we were launched via defterm, ignore the connection state (i.e. we treat the
            // close on exit mode as 'always', see GH #13325 for discussion)
            Close();
        }

        const auto mode = _profile.CloseOnExit();
        if ((mode == CloseOnExitMode::Always) ||
            ((mode == CloseOnExitMode::Graceful || mode == CloseOnExitMode::Automatic) && newConnectionState == ConnectionState::Closed))
        {
            Close();
        }
    }
}

void Pane::_CloseTerminalRequestedHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                          const winrt::Windows::Foundation::IInspectable& /*args*/)
{
    std::unique_lock lock{ _createCloseLock };

    // It's possible that this event handler started being executed, then before
    // we got the lock, another thread created another child. So our control is
    // actually no longer _our_ control, and instead could be a descendant.
    //
    // When the control's new Pane takes ownership of the control, the new
    // parent will register its own event handler. That event handler will get
    // fired after this handler returns, and will properly cleanup state.
    if (!_IsLeaf())
    {
        return;
    }

    Close();
}

void Pane::_RestartTerminalRequestedHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                            const winrt::Windows::Foundation::IInspectable& /*args*/)
{
    if (!_IsLeaf())
    {
        return;
    }
    _RestartTerminalRequestedHandlers(shared_from_this());
}

winrt::fire_and_forget Pane::_playBellSound(winrt::Windows::Foundation::Uri uri)
{
    auto weakThis{ weak_from_this() };

    co_await wil::resume_foreground(_root.Dispatcher());
    if (auto pane{ weakThis.lock() })
    {
        if (!_bellPlayerCreated)
        {
            // The MediaPlayer might not exist on Windows N SKU.
            try
            {
                _bellPlayerCreated = true;
                _bellPlayer = winrt::Windows::Media::Playback::MediaPlayer();
                // GH#12258: The media keys (like play/pause) should have no effect on our bell sound.
                _bellPlayer.CommandManager().IsEnabled(false);
            }
            CATCH_LOG();
        }
        if (_bellPlayer)
        {
            const auto source{ winrt::Windows::Media::Core::MediaSource::CreateFromUri(uri) };
            const auto item{ winrt::Windows::Media::Playback::MediaPlaybackItem(source) };
            _bellPlayer.Source(item);
            _bellPlayer.Play();
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
    if (_profile)
    {
        // We don't want to do anything if nothing is set, so check for that first
        if (static_cast<int>(_profile.BellStyle()) != 0)
        {
            if (WI_IsFlagSet(_profile.BellStyle(), winrt::Microsoft::Terminal::Settings::Model::BellStyle::Audible))
            {
                // Audible is set, play the sound
                auto sounds{ _profile.BellSound() };
                if (sounds && sounds.Size() > 0)
                {
                    winrt::hstring soundPath{ wil::ExpandEnvironmentStringsW<std::wstring>(sounds.GetAt(rand() % sounds.Size()).c_str()) };
                    winrt::Windows::Foundation::Uri uri{ soundPath };
                    _playBellSound(uri);
                }
                else
                {
                    const auto soundAlias = reinterpret_cast<LPCTSTR>(SND_ALIAS_SYSTEMHAND);
                    PlaySound(soundAlias, NULL, SND_ALIAS_ID | SND_ASYNC | SND_SENTRY);
                }
            }

            if (WI_IsFlagSet(_profile.BellStyle(), winrt::Microsoft::Terminal::Settings::Model::BellStyle::Window))
            {
                _control.BellLightOn();
            }

            // raise the event with the bool value corresponding to the taskbar flag
            _PaneRaiseBellHandlers(nullptr, WI_IsFlagSet(_profile.BellStyle(), winrt::Microsoft::Terminal::Settings::Model::BellStyle::Taskbar));
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
void Pane::_ControlGotFocusHandler(const winrt::Windows::Foundation::IInspectable& sender,
                                   const RoutedEventArgs& /* args */)
{
    auto f = FocusState::Programmatic;
    if (const auto o = sender.try_as<winrt::Windows::UI::Xaml::Controls::Control>())
    {
        f = o.FocusState();
    }
    _GotFocusHandlers(shared_from_this(), f);
}

// Event Description:
// - Called when our control loses focus. We'll use this to trigger our LostFocus
//   callback. The tab that's hosting us should have registered a callback which
//   can be used to update its own internal focus state
void Pane::_ControlLostFocusHandler(const winrt::Windows::Foundation::IInspectable& /* sender */,
                                    const RoutedEventArgs& /* args */)
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

    // Clear out our media player callbacks, and stop any playing media. This
    // will prevent the callback from being triggered after we've closed, and
    // also make sure that our sound stops when we're closed.
    if (_bellPlayer)
    {
        _bellPlayer.Pause();
        _bellPlayer.Source(nullptr);
        _bellPlayer.Close();
        _bellPlayer = nullptr;
        _bellPlayerCreated = false;
    }

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
//   is a leaf and it's not focused. If it's a parent, it returns nullptr if it nor
//   any children of this pane were the last pane to be focused, or the Pane that
//   _was_ the last pane to be focused (if there was one).
// - This Pane's control might not currently be focused, if the tab itself is
//   not currently focused.
// Return Value:
// - nullptr if we're a leaf and unfocused, or no children were marked
//   `_lastActive`, else returns this
std::shared_ptr<Pane> Pane::GetActivePane()
{
    return _FindPane([](const auto& p) { return p->_lastActive; });
}

// Method Description:
// - Gets the TermControl of this pane. If this Pane is not a leaf but is
//   focused, this will return the control of the last leaf pane that had focus.
//   Otherwise, this will return the control of the first child of this pane.
// Arguments:
// - <none>
// Return Value:
// - nullptr if this Pane is an unfocused parent, otherwise the TermControl of this Pane.
TermControl Pane::GetLastFocusedTerminalControl()
{
    if (!_IsLeaf())
    {
        if (_lastActive)
        {
            auto pane = shared_from_this();
            while (const auto p = pane->_parentChildPath.lock())
            {
                if (p->_IsLeaf())
                {
                    return p->_control;
                }
                pane = p;
            }
            // We didn't find our child somehow, they might have closed under us.
        }
        return _firstChild->GetLastFocusedTerminalControl();
    }
    return _control;
}

// Method Description:
// - Gets the TermControl of this pane. If this Pane is not a leaf this will
//   return the nullptr;
// Arguments:
// - <none>
// Return Value:
// - nullptr if this Pane is a parent, otherwise the TermControl of this Pane.
TermControl Pane::GetTerminalControl()
{
    return _IsLeaf() ? _control : nullptr;
}

// Method Description:
// - Recursively remove the "Active" state from this Pane and all its children.
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
//   should be "active".
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
// - Returns nullptr if no children of this pane were the last control to be
//   focused, or the profile of the last control to be focused (if there was
//   one).
// Arguments:
// - <none>
// Return Value:
// - nullptr if no children of this pane were the last control to be
//   focused, else the profile of the last control to be focused
Profile Pane::GetFocusedProfile()
{
    auto lastFocused = GetActivePane();
    return lastFocused ? lastFocused->_profile : nullptr;
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
    return (_lastActive) ||
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
    // If we are the focused pane, but not a leaf we should add borders
    if (!_IsLeaf())
    {
        _UpdateBorders();
    }
    const auto& brush{ _ComputeBorderColor() };
    _borderFirst.BorderBrush(brush);
    _borderSecond.BorderBrush(brush);
}

// Method Description:
// - Focus the current pane. Also trigger focus on the control, or if not a leaf
//   the control belonging to the last focused leaf.
//   This makes sure that focus exists within the tab (since panes aren't proper controls)
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_Focus()
{
    _GotFocusHandlers(shared_from_this(), FocusState::Programmatic);
    if (const auto& control = GetLastFocusedTerminalControl())
    {
        control.Focus(FocusState::Programmatic);
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
        // Originally, we would only raise a GotFocus event here when:
        //
        // if (_root.ActualWidth() == 0 && _root.ActualHeight() == 0)
        //
        // When these sizes are 0, then the pane might still be in startup,
        // and doesn't yet have a real size. In that case, the control.Focus
        // event won't be handled until _after_ the startup events are all
        // processed. This will lead to the Tab not being notified that the
        // focus moved to a different Pane.
        //
        // However, with the ability to execute multiple actions at a time, in
        // already existing windows, we need to always raise this event manually
        // here, to correctly inform the Tab that we're now focused. This will
        // take care of commandlines like:
        //
        // `wtd -w 0 mf down ; sp`
        // `wtd -w 0 fp -t 1 ; sp`
        _Focus();
    }
    else
    {
        _firstChild->_FocusFirstChild();
    }
}

// Method Description:
// - Updates the settings of this pane, presuming that it is a leaf.
// Arguments:
// - settings: The new TerminalSettings to apply to any matching controls
// - profile: The profile from which these settings originated.
// Return Value:
// - <none>
void Pane::UpdateSettings(const TerminalSettingsCreateResult& settings, const Profile& profile)
{
    assert(_IsLeaf());

    _profile = profile;

    _control.UpdateControlSettings(settings.DefaultSettings(), settings.UnfocusedSettings());
}

// Method Description:
// - Attempts to add the provided pane as a split of the current pane.
// Arguments:
// - pane: the new pane to add
// - splitType: How the pane should be attached
// Return Value:
// - the new reference to the child created from the current pane.
std::shared_ptr<Pane> Pane::AttachPane(std::shared_ptr<Pane> pane, SplitDirection splitType)
{
    // Splice the new pane into the tree
    const auto [first, _] = _Split(splitType, .5, pane);

    // If the new pane has a child that was the focus, re-focus it
    // to steal focus from the currently focused pane.
    if (const auto focus = pane->GetActivePane())
    {
        focus->_Focus();
    }
    return first;
}

// Method Description:
// - Attempts to find the parent of the target pane,
//   if found remove the pane from the tree and return it.
// - If the removed pane was (or contained the focus) the first sibling will
//   gain focus.
// Arguments:
// - pane: the pane to detach
// Return Value:
// - The removed pane, if found.
std::shared_ptr<Pane> Pane::DetachPane(std::shared_ptr<Pane> pane)
{
    // We can't remove a pane if we only have a reference to a leaf, even if we
    // are the pane.
    if (_IsLeaf())
    {
        return nullptr;
    }

    // Check if either of our children matches the search
    const auto isFirstChild = _firstChild == pane;
    const auto isSecondChild = _secondChild == pane;

    if (isFirstChild || isSecondChild)
    {
        // Keep a reference to the child we are removing
        auto detached = isFirstChild ? _firstChild : _secondChild;
        // Remove the child from the tree, replace the current node with the
        // other child.
        _CloseChild(isFirstChild, true);

        // Update the borders on this pane and any children to match if we have
        // no parent.
        detached->_borders = Borders::None;
        detached->_ApplySplitDefinitions();

        // Trigger the detached event on each child
        detached->WalkTree([](auto pane) {
            pane->_DetachedHandlers(pane);
        });

        return detached;
    }

    if (const auto detached = _firstChild->DetachPane(pane))
    {
        return detached;
    }

    return _secondChild->DetachPane(pane);
}

// Method Description:
// - Closes one of our children. In doing so, takes the control from the other
//   child, and makes this pane a leaf node again.
// Arguments:
// - closeFirst: if true, the first child should be closed, and the second
//   should be preserved, and vice-versa for false.
// - isDetaching: if true, then the pane event handlers for the closed child
//   should be kept, this way they don't have to be recreated when it is later
//   reattached to a tree somewhere as the control moves with the pane.
// Return Value:
// - <none>
void Pane::_CloseChild(const bool closeFirst, const bool isDetaching)
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
    auto closedChildClosedToken = closeFirst ? _firstClosedToken : _secondClosedToken;
    auto remainingChildClosedToken = closeFirst ? _secondClosedToken : _firstClosedToken;

    // If we were a parent pane, and we pointed into the now closed child
    // clear it. We will set it to something else later if
    auto usedToFocusClosedChildsTerminal = false;
    if (const auto prev = _parentChildPath.lock())
    {
        if (closedChild == prev)
        {
            _parentChildPath.reset();
            usedToFocusClosedChildsTerminal = true;
        }
    }

    // If the only child left is a leaf, that means we're a leaf now.
    if (remainingChild->_IsLeaf())
    {
        // Find what borders need to persist after we close the child
        _borders = _GetCommonBorders();

        // take the control, profile, id and isDefTermSession of the pane that _wasn't_ closed.
        _control = remainingChild->_control;
        _connectionState = remainingChild->_connectionState;
        _profile = remainingChild->_profile;
        _id = remainingChild->Id();
        _isDefTermSession = remainingChild->_isDefTermSession;

        // Add our new event handler before revoking the old one.
        _setupControlEvents();

        // Revoke the old event handlers. Remove both the handlers for the panes
        // themselves closing, and remove their handlers for their controls
        // closing. At this point, if the remaining child's control is closed,
        // they'll trigger only our event handler for the control's close.

        // However, if we are detaching the pane we want to keep its control
        // handlers since it is just getting moved.
        if (!isDetaching)
        {
            closedChild->WalkTree([](auto p) {
                if (p->_IsLeaf())
                {
                    p->_removeControlEvents();
                }
            });
        }

        closedChild->Closed(closedChildClosedToken);
        remainingChild->Closed(remainingChildClosedToken);
        remainingChild->_removeControlEvents();

        // If we or either of our children was focused, we want to take that
        // focus from them.
        _lastActive = _lastActive || _firstChild->_lastActive || _secondChild->_lastActive;

        // Remove all the ui elements of the remaining child. This'll make sure
        // we can re-attach the TermControl to our Grid.
        remainingChild->_root.Children().Clear();
        remainingChild->_borderFirst.Child(nullptr);

        // Reset our UI:
        _root.Children().Clear();
        _borderFirst.Child(nullptr);
        _borderSecond.Child(nullptr);
        _root.ColumnDefinitions().Clear();
        _root.RowDefinitions().Clear();

        // Reattach the TermControl to our grid.
        _root.Children().Append(_borderFirst);
        _borderFirst.Child(_control);

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
        if (usedToFocusClosedChildsTerminal || _lastActive)
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
            _GotFocusHandlers(shared_from_this(), FocusState::Programmatic);
        }

        _UpdateBorders();

        // Release our children.
        _firstChild = nullptr;
        _secondChild = nullptr;
    }
    else
    {
        // Find what borders need to persist after we close the child
        auto remainingBorders = _GetCommonBorders();

        // Steal all the state from our child
        _splitState = remainingChild->_splitState;
        _firstChild = remainingChild->_firstChild;
        _secondChild = remainingChild->_secondChild;

        // Set up new close handlers on the children
        _SetupChildCloseHandlers();

        // Revoke the old event handlers on our new children
        _firstChild->Closed(remainingChild->_firstClosedToken);
        _secondChild->Closed(remainingChild->_secondClosedToken);

        // Remove the event handlers on the old children
        remainingChild->Closed(remainingChildClosedToken);
        closedChild->Closed(closedChildClosedToken);
        if (!isDetaching)
        {
            closedChild->WalkTree([](auto p) {
                if (p->_IsLeaf())
                {
                    p->_removeControlEvents();
                }
            });
        }

        // Reset our UI:
        _root.Children().Clear();
        _borderFirst.Child(nullptr);
        _borderSecond.Child(nullptr);
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
        remainingChild->_borderFirst.Child(nullptr);
        remainingChild->_borderSecond.Child(nullptr);

        _borderFirst.Child(_firstChild->GetRootElement());
        _borderSecond.Child(_secondChild->GetRootElement());

        _root.Children().Append(_borderFirst);
        _root.Children().Append(_borderSecond);

        // Propagate the new borders down to the children.
        _borders = remainingBorders;
        _ApplySplitDefinitions();

        // If our child had focus and closed, just transfer to the first remaining
        // child
        if (closedChild->_HasFocusedChild())
        {
            _FocusFirstChild();
        }
        // We might not have focus currently, but if our parent does then we
        // want to make sure we have a valid path to one of our children.
        // We should only update the path if our other child doesn't have focus itself.
        else if (usedToFocusClosedChildsTerminal && !_secondChild->_HasFocusedChild())
        {
            // update our path to our first remaining leaf
            _parentChildPath = _firstChild;
            _firstChild->WalkTree([](auto p) {
                if (p->_IsLeaf())
                {
                    return true;
                }
                p->_parentChildPath = p->_firstChild;
                return false;
            });
            // This will focus the first terminal, and will set that leaf pane
            // to the active pane if we nor one of our parents is not itself focused.
            _FocusFirstChild();
        }

        // Release the pointers that the child was holding.
        remainingChild->_firstChild = nullptr;
        remainingChild->_secondChild = nullptr;
    }

    // Notify the discarded child that it was closed by its parent
    closedChild->_ClosedByParentHandlers();
}

winrt::fire_and_forget Pane::_CloseChildRoutine(const bool closeFirst)
{
    auto weakThis{ shared_from_this() };

    co_await wil::resume_foreground(_root.Dispatcher());

    if (auto pane{ weakThis.get() })
    {
        // This will query if animations are enabled via the "Show animations in
        // Windows" setting in the OS
        winrt::Windows::UI::ViewManagement::UISettings uiSettings;
        const auto animationsEnabledInOS = uiSettings.AnimationsEnabled();
        const auto animationsEnabledInApp = Media::Animation::Timeline::AllowDependentAnimations();

        // GH#7252: If either child is zoomed, just skip the animation. It won't work.
        const auto eitherChildZoomed = pane->_firstChild->_zoomed || pane->_secondChild->_zoomed;
        // If animations are disabled, just skip this and go straight to
        // _CloseChild. Curiously, the pane opening animation doesn't need this,
        // and will skip straight to Completed when animations are disabled, but
        // this one doesn't seem to.
        if (!animationsEnabledInOS || !animationsEnabledInApp || eitherChildZoomed)
        {
            pane->_CloseChild(closeFirst, false);
            co_return;
        }

        // Setup the animation

        auto removedChild = closeFirst ? _firstChild : _secondChild;
        auto remainingChild = closeFirst ? _secondChild : _firstChild;
        const auto splitWidth = _splitState == SplitState::Vertical;

        Size removedOriginalSize{
            ::base::saturated_cast<float>(removedChild->_root.ActualWidth()),
            ::base::saturated_cast<float>(removedChild->_root.ActualHeight())
        };
        Size remainingOriginalSize{
            ::base::saturated_cast<float>(remainingChild->_root.ActualWidth()),
            ::base::saturated_cast<float>(remainingChild->_root.ActualHeight())
        };

        // Remove both children from the grid
        _borderFirst.Child(nullptr);
        _borderSecond.Child(nullptr);

        if (_splitState == SplitState::Vertical)
        {
            Controls::Grid::SetColumn(_borderFirst, 0);
            Controls::Grid::SetColumn(_borderSecond, 1);
        }
        else if (_splitState == SplitState::Horizontal)
        {
            Controls::Grid::SetRow(_borderFirst, 0);
            Controls::Grid::SetRow(_borderSecond, 1);
        }

        // Create the dummy grid. This grid will be the one we actually animate,
        // in the place of the closed pane.
        Controls::Grid dummyGrid;
        // GH#603 - we can safely add a BG here, as the control is gone right
        // away, to fill the space as the rest of the pane expands.
        dummyGrid.Background(_themeResources.unfocusedBorderBrush);
        // It should be the size of the closed pane.
        dummyGrid.Width(removedOriginalSize.Width);
        dummyGrid.Height(removedOriginalSize.Height);

        _borderFirst.Child(closeFirst ? dummyGrid : remainingChild->GetRootElement());
        _borderSecond.Child(closeFirst ? remainingChild->GetRootElement() : dummyGrid);

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
                pane->_CloseChild(closeFirst, false);
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
    // Zoomed panes, and focused parents should have full borders
    if (_zoomed || (!_IsLeaf() && _lastActive))
    {
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

    if (_IsLeaf())
    {
        _borderFirst.BorderThickness(ThicknessHelper::FromLengths(left, top, right, bottom));
    }
    else
    {
        // If we are not a leaf we don't want to duplicate the shared border
        // between our children.
        if (_splitState == SplitState::Vertical)
        {
            _borderFirst.BorderThickness(ThicknessHelper::FromLengths(left, top, 0, bottom));
            _borderSecond.BorderThickness(ThicknessHelper::FromLengths(0, top, right, bottom));
        }
        else
        {
            _borderFirst.BorderThickness(ThicknessHelper::FromLengths(left, top, right, 0));
            _borderSecond.BorderThickness(ThicknessHelper::FromLengths(left, 0, right, bottom));
        }
    }
}

// Method Description:
// - Find the borders for the leaf pane, or the shared borders for child panes.
// - This deliberately ignores if a focused parent has borders.
// Arguments:
// - <none>
// Return Value:
// - <none>
Borders Pane::_GetCommonBorders()
{
    if (_IsLeaf())
    {
        return _borders;
    }

    return _firstChild->_GetCommonBorders() & _secondChild->_GetCommonBorders();
}

// Method Description:
// - Sets the row/column of our child UI elements, to match our current split type.
// - In case the split definition or parent borders were changed, this recursively
//   updates the children as well.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_ApplySplitDefinitions()
{
    if (_splitState == SplitState::Vertical)
    {
        Controls::Grid::SetColumn(_borderFirst, 0);
        Controls::Grid::SetColumn(_borderSecond, 1);

        _firstChild->_borders = _borders | Borders::Right;
        _secondChild->_borders = _borders | Borders::Left;
        _borders = Borders::None;

        _firstChild->_ApplySplitDefinitions();
        _secondChild->_ApplySplitDefinitions();
    }
    else if (_splitState == SplitState::Horizontal)
    {
        Controls::Grid::SetRow(_borderFirst, 0);
        Controls::Grid::SetRow(_borderSecond, 1);

        _firstChild->_borders = _borders | Borders::Bottom;
        _secondChild->_borders = _borders | Borders::Top;
        _borders = Borders::None;

        _firstChild->_ApplySplitDefinitions();
        _secondChild->_ApplySplitDefinitions();
    }
    _UpdateBorders();
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

    const auto splitWidth = _splitState == SplitState::Vertical;
    const auto totalSize = splitWidth ? _root.ActualWidth() : _root.ActualHeight();
    // If we don't have a size yet, it's likely that we're in startup, or we're
    // being executed as a sequence of actions. In that case, just skip the
    // animation.
    if (totalSize <= 0 || !animationsEnabledInOS || !animationsEnabledInApp)
    {
        return;
    }

    // Use the unfocused border color as the pane background, so an actual color
    // appears behind panes as we animate them sliding in.
    //
    // GH#603 - We set only the background of the new pane, while it animates
    // in. Once the animation is done, we'll remove that background, so if the
    // user wants vintage opacity, they'll be able to see what's under the
    // window.
    // * If we don't give it a background, then the BG will be entirely transparent.
    // * If we give the parent (us) root BG a color, then a transparent pane
    //   will flash opaque during the animation, then back to transparent, which
    //   looks bad.
    _secondChild->_root.Background(_themeResources.unfocusedBorderBrush);

    const auto [firstSize, secondSize] = _CalcChildrenSizes(::base::saturated_cast<float>(totalSize));

    // This is safe to capture this, because it's only being called in the
    // context of this method (not on another thread)
    auto setupAnimation = [&](const auto& size, const bool isFirstChild) {
        auto child = isFirstChild ? _firstChild : _secondChild;
        auto childGrid = child->_root;
        // If we are splitting a parent pane this may be null
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
            if (control)
            {
                control.HorizontalAlignment(HorizontalAlignment::Left);
                control.Width(isFirstChild ? totalSize : size);
            }

            // When the animation is completed, undo the trickiness from before, to
            // restore the controls to the behavior they'd usually have.
            animation.Completed([childGrid, control, root = _secondChild->_root](auto&&, auto&&) {
                childGrid.Width(NAN);
                childGrid.HorizontalAlignment(HorizontalAlignment::Stretch);
                if (control)
                {
                    control.Width(NAN);
                    control.HorizontalAlignment(HorizontalAlignment::Stretch);
                }
                root.Background(nullptr);
            });
        }
        else
        {
            // If we're animating the first child, then stick to the top/left of
            // the parent pane, otherwise use the bottom/right. This is always
            // the "outside" of the parent pane.
            childGrid.VerticalAlignment(isFirstChild ? VerticalAlignment::Top : VerticalAlignment::Bottom);
            if (control)
            {
                control.VerticalAlignment(VerticalAlignment::Top);
                control.Height(isFirstChild ? totalSize : size);
            }

            // When the animation is completed, undo the trickiness from before, to
            // restore the controls to the behavior they'd usually have.
            animation.Completed([childGrid, control, root = _secondChild->_root](auto&&, auto&&) {
                childGrid.Height(NAN);
                childGrid.VerticalAlignment(VerticalAlignment::Stretch);
                if (control)
                {
                    control.Height(NAN);
                    control.VerticalAlignment(VerticalAlignment::Stretch);
                }
                root.Background(nullptr);
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
//   * If this pane is the pane we're looking for, use the
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
// - nullopt if the target is not found, or if there is not enough space to fit.
//   Otherwise will return the "real split direction" (converting automatic splits),
//   and will return either the split direction or nullopt.
std::optional<std::optional<SplitDirection>> Pane::PreCalculateCanSplit(const std::shared_ptr<Pane> target,
                                                                        SplitDirection splitType,
                                                                        const float splitSize,
                                                                        const winrt::Windows::Foundation::Size availableSpace) const
{
    if (target.get() == this)
    {
        const auto firstPercent = 1.0f - splitSize;
        const auto secondPercent = splitSize;
        // If this pane is the pane we're looking for, use
        // the available space to calculate which direction to split in.
        const auto minSize = _GetMinSize();

        if (splitType == SplitDirection::Automatic)
        {
            splitType = availableSpace.Width > availableSpace.Height ? SplitDirection::Right : SplitDirection::Down;
        }

        if (splitType == SplitDirection::Left || splitType == SplitDirection::Right)
        {
            const auto widthMinusSeparator = availableSpace.Width - CombinedPaneBorderSize;
            const auto newFirstWidth = widthMinusSeparator * firstPercent;
            const auto newSecondWidth = widthMinusSeparator * secondPercent;

            return newFirstWidth > minSize.Width && newSecondWidth > minSize.Width ? std::optional{ splitType } : std::nullopt;
        }

        else if (splitType == SplitDirection::Up || splitType == SplitDirection::Down)
        {
            const auto heightMinusSeparator = availableSpace.Height - CombinedPaneBorderSize;
            const auto newFirstHeight = heightMinusSeparator * firstPercent;
            const auto newSecondHeight = heightMinusSeparator * secondPercent;

            return newFirstHeight > minSize.Height && newSecondHeight > minSize.Height ? std::optional{ splitType } : std::nullopt;
        }
    }
    else if (_IsLeaf())
    {
        // If this pane is _any other leaf_, then just return nullopt, to
        // indicate that the `target` Pane is not down this branch.
        return std::nullopt;
    }
    else
    {
        // If this pane is a parent, calculate how much space our children will
        // be able to use, and recurse into them.

        const auto isVerticalSplit = _splitState == SplitState::Vertical;
        const auto firstWidth = isVerticalSplit ?
                                    (availableSpace.Width * _desiredSplitPosition) - PaneBorderSize :
                                    availableSpace.Width;
        const auto secondWidth = isVerticalSplit ?
                                     (availableSpace.Width - firstWidth) - PaneBorderSize :
                                     availableSpace.Width;
        const auto firstHeight = !isVerticalSplit ?
                                     (availableSpace.Height * _desiredSplitPosition) - PaneBorderSize :
                                     availableSpace.Height;
        const auto secondHeight = !isVerticalSplit ?
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
// - The same as above, except this takes in the pane directly instead of a
//   profile and control to make a pane with
// Arguments:
// - splitType: what type of split we want to create.
// - splitSize: the desired size of the split
// - newPane: the new pane
// Return Value:
// - The two newly created Panes, with the original pane first
std::pair<std::shared_ptr<Pane>, std::shared_ptr<Pane>> Pane::Split(SplitDirection splitType,
                                                                    const float splitSize,
                                                                    std::shared_ptr<Pane> newPane)
{
    if (!_lastActive)
    {
        if (_firstChild && _firstChild->_HasFocusedChild())
        {
            return _firstChild->Split(splitType, splitSize, newPane);
        }
        else if (_secondChild && _secondChild->_HasFocusedChild())
        {
            return _secondChild->Split(splitType, splitSize, newPane);
        }

        return { nullptr, nullptr };
    }

    return _Split(splitType, splitSize, newPane);
}

// Method Description:
// - Toggle the split orientation of the currently focused pane
// Arguments:
// - <none>
// Return Value:
// - true if a split was changed
bool Pane::ToggleSplitOrientation()
{
    // If we are a leaf there is no split to toggle.
    if (_IsLeaf())
    {
        return false;
    }

    // If a parent pane is focused, or if one of its children are a leaf and is
    // focused then switch the split orientation on the current pane.
    const auto firstIsFocused = _firstChild->_IsLeaf() && _firstChild->_lastActive;
    const auto secondIsFocused = _secondChild->_IsLeaf() && _secondChild->_lastActive;
    if (_lastActive || firstIsFocused || secondIsFocused)
    {
        // Switch the split orientation
        _splitState = _splitState == SplitState::Horizontal ? SplitState::Vertical : SplitState::Horizontal;

        // then update the borders and positioning on ourselves and our children.
        _borders = _GetCommonBorders();
        // Since we changed if we are using rows/columns, make sure we remove the old definitions
        _root.ColumnDefinitions().Clear();
        _root.RowDefinitions().Clear();
        _CreateRowColDefinitions();
        _ApplySplitDefinitions();

        return true;
    }

    return _firstChild->ToggleSplitOrientation() || _secondChild->ToggleSplitOrientation();
}

// Method Description:
// - Converts an "automatic" split type into either Vertical or Horizontal,
//   based upon the current dimensions of the Pane.
// - Similarly, if Up/Down or Left/Right are provided a Horizontal or Vertical
//   split type will be returned.
// Arguments:
// - splitType: The SplitDirection to attempt to convert
// Return Value:
// - One of Horizontal or Vertical
SplitState Pane::_convertAutomaticOrDirectionalSplitState(const SplitDirection& splitType) const
{
    // Careful here! If the pane doesn't yet have a size, these dimensions will
    // be 0, and we'll always return Vertical.

    if (splitType == SplitDirection::Automatic)
    {
        // If the requested split type was "auto", determine which direction to
        // split based on our current dimensions
        const Size actualSize{ gsl::narrow_cast<float>(_root.ActualWidth()),
                               gsl::narrow_cast<float>(_root.ActualHeight()) };
        return actualSize.Width >= actualSize.Height ? SplitState::Vertical : SplitState::Horizontal;
    }
    if (splitType == SplitDirection::Up || splitType == SplitDirection::Down)
    {
        return SplitState::Horizontal;
    }
    // All that is left is Left / Right which are vertical splits
    return SplitState::Vertical;
}

// Method Description:
// - Does the bulk of the work of creating a new split. Initializes our UI,
//   creates a new Pane to host the control, registers event handlers.
// Arguments:
// - splitType: what type of split we should create.
// - splitSize: what fraction of the pane the new pane should get
// - newPane: the pane to add as a child
// Return Value:
// - The two newly created Panes, with the original pane as the first pane.
std::pair<std::shared_ptr<Pane>, std::shared_ptr<Pane>> Pane::_Split(SplitDirection splitType,
                                                                     const float splitSize,
                                                                     std::shared_ptr<Pane> newPane)
{
    auto actualSplitType = _convertAutomaticOrDirectionalSplitState(splitType);

    // Lock the create/close lock so that another operation won't concurrently
    // modify our tree
    std::unique_lock lock{ _createCloseLock };

    if (_IsLeaf())
    {
        // revoke our handler - the child will take care of the control now.
        _removeControlEvents();

        // Remove our old GotFocus handler from the control. We don't want the
        // control telling us that it's now focused, we want it telling its new
        // parent.
        _gotFocusRevoker.revoke();
        _lostFocusRevoker.revoke();
    }

    // Remove any children we currently have. We can't add the existing
    // TermControl to a new grid until we do this.
    _root.Children().Clear();
    _borderFirst.Child(nullptr);
    _borderSecond.Child(nullptr);

    // Create a new pane from ourself
    if (!_IsLeaf())
    {
        // Since we are a parent we don't have borders normally,
        // so set them temporarily for when we update our split definition.
        _borders = _GetCommonBorders();
        _firstChild->Closed(_firstClosedToken);
        _secondChild->Closed(_secondClosedToken);
        // If we are not a leaf we should create a new pane that contains our children
        auto first = std::make_shared<Pane>(_firstChild, _secondChild, _splitState, _desiredSplitPosition);
        _firstChild = first;
    }
    else
    {
        //   Move our control, guid, isDefTermSession into the first one.
        _firstChild = std::make_shared<Pane>(_profile, _control);
        _firstChild->_connectionState = std::exchange(_connectionState, ConnectionState::NotConnected);
        _profile = nullptr;
        _control = { nullptr };
        _firstChild->_isDefTermSession = _isDefTermSession;
    }

    _splitState = actualSplitType;
    _desiredSplitPosition = 1.0f - splitSize;
    _secondChild = newPane;
    // If we want the new pane to be the first child, swap the children
    if (splitType == SplitDirection::Up || splitType == SplitDirection::Left)
    {
        std::swap(_firstChild, _secondChild);
    }

    _root.ColumnDefinitions().Clear();
    _root.RowDefinitions().Clear();
    _CreateRowColDefinitions();

    _borderFirst.Child(_firstChild->GetRootElement());
    _borderSecond.Child(_secondChild->GetRootElement());

    _root.Children().Append(_borderFirst);
    _root.Children().Append(_borderSecond);

    _ApplySplitDefinitions();

    // Register event handlers on our children to handle their Close events
    _SetupChildCloseHandlers();

    _lastActive = false;

    _SetupEntranceAnimation();

    // Clear out our ID, only leaves should have IDs
    _id = {};

    // Regardless of which child the new child is, we want to return the
    // original one first.
    if (splitType == SplitDirection::Up || splitType == SplitDirection::Left)
    {
        return { _secondChild, _firstChild };
    }
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
    _zoomed = (zoomedPane == shared_from_this());
    _UpdateBorders();
    if (!_IsLeaf())
    {
        if (zoomedPane == _firstChild || zoomedPane == _secondChild)
        {
            // When we're zooming the pane, we'll need to remove it from our UI
            // tree. Easy way: just remove both children. We'll re-attach both
            // when we un-zoom.
            _root.Children().Clear();
            _borderFirst.Child(nullptr);
            _borderSecond.Child(nullptr);
        }

        // Always recurse into both children. If the (un)zoomed pane was one of
        // our direct children, we'll still want to update its borders.
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
    _zoomed = false;
    _UpdateBorders();
    if (!_IsLeaf())
    {
        if (zoomedPane == _firstChild || zoomedPane == _secondChild)
        {
            // When we're un-zooming the pane, we'll need to re-add it to our UI
            // tree where it originally belonged. easy way: just re-add both.
            _root.Children().Clear();

            _borderFirst.Child(_firstChild->GetRootElement());
            _borderSecond.Child(_secondChild->GetRootElement());

            _root.Children().Append(_borderFirst);
            _root.Children().Append(_borderSecond);
        }

        // Always recurse into both children. If the (un)zoomed pane was one of
        // our direct children, we'll still want to update its borders.
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
std::optional<uint32_t> Pane::Id() noexcept
{
    return _id;
}

// Method Description:
// - Sets this pane's ID
// - Panes are given IDs upon creation by TerminalTab
// Arguments:
// - The number to set this pane's ID to
void Pane::Id(uint32_t id) noexcept
{
    _id = id;
}

// Method Description:
// - Recursive function that focuses a pane with the given ID
// Arguments:
// - The ID of the pane we want to focus
bool Pane::FocusPane(const uint32_t id)
{
    // Always clear the parent child path if we are focusing a leaf
    return WalkTree([=](auto p) {
        p->_parentChildPath.reset();
        if (p->_id == id)
        {
            // Make sure to use _FocusFirstChild here - that'll properly update the
            // focus if we're in startup.
            p->_FocusFirstChild();
            return true;
        }
        return false;
    });
}

// Method Description:
// - Focuses the given pane if it is in the tree.
// - This is different than FocusPane(id) in that it allows focusing
//   panes that are not leaves.
// Arguments:
// - the pane to focus
// Return Value:
// - true if focus was set
bool Pane::FocusPane(const std::shared_ptr<Pane> pane)
{
    return WalkTree([&](auto p) {
        if (p == pane)
        {
            p->_Focus();
            return true;
        }
        // clear the parent child path if we are not the pane being focused.
        p->_parentChildPath.reset();
        return false;
    });
}

// Method Description:
// - Check if this pane contains the argument as a child anywhere along the tree.
// Arguments:
// - child: the child to search for.
// Return Value:
// - true if the child was found.
bool Pane::_HasChild(const std::shared_ptr<Pane> child)
{
    if (child == nullptr)
    {
        return false;
    }

    return WalkTree([&](const auto& p) {
        return p->_firstChild == child || p->_secondChild == child;
    });
}

// Method Description:
// - Recursive function that finds a pane with the given ID
// Arguments:
// - The ID of the pane we want to find
// Return Value:
// - A pointer to the pane with the given ID, if found.
std::shared_ptr<Pane> Pane::FindPane(const uint32_t id)
{
    return _FindPane([=](const auto& p) { return p->_IsLeaf() && p->_id == id; });
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
    auto lastSizeTree{ sizeTree };

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
// - pair of floats, where first value is the size snapped downward (not greater than
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

        auto lower = _control.SnapDimensionToGrid(widthOrHeight, dimension);
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

// Method Description:
// - Update our stored brushes for the current theme. This will also recursively
//   update all our children.
// - TerminalPage creates these brushes and ultimately owns them. Effectively,
//   we're just storing a smart pointer to the page's brushes.
void Pane::UpdateResources(const PaneResources& resources)
{
    _themeResources = resources;
    UpdateVisuals();

    if (!_IsLeaf())
    {
        _firstChild->UpdateResources(resources);
        _secondChild->UpdateResources(resources);
    }
}

int Pane::GetLeafPaneCount() const noexcept
{
    return _IsLeaf() ? 1 : (_firstChild->GetLeafPaneCount() + _secondChild->GetLeafPaneCount());
}

// Method Description:
// - Should be called when this pane is created via a default terminal handoff
// - Finalizes our configuration given the information that we have been
//   created via default handoff
void Pane::FinalizeConfigurationGivenDefault()
{
    _isDefTermSession = true;
}

// Method Description:
// - Returns true if the pane or one of its descendants is read-only
bool Pane::ContainsReadOnly() const
{
    return _IsLeaf() ? _control.ReadOnly() : (_firstChild->ContainsReadOnly() || _secondChild->ContainsReadOnly());
}

// Method Description:
// - If we're a parent, place the taskbar state for all our leaves into the
//   provided vector.
// - If we're a leaf, place our own state into the vector.
// Arguments:
// - states: a vector that will receive all the states of all leaves in the tree
// Return Value:
// - <none>
void Pane::CollectTaskbarStates(std::vector<winrt::TerminalApp::TaskbarState>& states)
{
    if (_IsLeaf())
    {
        auto tbState{ winrt::make<winrt::TerminalApp::implementation::TaskbarState>(_control.TaskbarState(),
                                                                                    _control.TaskbarProgress()) };
        states.push_back(tbState);
    }
    else
    {
        _firstChild->CollectTaskbarStates(states);
        _secondChild->CollectTaskbarStates(states);
    }
}

void Pane::EnableBroadcast(bool enabled)
{
    if (_IsLeaf())
    {
        _broadcastEnabled = enabled;
        UpdateVisuals();
    }
    else
    {
        _firstChild->EnableBroadcast(enabled);
        _secondChild->EnableBroadcast(enabled);
    }
}

void Pane::BroadcastKey(const winrt::Microsoft::Terminal::Control::TermControl& sourceControl,
                        const WORD vkey,
                        const WORD scanCode,
                        const winrt::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                        const bool keyDown)
{
    WalkTree([&](const auto& pane) {
        if (pane->_IsLeaf() && pane->_control != sourceControl && !pane->_control.ReadOnly())
        {
            pane->_control.RawWriteKeyEvent(vkey, scanCode, modifiers, keyDown);
        }
    });
}

void Pane::BroadcastChar(const winrt::Microsoft::Terminal::Control::TermControl& sourceControl,
                         const wchar_t character,
                         const WORD scanCode,
                         const winrt::Microsoft::Terminal::Core::ControlKeyStates modifiers)
{
    WalkTree([&](const auto& pane) {
        if (pane->_IsLeaf() && pane->_control != sourceControl && !pane->_control.ReadOnly())
        {
            pane->_control.RawWriteChar(character, scanCode, modifiers);
        }
    });
}

void Pane::BroadcastString(const winrt::Microsoft::Terminal::Control::TermControl& sourceControl,
                           const winrt::hstring& text)
{
    WalkTree([&](const auto& pane) {
        if (pane->_IsLeaf() && pane->_control != sourceControl && !pane->_control.ReadOnly())
        {
            pane->_control.RawWriteString(text);
        }
    });
}

void Pane::_ControlReadOnlyChangedHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                          const winrt::Windows::Foundation::IInspectable& /*e*/)
{
    UpdateVisuals();
}

winrt::Windows::UI::Xaml::Media::SolidColorBrush Pane::_ComputeBorderColor()
{
    if (_lastActive)
    {
        return _themeResources.focusedBorderBrush;
    }

    if (_broadcastEnabled && (_IsLeaf() && !_control.ReadOnly()))
    {
        return _themeResources.broadcastBorderBrush;
    }

    return _themeResources.unfocusedBorderBrush;
}

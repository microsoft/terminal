// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Tab.h"
#include "Utils.h"
#include <debugapi.h>

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
}

Tab::Tab(const GUID& profile, const TermControl& control)
{
    _rootPane = std::make_shared<LeafPane>(profile, control, true);
    _MakeTabViewItem();
}

Tab::~Tab()
{
    _RemoveAllRootPaneEventHandlers();
    OutputDebugString(L"~Tab()\n");
}

void Tab::_MakeTabViewItem()
{
    _tabViewItem = ::winrt::MUX::Controls::TabViewItem{};
}

UIElement Tab::GetRootElement()
{
    return _rootPane->GetRootElement();
}

// Method Description:
// - Returns nullptr if no children of this tab were the last control to be
//   focused, or the TermControl that _was_ the last control to be focused (if
//   there was one).
// - This control might not currently be focused, if the tab itself is not
//   currently focused.
// Arguments:
// - <none>
// Return Value:
// - nullptr if no children were marked `_lastFocused`, else the TermControl
//   that was last focused.
TermControl Tab::GetActiveTerminalControl() const
{
    return _rootPane->FindActivePane()->GetTerminalControl();
}

winrt::MUX::Controls::TabViewItem Tab::GetTabViewItem()
{
    return _tabViewItem;
}

// Method Description:
// - Returns true if this is the currently focused tab. For any set of tabs,
//   there should only be one tab that is marked as focused, though each tab has
//   no control over the other tabs in the set.
// Arguments:
// - <none>
// Return Value:
// - true iff this tab is focused.
bool Tab::IsFocused() const noexcept
{
    return _focused;
}

// Method Description:
// - Updates our focus state. If we're gaining focus, make sure to transfer
//   focus to the last focused terminal control in our tree of controls.
// Arguments:
// - focused: our new focus state. If true, we should be focused. If false, we
//   should be unfocused.
// Return Value:
// - <none>
void Tab::SetFocused(const bool focused)
{
    _focused = focused;

    if (_focused)
    {
        _Focus();
    }
}

// Method Description:
// - Returns nullopt if no children of this tab were the last control to be
//   focused, or the GUID of the profile of the last control to be focused (if
//   there was one).
// Arguments:
// - <none>
// Return Value:
// - nullopt if no children of this tab were the last control to be
//   focused, else the GUID of the profile of the last control to be focused
std::optional<GUID> Tab::GetFocusedProfile() const noexcept
{
    return _rootPane->FindActivePane()->GetProfile();
}

// Method Description:
// - Called after construction of a Tab object to bind event handlers to its
//   associated Pane and TermControl object
// Arguments:
// - control: reference to the TermControl object to bind event to
// Return Value:
// - <none>
void Tab::BindEventHandlers() noexcept
{
    const auto rootPaneAsLeaf = std::dynamic_pointer_cast<LeafPane>(_rootPane);
    THROW_HR_IF_NULL(E_FAIL, rootPaneAsLeaf);

    _SetupRootPaneEventHandlers();
}

// Method Description:
// - Attempts to update the settings of this tab's tree of panes.
// Arguments:
// - settings: The new TerminalSettings to apply to any matching controls
// - profile: The GUID of the profile these settings should apply to.
// Return Value:
// - <none>
void Tab::UpdateSettings(const TerminalSettings& settings, const GUID& profile)
{
    _rootPane->UpdateSettings(settings, profile);
}

// Method Description:
// - Focus the last focused control in our tree of panes.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Tab::_Focus()
{
    _focused = true;

    auto lastFocusedControl = GetActiveTerminalControl();
    if (lastFocusedControl)
    {
        lastFocusedControl.Focus(FocusState::Programmatic);
    }
}

winrt::fire_and_forget Tab::UpdateIcon(const winrt::hstring iconPath)
{
    // Don't reload our icon if it hasn't changed.
    if (iconPath == _lastIconPath)
    {
        return;
    }

    _lastIconPath = iconPath;

    std::weak_ptr<Tab> weakThis{ shared_from_this() };

    co_await winrt::resume_foreground(_tabViewItem.Dispatcher());

    if (auto tab{ weakThis.lock() })
    {
        _tabViewItem.IconSource(GetColoredIcon<winrt::MUX::Controls::IconSource>(_lastIconPath));
    }
}

// Method Description:
// - Gets the title string of the last focused terminal control in our tree.
//   Returns the empty string if there is no such control.
// Arguments:
// - <none>
// Return Value:
// - the title string of the last focused terminal control in our tree.
winrt::hstring Tab::GetActiveTitle() const
{
    const auto lastFocusedControl = GetActiveTerminalControl();
    return lastFocusedControl ? lastFocusedControl.Title() : L"";
}

// Method Description:
// - Set the text on the TabViewItem for this tab.
// Arguments:
// - text: The new text string to use as the Header for our TabViewItem
// Return Value:
// - <none>
winrt::fire_and_forget Tab::SetTabText(const winrt::hstring text)
{
    // Copy the hstring, so we don't capture a dead reference
    winrt::hstring textCopy{ text };
    std::weak_ptr<Tab> weakThis{ shared_from_this() };

    co_await winrt::resume_foreground(_tabViewItem.Dispatcher());

    if (auto tab{ weakThis.lock() })
    {
        _tabViewItem.Header(winrt::box_value(text));
    }
}

// Method Description:
// - Move the viewport of the terminal up or down a number of lines. Negative
//      values of `delta` will move the view up, and positive values will move
//      the viewport down.
// Arguments:
// - delta: a number of lines to move the viewport relative to the current viewport.
// Return Value:
// - <none>
winrt::fire_and_forget Tab::Scroll(const int delta)
{
    auto control = GetActiveTerminalControl();

    co_await winrt::resume_foreground(control.Dispatcher());

    const auto currentOffset = control.GetScrollOffset();
    control.KeyboardScrollViewport(currentOffset + delta);
}

// Method Description:
// - Determines whether the focused pane has sufficient space to be split.
// Arguments:
// - splitType: The type of split we want to create.
// Return Value:
// - True if the focused pane can be split. False otherwise.
bool Tab::CanSplitPane(winrt::TerminalApp::SplitState splitType)
{
    return _rootPane->FindActivePane()->CanSplit(splitType);
}

// Method Description:
// - Split the focused pane in our tree of panes, and place the
//   given TermControl into the newly created pane.
// Arguments:
// - splitType: The type of split we want to create.
// - profile: The profile GUID to associate with the newly created pane.
// - control: A TermControl to use in the new pane.
// Return Value:
// - <none>
void Tab::SplitPane(winrt::TerminalApp::SplitState splitType, const GUID& profile, TermControl& control)
{
    const auto newLeafPane = _rootPane->FindActivePane()->Split(splitType, profile, control);

    _AttachEventHandlersToControl(control);

    // Add a event handlers to the new pane's GotFocus event. When the pane
    // gains focus, we'll mark it as the new active pane.
    _AttachEventHandlersToLeafPane(newLeafPane);
}

// Method Description:
// - See Pane::CalcSnappedDimension
float Tab::CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
{
    return _rootPane->CalcSnappedDimension(widthOrHeight, dimension);
}

// Method Description:
// - Update the size of our panes to fill the new given size. This happens when
//   the window is resized.
// Arguments:
// - newSize: the amount of space that the panes have to fill now.
// Return Value:
// - <none>
void Tab::ResizeContent(const winrt::Windows::Foundation::Size& newSize)
{
    // NOTE: This _must_ be called on the root pane, so that it can propagate
    // throughout the entire tree.
    _rootPane->ResizeContent(newSize);
}

// Method Description:
// - Attempt to move a separator between panes, as to resize each child on
//   either size of the separator. See Pane::ResizeChild for details.
// Arguments:
// - direction: The direction to move the separator in.
// Return Value:
// - <none>
void Tab::ResizePane(const winrt::TerminalApp::Direction& direction)
{
    // NOTE: This _must_ be called on the root pane, so that it can propagate
    // throughout the entire tree.

    if (const auto rootPaneAsParent = std::dynamic_pointer_cast<ParentPane>(_rootPane))
    {
        rootPaneAsParent->ResizeChild(direction);
    }
}

// Method Description:
// - Attempt to move focus between panes, as to focus the child on
//   the other side of the separator. See Pane::NavigateFocus for details.
// Arguments:
// - direction: The direction to move the focus in.
// Return Value:
// - <none>
void Tab::NavigateFocus(const winrt::TerminalApp::Direction& direction)
{
    // NOTE: This _must_ be called on the root pane, so that it can propagate
    // throughout the entire tree.

    if (const auto rootPaneAsParent = std::dynamic_pointer_cast<ParentPane>(_rootPane))
    {
        rootPaneAsParent->NavigateFocus(direction);
    }
}

// Method Description:
// - Closes the currently focused pane in this tab. If it's the last pane in
//   this tab, our Closed event will be fired (at a later time) for anyone
//   registered as a handler of our close event.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Tab::ClosePane()
{
    _rootPane->FindActivePane()->Close();
}

// Method Description:
// - Setups all the event handlers we care about for a root pane. These are superset of
//   the events that we register for every other pane in the tree, however, this method
//   also calls _AttachEventHandlersToLeafPane and _AttachEventHandlersToControl, so there
//   is no need to also call these on the root pane.
// - It is called on initialization, and when the root pane changes (this is, it gets 
//   splitted or collapsed after split).
// Arguments:
// - <none>
// Return Value:
// - <none>
void Tab::_SetupRootPaneEventHandlers()
{
    if (const auto rootPaneAsLeaf = std::dynamic_pointer_cast<LeafPane>(_rootPane))
    {
        // Root pane also belongs to the pane tree, so attach the usual events, as for
        // every other pane.
        _AttachEventHandlersToLeafPane(rootPaneAsLeaf);
        _AttachEventHandlersToControl(rootPaneAsLeaf->GetTerminalControl());

        // When root pane closes, the tab also closes.
        _rootPaneClosedToken = rootPaneAsLeaf->Closed([=](auto&& /*s*/, auto&& /*e*/) {
            _RemoveAllRootPaneEventHandlers();

            _ClosedHandlers(nullptr, nullptr);
        });

        // When root pane is a leaf and got splitted, it produces the new parent pane that contains
        // both him and the new leaf near him. We then replace that child with the new parent pane.
        _rootPaneTypeChangedToken = rootPaneAsLeaf->Splitted([=](std::shared_ptr<ParentPane> splittedPane) {
            _RemoveAllRootPaneEventHandlers();

            _rootPane = splittedPane;
            _SetupRootPaneEventHandlers();
            _RootPaneChangedHandlers();
        });
    }
    else if (const auto rootPaneAsParent = std::dynamic_pointer_cast<ParentPane>(_rootPane))
    {
        // When root pane is a parent and one of its children got closed (and so the parent collapses),
        // we take in its remaining, orphaned child as our own.
        _rootPaneTypeChangedToken = rootPaneAsParent->ChildClosed([=](std::shared_ptr<Pane> collapsedPane) {
            _RemoveAllRootPaneEventHandlers();

            _rootPane = collapsedPane;
            _SetupRootPaneEventHandlers();
            _RootPaneChangedHandlers();
        });
    }
}

// Method Description:
// - Unsubscribes from all the events of the root pane that we're subscribed to.
// - Called when the root pane gets splitted/collapsed (because it is now the root 
//   pane then), when the root pane closes and in dtor.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Tab::_RemoveAllRootPaneEventHandlers()
{
    if (const auto rootAsLeaf = std::dynamic_pointer_cast<LeafPane>(_rootPane))
    {
        rootAsLeaf->Closed(_rootPaneClosedToken);
        rootAsLeaf->Splitted(_rootPaneTypeChangedToken);
    }
    else if (const auto rootAsParent = std::dynamic_pointer_cast<ParentPane>(_rootPane))
    {
        rootAsParent->ChildClosed(_rootPaneTypeChangedToken);
    }
}

// Method Description:
// - Register any event handlers that we may need with the given TermControl.
//   This should be called on each and every TermControl that we add to the tree
//   of Panes in this tab. We'll add events too:
//   * notify us when the control's title changed, so we can update our own
//     title (if necessary)
// Arguments:
// - control: the TermControl to add events to.
// Return Value:
// - <none>
void Tab::_AttachEventHandlersToControl(const TermControl& control)
{
    std::weak_ptr<Tab> weakThis{ shared_from_this() };

    control.TitleChanged([weakThis](auto newTitle) {
        // Check if Tab's lifetime has expired
        if (auto tab{ weakThis.lock() })
        {
            // The title of the control changed, but not necessarily the title of the tab.
            // Set the tab's text to the active panes' text.
            tab->SetTabText(tab->GetActiveTitle());
        }
    });

    // This is called when the terminal changes its font size or sets it for the first
    // time (because when we just create terminal via its ctor it has invalid font size).
    // On the latter event, we tell the root pane to resize itself so that its descendants
    // (including ourself) can properly snap to character grids. In future, we may also
    // want to do that on regular font changes.
    control.FontSizeChanged([this](const int /* fontWidth */,
                                   const int /* fontHeight */,
                                   const bool isInitialChange) {
        if (isInitialChange)
        {
            _rootPane->Relayout();
        }
    });
}

// Method Description:
// - Add an event handler to this pane's GotFocus event. When that pane gains
//   focus, we'll mark it as the new active pane. We'll also query the title of
//   that pane when it's focused to set our own text, and finally, we'll trigger
//   our own ActivePaneChanged event. This is to be called on every leaf pane in
//   the tree of panes in this tab.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Tab::_AttachEventHandlersToLeafPane(std::shared_ptr<LeafPane> pane)
{
    std::weak_ptr<Tab> weakThis{ shared_from_this() };

    pane->GotFocus([weakThis](std::shared_ptr<LeafPane> sender) {
        // Do nothing if the Tab's lifetime is expired or pane isn't new.
        auto tab{ weakThis.lock() };
        if (tab && sender != tab->_rootPane->FindActivePane())
        {
            // Clear the active state of the entire tree, and mark only the sender as active.
            tab->_rootPane->PropagateToLeaves([](LeafPane& pane) {
                pane.ClearActive();
            });

            sender->SetActive();

            // Update our own title text to match the newly-active pane.
            tab->SetTabText(tab->GetActiveTitle());

            // Raise our own ActivePaneChanged event.
            tab->_ActivePaneChangedHandlers();
        }
    });
}

DEFINE_EVENT(Tab, ActivePaneChanged, _ActivePaneChangedHandlers, winrt::delegate<>);
DEFINE_EVENT(Tab, RootPaneChanged, _RootPaneChangedHandlers, winrt::delegate<>);

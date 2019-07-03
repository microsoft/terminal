// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Tab.h"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;

static const int TabViewFontSize = 12;

Tab::Tab(const GUID& profile, const TermControl& control)
{
    _rootPane = std::make_shared<Pane>(profile, control, true);

    _rootPane->Closed([=]() {
        _closedHandlers();
    });

    _MakeTabViewItem();
}

void Tab::_MakeTabViewItem()
{
    _tabViewItem = ::winrt::Microsoft::UI::Xaml::Controls::TabViewItem{};
    _tabViewItem.FontSize(TabViewFontSize);
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
TermControl Tab::GetFocusedTerminalControl()
{
    return _rootPane->GetFocusedTerminalControl();
}

winrt::Microsoft::UI::Xaml::Controls::TabViewItem Tab::GetTabViewItem()
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
    return _rootPane->GetFocusedProfile();
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

    auto lastFocusedControl = _rootPane->GetFocusedTerminalControl();
    if (lastFocusedControl)
    {
        lastFocusedControl.GetControl().Focus(FocusState::Programmatic);
    }
}

// Method Description:
// - Update the focus state of this tab's tree of panes. If one of the controls
//   under this tab is focused, then it will be marked as the last focused. If
//   there are no focused panes, then there will not be a last focused control
//   when this returns.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Tab::UpdateFocus()
{
    _rootPane->UpdateFocus();
}

// Method Description:
// - Gets the title string of the last focused terminal control in our tree.
//   Returns the empty string if there is no such control.
// Arguments:
// - <none>
// Return Value:
// - the title string of the last focused terminal control in our tree.
winrt::hstring Tab::GetFocusedTitle() const
{
    const auto lastFocusedControl = _rootPane->GetFocusedTerminalControl();
    return lastFocusedControl ? lastFocusedControl.Title() : L"";
}

// Method Description:
// - Set the text on the TabViewItem for this tab.
// Arguments:
// - text: The new text string to use as the Header for our TabViewItem
// Return Value:
// - <none>
void Tab::SetTabText(const winrt::hstring& text)
{
    // Copy the hstring, so we don't capture a dead reference
    winrt::hstring textCopy{ text };
    _tabViewItem.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [text = std::move(textCopy), this]() {
        _tabViewItem.Header(winrt::box_value(text));
    });
}

// Method Description:
// - Move the viewport of the terminal up or down a number of lines. Negative
//      values of `delta` will move the view up, and positive values will move
//      the viewport down.
// Arguments:
// - delta: a number of lines to move the viewport relative to the current viewport.
// Return Value:
// - <none>
void Tab::Scroll(const int delta)
{
    auto control = GetFocusedTerminalControl();
    control.GetControl().Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [control, delta]() {
        const auto currentOffset = control.GetScrollOffset();
        control.KeyboardScrollViewport(currentOffset + delta);
    });
}

// Method Description:
// - Vertically split the focused pane in our tree of panes, and place the
//   given TermControl into the newly created pane.
// Arguments:
// - profile: The profile GUID to associate with the newly created pane.
// - control: A TermControl to use in the new pane.
// Return Value:
// - <none>
void Tab::AddVerticalSplit(const GUID& profile, TermControl& control)
{
    _rootPane->SplitVertical(profile, control);
}

// Method Description:
// - Horizontally split the focused pane in our tree of panes, and place the
//   given TermControl into the newly created pane.
// Arguments:
// - profile: The profile GUID to associate with the newly created pane.
// - control: A TermControl to use in the new pane.
// Return Value:
// - <none>
void Tab::AddHorizontalSplit(const GUID& profile, TermControl& control)
{
    _rootPane->SplitHorizontal(profile, control);
}

DEFINE_EVENT(Tab, Closed, _closedHandlers, ConnectionClosedEventArgs);

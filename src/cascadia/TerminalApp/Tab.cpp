// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Tab.h"
#include "Tab.g.cpp"
#include "Utils.h"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
}

namespace winrt::TerminalApp::implementation
{
    Tab::Tab(const GUID& profile, const TermControl& control)
    {
        _rootPane = std::make_shared<Pane>(profile, control, true);

        _rootPane->Closed([=](auto&& /*s*/, auto&& /*e*/) {
            _ClosedHandlers(nullptr, nullptr);
        });

        _activePane = _rootPane;

        _MakeTabViewItem();
    }

    // Method Description:
    // - Initializes a TabViewItem for this Tab instance.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_MakeTabViewItem()
    {
        _tabViewItem = ::winrt::MUX::Controls::TabViewItem{};
    }

    // Method Description:
    // - Get the root UIElement of this Tab's root pane.
    // Arguments:
    // - <none>
    // Return Value:
    // - The UIElement acting as root of the Tab's root pane.
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
        return _activePane->GetTerminalControl();
    }

    // Method Description:
    // - Gets the TabViewItem that represents this Tab
    // Arguments:
    // - <none>
    // Return Value:
    // - The TabViewItem that represents this Tab
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
        return _activePane->GetFocusedProfile();
    }

    // Method Description:
    // - Called after construction of a Tab object to bind event handlers to its
    //   associated Pane and TermControl object
    // Arguments:
    // - control: reference to the TermControl object to bind event to
    // Return Value:
    // - <none>
    void Tab::BindEventHandlers(const TermControl& control) noexcept
    {
        _AttachEventHandlersToPane(_rootPane);
        _AttachEventHandlersToControl(control);
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

    // Method Description:
    // - Set the icon on the TabViewItem for this tab.
    // Arguments:
    // - iconPath: The new path string to use as the IconPath for our TabViewItem
    // Return Value:
    // - <none>
    winrt::fire_and_forget Tab::UpdateIcon(const winrt::hstring iconPath)
    {
        // Don't reload our icon if it hasn't changed.
        if (iconPath == _lastIconPath)
        {
            return;
        }

        _lastIconPath = iconPath;

        auto weakThis{ get_weak() };

        co_await winrt::resume_foreground(_tabViewItem.Dispatcher());

        if (auto tab{ weakThis.get() })
        {
            IconPath(_lastIconPath);
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
        auto weakThis{ get_weak() };

        co_await winrt::resume_foreground(_tabViewItem.Dispatcher());

        if (auto tab{ weakThis.get() })
        {
            Title(text);
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
        control.ScrollViewport(currentOffset + delta);
    }

    // Method Description:
    // - Determines whether the focused pane has sufficient space to be split.
    // Arguments:
    // - splitType: The type of split we want to create.
    // Return Value:
    // - True if the focused pane can be split. False otherwise.
    bool Tab::CanSplitPane(winrt::TerminalApp::SplitState splitType)
    {
        return _activePane->CanSplit(splitType);
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
        auto [first, second] = _activePane->Split(splitType, profile, control);
        _activePane = first;
        _AttachEventHandlersToControl(control);

        // Add a event handlers to the new panes' GotFocus event. When the pane
        // gains focus, we'll mark it as the new active pane.
        _AttachEventHandlersToPane(first);
        _AttachEventHandlersToPane(second);

        // Immediately update our tracker of the focused pane now. If we're
        // splitting panes during startup (from a commandline), then it's
        // possible that the focus events won't propogate immediately. Updating
        // the focus here will give the same effect though.
        _UpdateActivePane(second);
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
    //   either size of the separator. See Pane::ResizePane for details.
    // Arguments:
    // - direction: The direction to move the separator in.
    // Return Value:
    // - <none>
    void Tab::ResizePane(const winrt::TerminalApp::Direction& direction)
    {
        // NOTE: This _must_ be called on the root pane, so that it can propagate
        // throughout the entire tree.
        _rootPane->ResizePane(direction);
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
        _rootPane->NavigateFocus(direction);
    }

    // Method Description:
    // - Prepares this tab for being removed from the UI hierarchy by shutting down all active connections.
    void Tab::Shutdown()
    {
        _rootPane->Shutdown();
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
        _activePane->Close();
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
        auto weakThis{ get_weak() };

        control.TitleChanged([weakThis](auto newTitle) {
            // Check if Tab's lifetime has expired
            if (auto tab{ weakThis.get() })
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
    // - Mark the given pane as the active pane in this tab. All other panes
    //   will be marked as inactive. We'll also update our own UI state to
    //   reflect this newly active pane.
    // Arguments:
    // - pane: a Pane to mark as active.
    // Return Value:
    // - <none>
    void Tab::_UpdateActivePane(std::shared_ptr<Pane> pane)
    {
        // Clear the active state of the entire tree, and mark only the pane as active.
        _rootPane->ClearActive();
        _activePane = pane;
        _activePane->SetActive();

        // Update our own title text to match the newly-active pane.
        SetTabText(GetActiveTitle());

        // Raise our own ActivePaneChanged event.
        _ActivePaneChangedHandlers();
    }

    // Method Description:
    // - Add an event handler to this pane's GotFocus event. When that pane gains
    //   focus, we'll mark it as the new active pane. We'll also query the title of
    //   that pane when it's focused to set our own text, and finally, we'll trigger
    //   our own ActivePaneChanged event.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_AttachEventHandlersToPane(std::shared_ptr<Pane> pane)
    {
        auto weakThis{ get_weak() };

        pane->GotFocus([weakThis](std::shared_ptr<Pane> sender) {
            // Do nothing if the Tab's lifetime is expired or pane isn't new.
            auto tab{ weakThis.get() };

            if (tab && sender != tab->_activePane)
            {
                tab->_UpdateActivePane(sender);
            }
        });
    }

    // Method Description:
    // - Get the total number of leaf panes in this tab. This will be the number
    //   of actual controls hosted by this tab.
    // Arguments:
    // - <none>
    // Return Value:
    // - The total number of leaf panes hosted by this tab.
    int Tab::_GetLeafPaneCount() const noexcept
    {
        return _rootPane->GetLeafPaneCount();
    }

    // Method Description:
    // - This is a helper to determine which direction an "Automatic" split should
    //   happen in for the active pane of this tab, but without using the ActualWidth() and
    //   ActualHeight() methods.
    // - See Pane::PreCalculateAutoSplit
    // Arguments:
    // - availableSpace: The theoretical space that's available for this Tab's content
    // Return Value:
    // - The SplitState that we should use for an `Automatic` split given
    //   `availableSpace`
    SplitState Tab::PreCalculateAutoSplit(winrt::Windows::Foundation::Size availableSpace) const
    {
        return _rootPane->PreCalculateAutoSplit(_activePane, availableSpace).value_or(SplitState::Vertical);
    }

    DEFINE_EVENT(Tab, ActivePaneChanged, _ActivePaneChangedHandlers, winrt::delegate<>);
}

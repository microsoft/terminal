// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include "ColorPickupFlyout.h"
#include "Tab.h"
#include "Tab.g.cpp"
#include "Utils.h"
#include "ColorHelper.h"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::System;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
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
        Content(_rootPane->GetRootElement());

        _MakeTabViewItem();
        _MakeSwitchToTabCommand();
        _CreateContextMenu();
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

        _tabViewItem.DoubleTapped([weakThis = get_weak()](auto&& /*s*/, auto&& /*e*/) {
            if (auto tab{ weakThis.get() })
            {
                tab->_inRename = true;
                tab->_UpdateTabHeader();
            }
        });

        _UpdateTitle();
        _RecalculateAndApplyTabColor();
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
    // - Called after construction of a Tab object to bind event handlers to its
    //   associated Pane and TermControl object
    // Arguments:
    // - control: reference to the TermControl object to bind event to
    // Return Value:
    // - <none>
    void Tab::Initialize(const TermControl& control)
    {
        _BindEventHandlers(control);
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
    void Tab::_BindEventHandlers(const TermControl& control) noexcept
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
            // The TabViewItem Icon needs MUX while the IconSourceElement in the CommandPalette needs WUX...
            IconSource(IconPathConverter::IconSourceWUX(_lastIconPath));
            _tabViewItem.IconSource(IconPathConverter::IconSourceMUX(_lastIconPath));

            // Update SwitchToTab command's icon
            SwitchToTabCommand().Icon(_lastIconPath);
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
        if (!_runtimeTabText.empty())
        {
            return _runtimeTabText;
        }
        const auto lastFocusedControl = GetActiveTerminalControl();
        return lastFocusedControl ? lastFocusedControl.Title() : L"";
    }

    // Method Description:
    // - Set the text on the TabViewItem for this tab, and bubbles the new title
    //   value up to anyone listening for changes to our title. Callers can
    //   listen for the title change with a PropertyChanged even handler.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    winrt::fire_and_forget Tab::_UpdateTitle()
    {
        auto weakThis{ get_weak() };
        co_await winrt::resume_foreground(_tabViewItem.Dispatcher());
        if (auto tab{ weakThis.get() })
        {
            // Bubble our current tab text to anyone who's listening for changes.
            Title(GetActiveTitle());

            // Update SwitchToTab command's name
            SwitchToTabCommand().Name(Title());

            // Update the UI to reflect the changed
            _UpdateTabHeader();
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
    bool Tab::CanSplitPane(SplitState splitType)
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
    void Tab::SplitPane(SplitState splitType, const GUID& profile, TermControl& control)
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
        // possible that the focus events won't propagate immediately. Updating
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
    void Tab::ResizePane(const Direction& direction)
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
    void Tab::NavigateFocus(const Direction& direction)
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

    void Tab::SetTabText(winrt::hstring title)
    {
        _runtimeTabText = title;
        _UpdateTitle();
    }

    void Tab::ResetTabText()
    {
        _runtimeTabText = L"";
        _UpdateTitle();
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
                tab->_UpdateTitle();
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

        control.TabColorChanged([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                // The control's tabColor changed, but it is not necessarily the
                // active control in this tab. We'll just recalculate the
                // current color anyways.
                tab->_RecalculateAndApplyTabColor();
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
        _UpdateTitle();

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
                tab->_RecalculateAndApplyTabColor();
            }
        });

        // Add a Closed event handler to the Pane. If the pane closes out from
        // underneath us, and it's zoomed, we want to be able to make sure to
        // update our state accordingly to un-zoom that pane. See GH#7252.
        pane->Closed([weakThis](auto&& /*s*/, auto && /*e*/) -> winrt::fire_and_forget {
            if (auto tab{ weakThis.get() })
            {
                if (tab->_zoomedPane)
                {
                    co_await winrt::resume_foreground(tab->Content().Dispatcher());

                    tab->Content(tab->_rootPane->GetRootElement());
                    tab->ExitZoom();
                }
            }
        });
    }

    // Method Description:
    // - Creates a context menu attached to the tab.
    // Currently contains elements allowing to select or
    // to close the current tab
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_CreateContextMenu()
    {
        auto weakThis{ get_weak() };

        // Close
        Controls::MenuFlyoutItem closeTabMenuItem;
        Controls::FontIcon closeSymbol;
        closeSymbol.FontFamily(Media::FontFamily{ L"Segoe MDL2 Assets" });
        closeSymbol.Glyph(L"\xE8BB");

        closeTabMenuItem.Click([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->_rootPane->Close();
            }
        });
        closeTabMenuItem.Text(RS_(L"TabClose"));
        closeTabMenuItem.Icon(closeSymbol);

        // "Color..."
        Controls::MenuFlyoutItem chooseColorMenuItem;
        Controls::FontIcon colorPickSymbol;
        colorPickSymbol.FontFamily(Media::FontFamily{ L"Segoe MDL2 Assets" });
        colorPickSymbol.Glyph(L"\xE790");

        chooseColorMenuItem.Click([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->ActivateColorPicker();
            }
        });
        chooseColorMenuItem.Text(RS_(L"TabColorChoose"));
        chooseColorMenuItem.Icon(colorPickSymbol);

        // Color Picker (it's convenient to have it here)
        _tabColorPickup.ColorSelected([weakThis](auto newTabColor) {
            if (auto tab{ weakThis.get() })
            {
                tab->SetRuntimeTabColor(newTabColor);
            }
        });

        _tabColorPickup.ColorCleared([weakThis]() {
            if (auto tab{ weakThis.get() })
            {
                tab->ResetRuntimeTabColor();
            }
        });

        Controls::MenuFlyoutItem renameTabMenuItem;
        {
            // "Rename Tab"
            Controls::FontIcon renameTabSymbol;
            renameTabSymbol.FontFamily(Media::FontFamily{ L"Segoe MDL2 Assets" });
            renameTabSymbol.Glyph(L"\xE932"); // Label

            renameTabMenuItem.Click([weakThis](auto&&, auto&&) {
                if (auto tab{ weakThis.get() })
                {
                    tab->_inRename = true;
                    tab->_UpdateTabHeader();
                }
            });
            renameTabMenuItem.Text(RS_(L"RenameTabText"));
            renameTabMenuItem.Icon(renameTabSymbol);
        }

        // Build the menu
        Controls::MenuFlyout newTabFlyout;
        Controls::MenuFlyoutSeparator menuSeparator;
        newTabFlyout.Items().Append(chooseColorMenuItem);
        newTabFlyout.Items().Append(renameTabMenuItem);
        newTabFlyout.Items().Append(menuSeparator);
        newTabFlyout.Items().Append(_CreateCloseSubMenu());
        newTabFlyout.Items().Append(closeTabMenuItem);
        _tabViewItem.ContextFlyout(newTabFlyout);
    }

    // Method Description:
    // - Creates a sub-menu containing menu items to close multiple tabs
    // Arguments:
    // - <none>
    // Return Value:
    // - the created MenuFlyoutSubItem
    Controls::MenuFlyoutSubItem Tab::_CreateCloseSubMenu()
    {
        auto weakThis{ get_weak() };

        // Close tabs after
        _closeTabsAfterMenuItem.Click([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->_CloseTabsAfter();
            }
        });
        _closeTabsAfterMenuItem.Text(RS_(L"TabCloseAfter"));

        // Close other tabs
        _closeOtherTabsMenuItem.Click([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->_CloseOtherTabs();
            }
        });
        _closeOtherTabsMenuItem.Text(RS_(L"TabCloseOther"));

        Controls::MenuFlyoutSubItem closeSubMenu;
        closeSubMenu.Text(RS_(L"TabCloseSubMenu"));
        closeSubMenu.Items().Append(_closeTabsAfterMenuItem);
        closeSubMenu.Items().Append(_closeOtherTabsMenuItem);

        return closeSubMenu;
    }

    // Method Description:
    // - Enable the Close menu items based on tab index and total number of tabs
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_EnableCloseMenuItems()
    {
        // close other tabs is enabled only if there are other tabs
        _closeOtherTabsMenuItem.IsEnabled(TabViewNumTabs() > 1);
        // close tabs after is enabled only if there are other tabs on the right
        _closeTabsAfterMenuItem.IsEnabled(TabViewIndex() < TabViewNumTabs() - 1);
    }

    // Method Description:
    // - This will update the contents of our TabViewItem for our current state.
    //   - If we're not in a rename, we'll set the Header of the TabViewItem to
    //     simply our current tab text (either the runtime tab text or the
    //     active terminal's text).
    //   - If we're in a rename, then we'll set the Header to a TextBox with the
    //     current tab text. The user can then use that TextBox to set a string
    //     to use as an override for the tab's text.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_UpdateTabHeader()
    {
        winrt::hstring tabText{ GetActiveTitle() };

        if (!_inRename)
        {
            if (_zoomedPane)
            {
                Controls::StackPanel sp;
                sp.Orientation(Controls::Orientation::Horizontal);
                Controls::FontIcon ico;
                ico.FontFamily(Media::FontFamily{ L"Segoe MDL2 Assets" });
                ico.Glyph(L"\xE8A3"); // "ZoomIn", a magnifying glass with a '+' in it.
                ico.FontSize(12);
                ico.Margin(ThicknessHelper::FromLengths(0, 0, 8, 0));
                sp.Children().Append(ico);
                Controls::TextBlock tb;
                tb.Text(tabText);
                sp.Children().Append(tb);

                _tabViewItem.Header(sp);
            }
            else
            {
                // If we're not currently in the process of renaming the tab,
                // then just set the tab's text to whatever our active title is.
                _tabViewItem.Header(winrt::box_value(tabText));
            }
        }
        else
        {
            _ConstructTabRenameBox(tabText);
        }
    }

    // Method Description:
    // - Create a new TextBox to use as the control for renaming the tab text.
    //   If the text box is already created, then this will do nothing, and
    //   leave the current box unmodified.
    // Arguments:
    // - tabText: This should be the text to initialize the rename text box with.
    // Return Value:
    // - <none>
    void Tab::_ConstructTabRenameBox(const winrt::hstring& tabText)
    {
        if (_tabViewItem.Header().try_as<Controls::TextBox>())
        {
            return;
        }

        Controls::TextBox tabTextBox;
        tabTextBox.Text(tabText);

        // The TextBox has a MinHeight already set by default, which is
        // larger than we want. Get rid of it.
        tabTextBox.MinHeight(0);
        // Also get rid of the internal padding on the text box, between the
        // border and the text content, on the top and bottom. This will
        // help the box fit within the bounds of the tab.
        Thickness internalPadding = ThicknessHelper::FromLengths(4, 0, 4, 0);
        tabTextBox.Padding(internalPadding);

        // Make the margin (0, -8, 0, -8), to counteract the padding that
        // the TabViewItem has.
        //
        // This is maybe a bit fragile, as the actual value might not be exactly
        // (0, 8, 0, 8), but using TabViewItemHeaderPadding to look up the real
        // value at runtime didn't work. So this is good enough for now.
        Thickness negativeMargins = ThicknessHelper::FromLengths(0, -8, 0, -8);
        tabTextBox.Margin(negativeMargins);

        // Set up some event handlers on the text box. We need three of them:
        // * A LostFocus event, so when the TextBox loses focus, we'll
        //   remove it and return to just the text on the tab.
        // * A KeyUp event, to be able to submit the tab text on Enter or
        //   dismiss the text box on Escape
        // * A LayoutUpdated event, so that we can auto-focus the text box
        //   when it's added to the tree.
        auto weakThis{ get_weak() };

        // When the text box loses focus, update the tab title of our tab.
        // - If there are any contents in the box, we'll use that value as
        //   the new "runtime text", which will override any text set by the
        //   application.
        // - If the text box is empty, we'll reset the "runtime text", and
        //   return to using the active terminal's title.
        tabTextBox.LostFocus([weakThis](const IInspectable& sender, auto&&) {
            auto tab{ weakThis.get() };
            auto textBox{ sender.try_as<Controls::TextBox>() };
            if (tab && textBox)
            {
                tab->_runtimeTabText = textBox.Text();
                tab->_inRename = false;
                tab->_UpdateTitle();
            }
        });

        // NOTE: (Preview)KeyDown does not work here. If you use that, we'll
        // remove the TextBox from the UI tree, then the following KeyUp
        // will bubble to the NewTabButton, which we don't want to have
        // happen.
        tabTextBox.KeyUp([weakThis](const IInspectable& sender, Input::KeyRoutedEventArgs const& e) {
            auto tab{ weakThis.get() };
            auto textBox{ sender.try_as<Controls::TextBox>() };
            if (tab && textBox)
            {
                switch (e.OriginalKey())
                {
                case VirtualKey::Enter:
                    tab->_runtimeTabText = textBox.Text();
                    [[fallthrough]];
                case VirtualKey::Escape:
                    e.Handled(true);
                    textBox.Text(tab->_runtimeTabText);
                    tab->_inRename = false;
                    tab->_UpdateTitle();
                    break;
                }
            }
        });

        // As soon as the text box is added to the UI tree, focus it. We can't focus it till it's in the tree.
        _tabRenameBoxLayoutUpdatedRevoker = tabTextBox.LayoutUpdated(winrt::auto_revoke, [this](auto&&, auto&&) {
            // Curiously, the sender for this event is null, so we have to
            // get the TextBox from the Tab's Header().
            auto textBox{ _tabViewItem.Header().try_as<Controls::TextBox>() };
            if (textBox)
            {
                textBox.SelectAll();
                textBox.Focus(FocusState::Programmatic);
            }
            // Only let this succeed once.
            _tabRenameBoxLayoutUpdatedRevoker.revoke();
        });

        _tabViewItem.Header(tabTextBox);
    }

    // Method Description:
    // Returns the tab color, if any
    // Arguments:
    // - <none>
    // Return Value:
    // - The tab's color, if any
    std::optional<winrt::Windows::UI::Color> Tab::GetTabColor()
    {
        const auto currControlColor{ GetActiveTerminalControl().TabColor() };
        std::optional<winrt::Windows::UI::Color> controlTabColor;
        if (currControlColor != nullptr)
        {
            controlTabColor = currControlColor.Value();
        }

        // A Tab's color will be the result of layering a variety of sources,
        // from the bottom up:
        //
        // Color                |             | Set by
        // -------------------- | --          | --
        // Runtime Color        | _optional_  | Color Picker / `setTabColor` action
        // Control Tab Color    | _optional_  | Profile's `tabColor`, or a color set by VT
        // Theme Tab Background | _optional_  | `tab.backgroundColor` in the theme
        // Tab Default Color    | **default** | TabView in XAML
        //
        // coalesce will get us the first of these values that's
        // actually set, with nullopt being our sentinel for "use the default
        // tabview color" (and clear out any colors we've set).

        return til::coalesce(_runtimeTabColor,
                             controlTabColor,
                             _themeTabColor,
                             std::optional<Windows::UI::Color>(std::nullopt));
    }

    // Method Description:
    // - Sets the runtime tab background color to the color chosen by the user
    // - Sets the tab foreground color depending on the luminance of
    // the background color
    // Arguments:
    // - color: the color the user picked for their tab
    // Return Value:
    // - <none>
    void Tab::SetRuntimeTabColor(const winrt::Windows::UI::Color& color)
    {
        _runtimeTabColor.emplace(color);
        _RecalculateAndApplyTabColor();
    }

    // Method Description:
    // - This function dispatches a function to the UI thread to recalculate
    //   what this tab's current background color should be. If a color is set,
    //   it will apply the given color to the tab's background. Otherwise, it
    //   will clear the tab's background color.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_RecalculateAndApplyTabColor()
    {
        auto weakThis{ get_weak() };

        _tabViewItem.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [weakThis]() {
            auto ptrTab = weakThis.get();
            if (!ptrTab)
                return;

            auto tab{ ptrTab };

            std::optional<winrt::Windows::UI::Color> currentColor = tab->GetTabColor();
            if (currentColor.has_value())
            {
                tab->_ApplyTabColor(currentColor.value());
            }
            else
            {
                tab->_ClearTabBackgroundColor();
            }
        });
    }

    // Method Description:
    // - Applies the given color to the background of this tab's TabViewItem.
    // - Sets the tab foreground color depending on the luminance of
    // the background color
    // - This method should only be called on the UI thread.
    // Arguments:
    // - color: the color the user picked for their tab
    // Return Value:
    // - <none>
    void Tab::_ApplyTabColor(const winrt::Windows::UI::Color& color)
    {
        Media::SolidColorBrush selectedTabBrush{};
        Media::SolidColorBrush deselectedTabBrush{};
        Media::SolidColorBrush fontBrush{};
        Media::SolidColorBrush hoverTabBrush{};
        // calculate the luminance of the current color and select a font
        // color based on that
        // see https://www.w3.org/TR/WCAG20/#relativeluminancedef
        if (TerminalApp::ColorHelper::IsBrightColor(color))
        {
            fontBrush.Color(winrt::Windows::UI::Colors::Black());
        }
        else
        {
            fontBrush.Color(winrt::Windows::UI::Colors::White());
        }

        hoverTabBrush.Color(TerminalApp::ColorHelper::GetAccentColor(color));
        selectedTabBrush.Color(color);

        // currently if a tab has a custom color, a deselected state is
        // signified by using the same color with a bit ot transparency
        auto deselectedTabColor = color;
        deselectedTabColor.A = 64;
        deselectedTabBrush.Color(deselectedTabColor);

        // currently if a tab has a custom color, a deselected state is
        // signified by using the same color with a bit ot transparency
        _tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackgroundSelected"), selectedTabBrush);
        _tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackground"), deselectedTabBrush);
        _tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackgroundPointerOver"), hoverTabBrush);
        _tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackgroundPressed"), selectedTabBrush);
        _tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderForeground"), fontBrush);
        _tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderForegroundSelected"), fontBrush);
        _tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderForegroundPointerOver"), fontBrush);
        _tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderForegroundPressed"), fontBrush);
        _tabViewItem.Resources().Insert(winrt::box_value(L"TabViewButtonForegroundActiveTab"), fontBrush);

        _RefreshVisualState();

        _colorSelected(color);
    }

    // Method Description:
    // - Clear the custom runtime color of the tab, if any color is set. This
    //   will re-apply whatever the tab's base color should be (either the color
    //   from the control, the theme, or the default tab color.)
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::ResetRuntimeTabColor()
    {
        _runtimeTabColor.reset();
        _RecalculateAndApplyTabColor();
    }

    // Method Description:
    // - Clear out any color we've set for the TabViewItem.
    // - This method should only be called on the UI thread.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_ClearTabBackgroundColor()
    {
        winrt::hstring keys[] = {
            L"TabViewItemHeaderBackground",
            L"TabViewItemHeaderBackgroundSelected",
            L"TabViewItemHeaderBackgroundPointerOver",
            L"TabViewItemHeaderForeground",
            L"TabViewItemHeaderForegroundSelected",
            L"TabViewItemHeaderForegroundPointerOver",
            L"TabViewItemHeaderBackgroundPressed",
            L"TabViewItemHeaderForegroundPressed",
            L"TabViewButtonForegroundActiveTab"
        };

        // simply clear any of the colors in the tab's dict
        for (auto keyString : keys)
        {
            auto key = winrt::box_value(keyString);
            if (_tabViewItem.Resources().HasKey(key))
            {
                _tabViewItem.Resources().Remove(key);
            }
        }

        _RefreshVisualState();
        _colorCleared();
    }

    // Method Description:
    // - Display the tab color picker at the location of the TabViewItem for this tab.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::ActivateColorPicker()
    {
        _tabColorPickup.ShowAt(_tabViewItem);
    }

    // Method Description:
    // Toggles the visual state of the tab view item,
    // so that changes to the tab color are reflected immediately
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_RefreshVisualState()
    {
        if (_focused)
        {
            VisualStateManager::GoToState(_tabViewItem, L"Normal", true);
            VisualStateManager::GoToState(_tabViewItem, L"Selected", true);
        }
        else
        {
            VisualStateManager::GoToState(_tabViewItem, L"Selected", true);
            VisualStateManager::GoToState(_tabViewItem, L"Normal", true);
        }
    }

    // - Get the total number of leaf panes in this tab. This will be the number
    //   of actual controls hosted by this tab.
    // Arguments:
    // - <none>
    // Return Value:
    // - The total number of leaf panes hosted by this tab.
    int Tab::GetLeafPaneCount() const noexcept
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

    bool Tab::PreCalculateCanSplit(SplitState splitType, winrt::Windows::Foundation::Size availableSpace) const
    {
        return _rootPane->PreCalculateCanSplit(_activePane, splitType, availableSpace).value_or(false);
    }

    // Method Description:
    // - Toggle our zoom state.
    //   * If we're not zoomed, then zoom the active pane, making it take the
    //     full size of the tab. We'll achieve this by changing our response to
    //     Tab::GetRootElement, so that it'll return the zoomed pane only.
    //   *  If we're currently zoomed on a pane, un-zoom that pane.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::ToggleZoom()
    {
        if (_zoomedPane)
        {
            ExitZoom();
        }
        else
        {
            EnterZoom();
        }
    }
    void Tab::EnterZoom()
    {
        _zoomedPane = _activePane;
        _rootPane->Maximize(_zoomedPane);
        // Update the tab header to show the magnifying glass
        _UpdateTabHeader();
        Content(_zoomedPane->GetRootElement());
    }
    void Tab::ExitZoom()
    {
        _rootPane->Restore(_zoomedPane);
        _zoomedPane = nullptr;
        // Update the tab header to hide the magnifying glass
        _UpdateTabHeader();
        Content(_rootPane->GetRootElement());
    }

    bool Tab::IsZoomed()
    {
        return _zoomedPane != nullptr;
    }

    // Method Description:
    // - Initializes a SwitchToTab command object for this Tab instance.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_MakeSwitchToTabCommand()
    {
        SwitchToTabArgs args{ _TabViewIndex };
        ActionAndArgs focusTabAction{ ShortcutAction::SwitchToTab, args };

        Command command;
        command.Action(focusTabAction);
        command.Name(Title());
        command.Icon(_lastIconPath);

        SwitchToTabCommand(command);
    }

    void Tab::_CloseTabsAfter()
    {
        CloseTabsAfterArgs args{ _TabViewIndex };
        ActionAndArgs closeTabsAfter{ ShortcutAction::CloseTabsAfter, args };

        _dispatch.DoAction(closeTabsAfter);
    }

    void Tab::_CloseOtherTabs()
    {
        CloseOtherTabsArgs args{ _TabViewIndex };
        ActionAndArgs closeOtherTabs{ ShortcutAction::CloseOtherTabs, args };

        _dispatch.DoAction(closeOtherTabs);
    }

    void Tab::UpdateTabViewIndex(const uint32_t idx, const uint32_t numTabs)
    {
        TabViewIndex(idx);
        TabViewNumTabs(numTabs);
        _EnableCloseMenuItems();
        SwitchToTabCommand().Action().Args().as<SwitchToTabArgs>().TabIndex(idx);
    }

    void Tab::SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch)
    {
        _dispatch = dispatch;
    }

    DEFINE_EVENT(Tab, ActivePaneChanged, _ActivePaneChangedHandlers, winrt::delegate<>);
    DEFINE_EVENT(Tab, ColorSelected, _colorSelected, winrt::delegate<winrt::Windows::UI::Color>);
    DEFINE_EVENT(Tab, ColorCleared, _colorCleared, winrt::delegate<>);
}

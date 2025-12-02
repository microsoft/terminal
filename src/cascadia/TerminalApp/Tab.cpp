// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include "ColorPickupFlyout.h"
#include "Tab.h"
#include "SettingsPaneContent.h"
#include "Tab.g.cpp"
#include "Utils.h"
#include "AppLogic.h"
#include "../../types/inc/ColorFix.hpp"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::System;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

#define ASSERT_UI_THREAD() assert(TabViewItem().Dispatcher().HasThreadAccess())

namespace winrt::TerminalApp::implementation
{
    Tab::Tab(std::shared_ptr<Pane> rootPane)
    {
        _rootPane = rootPane;
        _activePane = nullptr;

        _closePaneMenuItem.Visibility(WUX::Visibility::Collapsed);
        _restartConnectionMenuItem.Visibility(WUX::Visibility::Collapsed);

        auto firstId = _nextPaneId;

        _rootPane->WalkTree([&](const auto& pane) {
            // update the IDs on each pane
            if (pane->_IsLeaf())
            {
                pane->Id(_nextPaneId);
                _nextPaneId++;
            }
            // Try to find the pane marked active (if it exists)
            if (pane->_lastActive)
            {
                _activePane = pane;
            }
        });

        // In case none of the panes were already marked as the focus, just
        // focus the first one.
        if (_activePane == nullptr)
        {
            const auto firstPane = _rootPane->FindPane(firstId);
            firstPane->SetActive();
            _activePane = firstPane;
        }
        // If the focused pane is a leaf, add it to the MRU panes
        if (const auto id = _activePane->Id())
        {
            _mruPanes.insert(_mruPanes.begin(), id.value());
        }

        _Setup();
    }

    // Method Description:
    // - Shared setup for the constructors. Assumed that _rootPane has been set.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_Setup()
    {
        _rootClosedToken = _rootPane->Closed([=](auto&& /*s*/, auto&& /*e*/) {
            Closed.raise(nullptr, nullptr);
        });

        Content(_rootPane->GetRootElement());

        _MakeTabViewItem();
        _CreateContextMenu();

        _headerControl.TabStatus(_tabStatus);

        // Add an event handler for the header control to tell us when they want their title to change
        _headerControl.TitleChangeRequested([weakThis = get_weak()](auto&& title) {
            if (auto tab{ weakThis.get() })
            {
                tab->SetTabText(title);
            }
        });

        // GH#9162 - when the header is done renaming, ask for focus to be
        // tossed back to the control, rather into ourselves.
        _headerControl.RenameEnded([weakThis = get_weak()](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->RequestFocusActiveControl.raise();
            }
        });

        _UpdateHeaderControlMaxWidth();

        // Use our header control as the TabViewItem's header
        TabViewItem().Header(_headerControl);
    }

    // Method Description:
    // - Called when the timer for the bell indicator in the tab header fires
    // - Removes the bell indicator from the tab header
    // Arguments:
    // - sender, e: not used
    void Tab::_BellIndicatorTimerTick(const Windows::Foundation::IInspectable& /*sender*/, const Windows::Foundation::IInspectable& /*e*/)
    {
        ShowBellIndicator(false);
        _bellIndicatorTimer.Stop();
    }

    // Method Description:
    // - Initializes a TabViewItem for this Tab instance.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_MakeTabViewItem()
    {
        TabViewItem(::winrt::MUX::Controls::TabViewItem{});

        // GH#3609 If the tab was tapped, and no one else was around to handle
        // it, then ask our parent to toss focus into the active control.
        TabViewItem().Tapped([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->RequestFocusActiveControl.raise();
            }
        });

        // BODGY: When the tab is drag/dropped, the TabView gets a
        // TabDragStarting. However, the way it is implemented[^1], the
        // TabViewItem needs either an Item or a Content for the event to
        // include the correct TabViewItem. Otherwise, it will just return the
        // first TabViewItem in the TabView with the same Content as the dragged
        // tab (which, if the Content is null, will be the _first_ tab).
        //
        // So here, we'll stick an empty border in, just so that every tab has a
        // Content which is not equal to the others.
        //
        // [^1]: microsoft-ui-xaml/blob/92fbfcd55f05c92ac65569f5d284c5b36492091e/dev/TabView/TabView.cpp#L751-L758
        TabViewItem().Content(winrt::WUX::Controls::Border{});

        TabViewItem().DoubleTapped([weakThis = get_weak()](auto&& /*s*/, auto&& /*e*/) {
            if (auto tab{ weakThis.get() })
            {
                tab->ActivateTabRenamer();
            }
        });

        UpdateTitle();
        _RecalculateAndApplyTabColor();
    }

    void Tab::_UpdateHeaderControlMaxWidth()
    {
        try
        {
            // Make sure to try/catch this, because the LocalTests won't be
            // able to use this helper.
            const auto settings{ winrt::TerminalApp::implementation::AppLogic::CurrentAppSettings() };
            if (settings.GlobalSettings().TabWidthMode() == winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode::SizeToContent)
            {
                _headerControl.RenamerMaxWidth(HeaderRenameBoxWidthTitleLength);
            }
            else
            {
                _headerControl.RenamerMaxWidth(HeaderRenameBoxWidthDefault);
            }
        }
        CATCH_LOG()
    }

    void Tab::SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch)
    {
        ASSERT_UI_THREAD();

        _dispatch = dispatch;
    }

    void Tab::SetActionMap(const Microsoft::Terminal::Settings::Model::IActionMapView& actionMap)
    {
        ASSERT_UI_THREAD();

        _actionMap = actionMap;
        _UpdateSwitchToTabKeyChord();
    }

    // Method Description:
    // - Sets the key chord resulting in switch to the current tab.
    // Updates tool tip if required
    // Arguments:
    // - keyChord - string representation of the key chord that switches to the current tab
    // Return Value:
    // - <none>
    void Tab::_UpdateSwitchToTabKeyChord()
    {
        const auto id = fmt::format(FMT_COMPILE(L"Terminal.SwitchToTab{}"), _TabViewIndex);
        const auto keyChord{ _actionMap.GetKeyBindingForAction(id) };
        const auto keyChordText = keyChord ? KeyChordSerialization::ToString(keyChord) : L"";

        if (_keyChord == keyChordText)
        {
            return;
        }

        _keyChord = keyChordText;
        _UpdateToolTip();
    }

    // Method Description:
    // - Sets tab tool tip to a concatenation of title and key chord
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_UpdateToolTip()
    {
        auto titleRun = WUX::Documents::Run();
        titleRun.Text(_CreateToolTipTitle());

        auto textBlock = WUX::Controls::TextBlock{};
        textBlock.TextWrapping(WUX::TextWrapping::Wrap);
        textBlock.TextAlignment(WUX::TextAlignment::Center);
        textBlock.Inlines().Append(titleRun);

        if (!_keyChord.empty())
        {
            auto keyChordRun = WUX::Documents::Run();
            keyChordRun.Text(_keyChord);
            keyChordRun.FontStyle(winrt::Windows::UI::Text::FontStyle::Italic);
            textBlock.Inlines().Append(WUX::Documents::LineBreak{});
            textBlock.Inlines().Append(keyChordRun);
        }

        WUX::Controls::ToolTip toolTip{};
        toolTip.Content(textBlock);
        WUX::Controls::ToolTipService::SetToolTip(TabViewItem(), toolTip);
    }

    // Method Description:
    // - Returns nullptr if no children of this tab were the last control to be
    //   focused, the active control of the current pane, or the last active child control
    //   of the active pane if it is a parent.
    // - This control might not currently be focused, if the tab itself is not
    //   currently focused.
    // Arguments:
    // - <none>
    // Return Value:
    // - nullptr if no children were marked `_lastFocused`, else the TermControl
    //   that was last focused.
    TermControl Tab::GetActiveTerminalControl() const
    {
        ASSERT_UI_THREAD();

        if (_activePane)
        {
            return _activePane->GetLastFocusedTerminalControl();
        }
        return nullptr;
    }

    IPaneContent Tab::GetActiveContent() const
    {
        return _activePane ? _activePane->GetContent() : nullptr;
    }

    // Method Description:
    // - Called after construction of a Tab object to bind event handlers to its
    //   associated Pane and TermControl objects
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::Initialize()
    {
        ASSERT_UI_THREAD();

        _rootPane->WalkTree([&](const auto& pane) {
            // Attach event handlers to each new pane
            _AttachEventHandlersToPane(pane);
            if (auto content = pane->GetContent())
            {
                _AttachEventHandlersToContent(pane->Id().value(), content);
            }
        });
    }

    // Method Description:
    // - Updates our focus state. If we're gaining focus, make sure to transfer
    //   focus to the last focused terminal control in our tree of controls.
    // Arguments:
    // - focused: our new focus state
    // Return Value:
    // - <none>
    void Tab::Focus(WUX::FocusState focusState)
    {
        ASSERT_UI_THREAD();

        _focusState = focusState;

        if (_focused())
        {
            auto lastFocusedControl = GetActiveTerminalControl();
            if (lastFocusedControl)
            {
                lastFocusedControl.Focus(_focusState);

                // Update our own progress state. This will fire an event signaling
                // that our taskbar progress changed.
                _UpdateProgressState();
            }
            // When we gain focus, remove the bell indicator if it is active
            if (_tabStatus.BellIndicator())
            {
                ShowBellIndicator(false);
            }
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
    Profile Tab::GetFocusedProfile() const noexcept
    {
        ASSERT_UI_THREAD();

        return _activePane->GetFocusedProfile();
    }

    // Method Description:
    // - Attempts to update the settings that apply to this tab.
    // - Panes are handled elsewhere, by somebody who can establish broader knowledge
    //   of the settings that apply to all tabs.
    // Return Value:
    // - <none>
    void Tab::UpdateSettings(const CascadiaSettings& settings)
    {
        ASSERT_UI_THREAD();

        // The tabWidthMode may have changed, update the header control accordingly
        _UpdateHeaderControlMaxWidth();

        // Update the settings on all our panes.
        _rootPane->WalkTree([&](const auto& pane) {
            pane->UpdateSettings(settings);
            return false;
        });
    }

    // Method Description:
    // - Set the icon on the TabViewItem for this tab.
    // Arguments:
    // - iconPath: The new path string to use as the IconPath for our TabViewItem
    // Return Value:
    // - <none>
    void Tab::UpdateIcon(const winrt::hstring& iconPath, const winrt::Microsoft::Terminal::Settings::Model::IconStyle iconStyle)
    {
        ASSERT_UI_THREAD();

        // Don't reload our icon and iconStyle hasn't changed.
        if (iconPath == _lastIconPath && iconStyle == _lastIconStyle)
        {
            return;
        }
        _lastIconPath = iconPath;
        _lastIconStyle = iconStyle;

        // If the icon is currently hidden, just return here (but only after setting _lastIconPath to the new path
        // for when we show the icon again)
        if (_iconHidden)
        {
            return;
        }

        if (iconStyle == IconStyle::Hidden)
        {
            // The TabViewItem Icon needs MUX while the IconSourceElement in the CommandPalette needs WUX...
            Icon({});
            TabViewItem().IconSource(IconSource{ nullptr });
        }
        else
        {
            Icon(_lastIconPath);
            bool isMonochrome = iconStyle == IconStyle::Monochrome;
            TabViewItem().IconSource(Microsoft::Terminal::UI::IconPathConverter::IconSourceMUX(_lastIconPath, isMonochrome));
        }
    }

    // Method Description:
    // - Hide or show the tab icon for this tab
    // - Used when we want to show the progress ring, which should replace the icon
    // Arguments:
    // - hide: if true, we hide the icon; if false, we show the icon
    void Tab::HideIcon(const bool hide)
    {
        ASSERT_UI_THREAD();

        if (_iconHidden != hide)
        {
            if (hide)
            {
                Icon({});
                TabViewItem().IconSource(IconSource{ nullptr });
            }
            else
            {
                Icon(_lastIconPath);
                TabViewItem().IconSource(Microsoft::Terminal::UI::IconPathConverter::IconSourceMUX(_lastIconPath, _lastIconStyle == IconStyle::Monochrome));
            }
            _iconHidden = hide;
        }
    }

    // Method Description:
    // - Hide or show the bell indicator in the tab header
    // Arguments:
    // - show: if true, we show the indicator; if false, we hide the indicator
    void Tab::ShowBellIndicator(const bool show)
    {
        ASSERT_UI_THREAD();

        _tabStatus.BellIndicator(show);
    }

    // Method Description:
    // - Activates the timer for the bell indicator in the tab
    // - Called if a bell raised when the tab already has focus
    void Tab::ActivateBellIndicatorTimer()
    {
        ASSERT_UI_THREAD();

        if (!_bellIndicatorTimer)
        {
            _bellIndicatorTimer.Interval(std::chrono::milliseconds(2000));
            _bellIndicatorTimer.Tick({ get_weak(), &Tab::_BellIndicatorTimerTick });
        }

        _bellIndicatorTimer.Start();
    }

    // Method Description:
    // - Gets the title string of the last focused terminal control in our tree.
    //   Returns the empty string if there is no such control.
    // Arguments:
    // - <none>
    // Return Value:
    // - the title string of the last focused terminal control in our tree.
    winrt::hstring Tab::_GetActiveTitle() const
    {
        if (!_runtimeTabText.empty())
        {
            return _runtimeTabText;
        }
        if (!_activePane->_IsLeaf())
        {
            return RS_(L"MultiplePanes");
        }
        const auto activeContent = GetActiveContent();
        return activeContent ? activeContent.Title() : winrt::hstring{};
    }

    // Method Description:
    // - Set the text on the TabViewItem for this tab, and bubbles the new title
    //   value up to anyone listening for changes to our title. Callers can
    //   listen for the title change with a PropertyChanged even handler.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::UpdateTitle()
    {
        ASSERT_UI_THREAD();

        const auto activeTitle = _GetActiveTitle();
        // Bubble our current tab text to anyone who's listening for changes.
        Title(activeTitle);

        // Update the control to reflect the changed title
        _headerControl.Title(activeTitle);
        Automation::AutomationProperties::SetName(TabViewItem(), activeTitle);
        _UpdateToolTip();
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
        ASSERT_UI_THREAD();

        auto control = GetActiveTerminalControl();
        const auto currentOffset = control.ScrollOffset();
        control.ScrollViewport(::base::ClampAdd(currentOffset, delta));
    }

    // Method Description:
    // - Serializes the state of this tab as a series of commands that can be
    //   executed to recreate it.
    // Arguments:
    // - <none>
    // Return Value:
    // - A vector of commands
    std::vector<ActionAndArgs> Tab::BuildStartupActions(BuildStartupKind kind) const
    {
        ASSERT_UI_THREAD();

        // Give initial ids (0 for the child created with this tab,
        // 1 for the child after the first split.
        auto state = _rootPane->BuildStartupActions(0, 1, kind);

        {
            ActionAndArgs newTabAction{};
            INewContentArgs newContentArgs{ state.firstPane->GetTerminalArgsForPane(kind) };

            // Special case here: if there was one pane (which results in no actions
            // being generated), and it was a settings pane, then promote that to an
            // open settings action. The openSettings action itself has additional machinery
            // to prevent multiple top-level settings tabs.
            const auto wasSettings = state.args.empty() &&
                                     (newContentArgs && newContentArgs.Type() == L"settings");
            if (wasSettings)
            {
                newTabAction.Action(ShortcutAction::OpenSettings);
                newTabAction.Args(OpenSettingsArgs{ SettingsTarget::SettingsUI });
                return std::vector<ActionAndArgs>{ std::move(newTabAction) };
            }

            newTabAction.Action(ShortcutAction::NewTab);
            newTabAction.Args(NewTabArgs{ newContentArgs });

            state.args.emplace(state.args.begin(), std::move(newTabAction));
        }

        if (_runtimeTabColor)
        {
            ActionAndArgs setColorAction{};
            setColorAction.Action(ShortcutAction::SetTabColor);

            SetTabColorArgs setColorArgs{ _runtimeTabColor.value() };
            setColorAction.Args(setColorArgs);

            state.args.emplace_back(std::move(setColorAction));
        }

        if (!_runtimeTabText.empty())
        {
            ActionAndArgs renameTabAction{};
            renameTabAction.Action(ShortcutAction::RenameTab);

            RenameTabArgs renameTabArgs{ _runtimeTabText };
            renameTabAction.Args(renameTabArgs);

            state.args.emplace_back(std::move(renameTabAction));
        }

        // If we only have one arg, we only have 1 pane so we don't need any
        // special focus logic
        if (state.args.size() > 1 && state.focusedPaneId.has_value())
        {
            ActionAndArgs focusPaneAction{};
            focusPaneAction.Action(ShortcutAction::FocusPane);
            FocusPaneArgs focusArgs{ state.focusedPaneId.value() };
            focusPaneAction.Args(focusArgs);

            state.args.emplace_back(std::move(focusPaneAction));
        }

        if (_zoomedPane)
        {
            // we start without any panes zoomed so toggle zoom will enable zoom.
            ActionAndArgs zoomPaneAction{};
            zoomPaneAction.Action(ShortcutAction::TogglePaneZoom);

            state.args.emplace_back(std::move(zoomPaneAction));
        }

        return state.args;
    }

    // Method Description:
    // - Split the focused pane in our tree of panes, and place the
    //   given pane into the tree of panes according to the split
    // Arguments:
    // - splitType: The type of split we want to create
    // - splitSize: The size of the split we want to create
    // - pane: The new pane to add to the tree of panes; note that this pane
    //         could itself be a parent pane/the root node of a tree of panes
    // Return Value:
    // - a pair of (the Pane that now holds the original content, the new Pane in the tree)
    std::pair<std::shared_ptr<Pane>, std::shared_ptr<Pane>> Tab::SplitPane(SplitDirection splitType,
                                                                           const float splitSize,
                                                                           std::shared_ptr<Pane> pane)
    {
        ASSERT_UI_THREAD();

        // Add the new event handlers to the new pane(s)
        // and update their ids.
        pane->WalkTree([&](const auto& p) {
            _AttachEventHandlersToPane(p);
            if (p->_IsLeaf())
            {
                p->Id(_nextPaneId);
                if (const auto& content{ p->GetContent() })
                {
                    _AttachEventHandlersToContent(p->Id().value(), content);
                }
                _nextPaneId++;
            }
            return false;
        });
        pane->EnableBroadcast(_tabStatus.IsInputBroadcastActive());

        // Make sure to take the ID before calling Split() - Split() will clear out the active pane's ID
        const auto activePaneId = _activePane->Id();
        // Depending on which direction will be split, the new pane can be
        // either the first or second child, but this will always return the
        // original pane first.
        auto [original, newPane] = _activePane->Split(splitType, splitSize, pane);

        // After split, Close Pane Menu Item should be visible
        _closePaneMenuItem.Visibility(WUX::Visibility::Visible);

        // The active pane has an id if it is a leaf
        if (activePaneId)
        {
            original->Id(activePaneId.value());
        }

        _activePane = original;

        // Add a event handlers to the new panes' GotFocus event. When the pane
        // gains focus, we'll mark it as the new active pane.
        _AttachEventHandlersToPane(original);

        // Immediately update our tracker of the focused pane now. If we're
        // splitting panes during startup (from a commandline), then it's
        // possible that the focus events won't propagate immediately. Updating
        // the focus here will give the same effect though.
        _UpdateActivePane(newPane);

        return { original, newPane };
    }

    // Method Description:
    // - Removes the currently active pane from this tab. If that was the only
    //   remaining pane, then the entire tab is closed as well.
    // Arguments:
    // - <none>
    // Return Value:
    // - The removed pane, if the remove succeeded.
    std::shared_ptr<Pane> Tab::DetachPane()
    {
        ASSERT_UI_THREAD();

        // if we only have one pane, or the focused pane is the root, remove it
        // entirely and close this tab
        if (_rootPane == _activePane)
        {
            return DetachRoot();
        }

        // Attempt to remove the active pane from the tree
        if (const auto pane = _rootPane->DetachPane(_activePane))
        {
            // Just make sure that the remaining pane is marked active
            _UpdateActivePane(_rootPane->GetActivePane());

            return pane;
        }

        return nullptr;
    }

    // Method Description:
    // - Closes this tab and returns the root pane to be used elsewhere.
    // Arguments:
    // - <none>
    // Return Value:
    // - The root pane.
    std::shared_ptr<Pane> Tab::DetachRoot()
    {
        ASSERT_UI_THREAD();

        // remove the closed event handler since we are closing the tab
        // manually.
        _rootPane->Closed(_rootClosedToken);
        auto p = _rootPane;
        p->WalkTree([](const auto& pane) {
            pane->Detached.raise(pane);
        });

        // Clean up references and close the tab
        _rootPane = nullptr;
        _activePane = nullptr;
        Content(nullptr);
        Closed.raise(nullptr, nullptr);

        return p;
    }

    // Method Description:
    // - Add an arbitrary pane to this tab. This will be added as a split on the
    //   currently active pane.
    // Arguments:
    // - pane: The pane to add.
    // Return Value:
    // - <none>
    void Tab::AttachPane(std::shared_ptr<Pane> pane)
    {
        ASSERT_UI_THREAD();

        // Add the new event handlers to the new pane(s)
        // and update their ids.
        pane->WalkTree([&](const auto& p) {
            _AttachEventHandlersToPane(p);
            if (p->_IsLeaf())
            {
                p->Id(_nextPaneId);

                if (const auto& content{ p->GetContent() })
                {
                    _AttachEventHandlersToContent(p->Id().value(), content);
                }
                _nextPaneId++;
            }
        });
        pane->EnableBroadcast(_tabStatus.IsInputBroadcastActive());
        // pass the old id to the new child
        const auto previousId = _activePane->Id();

        // Add the new pane as an automatic split on the active pane.
        auto first = _activePane->AttachPane(pane, SplitDirection::Automatic);

        // This will be true if the original _activePane is a leaf pane.
        // If it is a parent pane then we don't want to set an ID on it.
        if (previousId)
        {
            first->Id(previousId.value());
        }

        // Update with event handlers on the new child.
        _activePane = first;
        _AttachEventHandlersToPane(first);

        // Make sure that we have the right pane set as the active pane
        if (const auto focus = pane->GetActivePane())
        {
            _UpdateActivePane(focus);
        }
    }

    // Method Description:
    // - Attaches the given color picker to ourselves
    // - Typically will be called after we have sent a request for the color picker
    // Arguments:
    // - colorPicker: The color picker that we should attach to ourselves
    // Return Value:
    // - <none>
    void Tab::AttachColorPicker(TerminalApp::ColorPickupFlyout& colorPicker)
    {
        ASSERT_UI_THREAD();

        auto weakThis{ get_weak() };

        _tabColorPickup = colorPicker;

        _colorSelectedToken = _tabColorPickup.ColorSelected([weakThis](auto newTabColor) {
            if (auto tab{ weakThis.get() })
            {
                tab->SetRuntimeTabColor(newTabColor);
            }
        });

        _colorClearedToken = _tabColorPickup.ColorCleared([weakThis]() {
            if (auto tab{ weakThis.get() })
            {
                tab->ResetRuntimeTabColor();
            }
        });

        _pickerClosedToken = _tabColorPickup.Closed([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->_tabColorPickup.ColorSelected(tab->_colorSelectedToken);
                tab->_tabColorPickup.ColorCleared(tab->_colorClearedToken);
                tab->_tabColorPickup.Closed(tab->_pickerClosedToken);
                tab->_tabColorPickup = nullptr;
            }
        });

        _tabColorPickup.ShowAt(TabViewItem());
    }

    // Method Description:
    // - Find the currently active pane, and then switch the split direction of
    //   its parent. E.g. switch from Horizontal to Vertical.
    // Return Value:
    // - <none>
    void Tab::ToggleSplitOrientation()
    {
        ASSERT_UI_THREAD();

        _rootPane->ToggleSplitOrientation();
    }

    // Method Description:
    // - See Pane::CalcSnappedDimension
    float Tab::CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
    {
        ASSERT_UI_THREAD();

        return _rootPane->CalcSnappedDimension(widthOrHeight, dimension);
    }

    // Method Description:
    // - Attempt to move a separator between panes, as to resize each child on
    //   either size of the separator. See Pane::ResizePane for details.
    // Arguments:
    // - direction: The direction to move the separator in.
    // Return Value:
    // - <none>
    void Tab::ResizePane(const ResizeDirection& direction)
    {
        ASSERT_UI_THREAD();

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
    // - Whether changing the focus succeeded. This allows a keychord to propagate
    //   to the terminal when no other panes are present (GH#6219)
    bool Tab::NavigateFocus(const FocusDirection& direction)
    {
        ASSERT_UI_THREAD();

        // NOTE: This _must_ be called on the root pane, so that it can propagate
        // throughout the entire tree.
        if (const auto newFocus = _rootPane->NavigateDirection(_activePane, direction, _mruPanes))
        {
            // Mark that we want the active pane to changed
            _changingActivePane = true;
            const auto res = _rootPane->FocusPane(newFocus);
            _changingActivePane = false;

            if (_zoomedPane)
            {
                UpdateZoom(newFocus);
            }

            return res;
        }

        return false;
    }

    // Method Description:
    // - Attempts to swap the location of the focused pane with another pane
    //   according to direction. When there are multiple adjacent panes it will
    //   select the first one (top-left-most).
    // Arguments:
    // - direction: The direction to move the pane in.
    // Return Value:
    // - true if two panes were swapped.
    bool Tab::SwapPane(const FocusDirection& direction)
    {
        ASSERT_UI_THREAD();

        // You cannot swap panes with the parent/child pane because of the
        // circular reference.
        if (direction == FocusDirection::Parent || direction == FocusDirection::Child)
        {
            return false;
        }
        // NOTE: This _must_ be called on the root pane, so that it can propagate
        // throughout the entire tree.
        if (auto neighbor = _rootPane->NavigateDirection(_activePane, direction, _mruPanes))
        {
            // SwapPanes will refocus the terminal to make sure that it has focus
            // even after moving.
            _changingActivePane = true;
            const auto res = _rootPane->SwapPanes(_activePane, neighbor);
            _changingActivePane = false;
            return res;
        }

        return false;
    }

    bool Tab::FocusPane(const uint32_t id)
    {
        ASSERT_UI_THREAD();

        if (_rootPane == nullptr)
        {
            return false;
        }
        _changingActivePane = true;
        const auto res = _rootPane->FocusPane(id);
        _changingActivePane = false;
        return res;
    }

    // Method Description:
    // - Prepares this tab for being removed from the UI hierarchy by shutting down all active connections.
    void Tab::Shutdown()
    {
        ASSERT_UI_THREAD();

        // NOTE: `TerminalPage::_HandleCloseTabRequested` relies on the content being null after this call.
        Content(nullptr);

        if (_rootPane)
        {
            _rootPane->Shutdown();
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
        ASSERT_UI_THREAD();

        _activePane->Close();
    }

    void Tab::SetTabText(winrt::hstring title)
    {
        ASSERT_UI_THREAD();

        _runtimeTabText = title;
        UpdateTitle();
    }

    winrt::hstring Tab::GetTabText() const
    {
        ASSERT_UI_THREAD();

        return _runtimeTabText;
    }

    void Tab::ResetTabText()
    {
        ASSERT_UI_THREAD();

        _runtimeTabText = L"";
        UpdateTitle();
    }

    // Method Description:
    // - Show a TextBox in the Header to allow the user to set a string
    //     to use as an override for the tab's text
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::ActivateTabRenamer()
    {
        ASSERT_UI_THREAD();

        _headerControl.BeginRename();
    }

    // Method Description:
    // - Removes any event handlers set by the tab on the given pane's control.
    //   The pane's ID is the most stable identifier for a given control, because
    //   the control itself doesn't have a particular ID and its pointer is
    //   unstable since it is moved when panes split.
    // Arguments:
    // - paneId: The ID of the pane that contains the given content.
    // Return Value:
    // - <none>
    void Tab::_DetachEventHandlersFromContent(const uint32_t paneId)
    {
        auto it = _contentEvents.find(paneId);
        if (it != _contentEvents.end())
        {
            // revoke the event handlers by resetting the event struct
            // and remove it from the map
            _contentEvents.erase(paneId);
        }
    }

    // Method Description:
    // - Register any event handlers that we may need with the given TermControl.
    //   This should be called on each and every TermControl that we add to the tree
    //   of Panes in this tab. We'll add events too:
    //   * notify us when the control's title changed, so we can update our own
    //     title (if necessary)
    // Arguments:
    // - paneId: the ID of the pane that this control belongs to.
    // - control: the TermControl to add events to.
    // Return Value:
    // - <none>
    void Tab::_AttachEventHandlersToContent(const uint32_t paneId, const TerminalApp::IPaneContent& content)
    {
        auto weakThis{ get_weak() };
        auto dispatcher = DispatcherQueue::GetForCurrentThread();
        ContentEventTokens events{};

        auto throttledTitleChanged = std::make_shared<ThrottledFunc<>>(
            dispatcher,
            til::throttled_func_options{
                .delay = std::chrono::milliseconds{ 200 },
                .leading = true,
                .trailing = true,
            },
            [weakThis]() {
                if (const auto tab = weakThis.get())
                {
                    tab->UpdateTitle();
                }
            });

        events.TitleChanged = content.TitleChanged(
            winrt::auto_revoke,
            [func = std::move(throttledTitleChanged)](auto&&, auto&&) {
                func->Run();
            });

        auto throttledTaskbarProgressChanged = std::make_shared<ThrottledFunc<>>(
            dispatcher,
            til::throttled_func_options{
                .delay = std::chrono::milliseconds{ 200 },
                .trailing = true,
            },
            [weakThis]() {
                if (const auto tab = weakThis.get())
                {
                    tab->_UpdateProgressState();
                }
            });

        events.TaskbarProgressChanged = content.TaskbarProgressChanged(
            winrt::auto_revoke,
            [func = std::move(throttledTaskbarProgressChanged)](auto&&, auto&&) {
                func->Run();
            });

        events.TabColorChanged = content.TabColorChanged(
            winrt::auto_revoke,
            [dispatcher, weakThis](auto&&, auto&&) -> safe_void_coroutine {
                const auto weakThisCopy = weakThis;
                co_await wil::resume_foreground(dispatcher);
                if (auto tab{ weakThisCopy.get() })
                {
                    // The control's tabColor changed, but it is not necessarily the
                    // active control in this tab. We'll just recalculate the
                    // current color anyways.
                    tab->_RecalculateAndApplyTabColor();
                    tab->_tabStatus.TabColorIndicator(tab->GetTabColor().value_or(Windows::UI::Colors::Transparent()));
                }
            });

        events.ConnectionStateChanged = content.ConnectionStateChanged(
            winrt::auto_revoke,
            [dispatcher, weakThis](auto&&, auto&&) -> safe_void_coroutine {
                const auto weakThisCopy = weakThis;
                co_await wil::resume_foreground(dispatcher);
                if (auto tab{ weakThisCopy.get() })
                {
                    tab->_UpdateConnectionClosedState();
                }
            });

        events.ReadOnlyChanged = content.ReadOnlyChanged(
            winrt::auto_revoke,
            [dispatcher, weakThis](auto&&, auto&&) -> safe_void_coroutine {
                const auto weakThisCopy = weakThis;
                co_await wil::resume_foreground(dispatcher);
                if (auto tab{ weakThis.get() })
                {
                    tab->_RecalculateAndApplyReadOnly();
                }
            });

        events.FocusRequested = content.FocusRequested(
            winrt::auto_revoke,
            [dispatcher, weakThis](TerminalApp::IPaneContent sender, auto) -> safe_void_coroutine {
                const auto weakThisCopy = weakThis;
                co_await wil::resume_foreground(dispatcher);
                if (const auto tab{ weakThisCopy.get() })
                {
                    if (tab->_focused())
                    {
                        sender.Focus(FocusState::Pointer);
                    }
                }
            });

        events.BellRequested = content.BellRequested(
            winrt::auto_revoke,
            [dispatcher, weakThis](TerminalApp::IPaneContent sender, auto bellArgs) -> safe_void_coroutine {
                const auto weakThisCopy = weakThis;
                co_await wil::resume_foreground(dispatcher);
                if (const auto tab{ weakThisCopy.get() })
                {
                    if (bellArgs.FlashTaskbar())
                    {
                        // If visual is set, we need to bubble this event all the way to app host to flash the taskbar
                        // In this part of the chain we bubble it from the hosting tab to the page
                        tab->TabRaiseVisualBell.raise();
                    }

                    // Show the bell indicator in the tab header
                    tab->ShowBellIndicator(true);

                    // If this tab is focused, activate the bell indicator timer, which will
                    // remove the bell indicator once it fires
                    // (otherwise, the indicator is removed when the tab gets focus)
                    if (tab->_focusState != WUX::FocusState::Unfocused)
                    {
                        tab->ActivateBellIndicatorTimer();
                    }
                }
            });

        if (const auto& terminal{ content.try_as<TerminalApp::TerminalPaneContent>() })
        {
            events.RestartTerminalRequested = terminal.RestartTerminalRequested(winrt::auto_revoke, { get_weak(), &Tab::_bubbleRestartTerminalRequested });
        }

        if (_tabStatus.IsInputBroadcastActive())
        {
            if (const auto& termContent{ content.try_as<TerminalApp::TerminalPaneContent>() })
            {
                _addBroadcastHandlers(termContent.GetTermControl(), events);
            }
        }

        _contentEvents[paneId] = std::move(events);
    }

    // Method Description:
    // - Get the combined taskbar state for the tab. This is the combination of
    //   all the states of all our panes. Taskbar states are given a priority
    //   based on the rules in:
    //   https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itaskbarlist3-setprogressstate
    //   under "How the Taskbar Button Chooses the Progress Indicator for a
    //   Group"
    // Arguments:
    // - <none>
    // Return Value:
    // - A TaskbarState object representing the combined taskbar state and
    //   progress percentage of all our panes.
    winrt::TerminalApp::TaskbarState Tab::GetCombinedTaskbarState() const
    {
        ASSERT_UI_THREAD();

        std::vector<winrt::TerminalApp::TaskbarState> states;
        if (_rootPane)
        {
            _rootPane->CollectTaskbarStates(states);
        }
        return states.empty() ? winrt::make<winrt::TerminalApp::implementation::TaskbarState>() :
                                *std::min_element(states.begin(), states.end(), TerminalApp::implementation::TaskbarState::ComparePriority);
    }

    // Method Description:
    // - This should be called on the UI thread. If you don't, then it might
    //   silently do nothing.
    // - Update our TabStatus to reflect the progress state of the currently
    //   active pane.
    // - This is called every time _any_ control's progress state changes,
    //   regardless of if that control is the active one or not. This is simpler
    //   then re-attaching this handler to the active control each time it
    //   changes.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_UpdateProgressState()
    {
        const auto state{ GetCombinedTaskbarState() };

        const auto taskbarState = state.State();
        // The progress of the control changed, but not necessarily the progress of the tab.
        // Set the tab's progress ring to the active pane's progress
        if (taskbarState > 0)
        {
            if (taskbarState == 3)
            {
                // 3 is the indeterminate state, set the progress ring as such
                _tabStatus.IsProgressRingIndeterminate(true);
            }
            else
            {
                // any non-indeterminate state has a value, set the progress ring as such
                _tabStatus.IsProgressRingIndeterminate(false);

                const auto progressValue = gsl::narrow<uint32_t>(state.Progress());
                _tabStatus.ProgressValue(progressValue);
            }
            // Hide the tab icon (the progress ring is placed over it)
            HideIcon(true);
            _tabStatus.IsProgressRingActive(true);
        }
        else
        {
            // Show the tab icon
            HideIcon(false);
            _tabStatus.IsProgressRingActive(false);
        }

        // fire an event signaling that our taskbar progress changed.
        TaskbarProgressChanged.raise(nullptr, nullptr);
    }

    // Method Description:
    // - Set an indicator on the tab if any pane is in a closed connection state.
    // - Show/hide the Restart Connection context menu entry depending on active pane's state.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_UpdateConnectionClosedState()
    {
        ASSERT_UI_THREAD();

        if (_rootPane)
        {
            const bool isClosed = _rootPane->WalkTree([&](const auto& p) {
                return p->IsConnectionClosed();
            });

            _tabStatus.IsConnectionClosed(isClosed);
        }

        if (_activePane)
        {
            _restartConnectionMenuItem.Visibility(_activePane->IsConnectionClosed() ?
                                                      WUX::Visibility::Visible :
                                                      WUX::Visibility::Collapsed);
        }
    }

    void Tab::_RestartActivePaneConnection()
    {
        ActionAndArgs restartConnection{ ShortcutAction::RestartConnection, nullptr };
        _dispatch.DoAction(*this, restartConnection);
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
        UpdateTitle();
        _UpdateProgressState();
        _UpdateConnectionClosedState();

        // We need to move the pane to the top of our mru list
        // If its already somewhere in the list, remove it first
        if (const auto paneId = pane->Id())
        {
            for (auto i = _mruPanes.begin(); i != _mruPanes.end(); ++i)
            {
                if (*i == paneId.value())
                {
                    _mruPanes.erase(i);
                    break;
                }
            }
            _mruPanes.insert(_mruPanes.begin(), paneId.value());
        }

        if (_rootPane->GetLeafPaneCount() == 1)
        {
            _closePaneMenuItem.Visibility(WUX::Visibility::Collapsed);
        }

        _RecalculateAndApplyReadOnly();

        // Raise our own ActivePaneChanged event.
        ActivePaneChanged.raise(*this, nullptr);

        // If the new active pane is a terminal, tell other interested panes
        // what the new active pane is.
        const auto content{ pane->GetContent() };
        if (const auto termContent{ content.try_as<winrt::TerminalApp::TerminalPaneContent>() })
        {
            const auto& termControl{ termContent.GetTermControl() };
            _rootPane->WalkTree([termControl](const auto& p) {
                if (const auto& taskPane{ p->GetContent().try_as<SnippetsPaneContent>() })
                {
                    taskPane.SetLastActiveControl(termControl);
                }
                else if (const auto& taskPane{ p->GetContent().try_as<MarkdownPaneContent>() })
                {
                    taskPane.SetLastActiveControl(termControl);
                }
            });
        }
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
        std::weak_ptr<Pane> weakPane{ pane };

        auto gotFocusToken = pane->GotFocus([weakThis](std::shared_ptr<Pane> sender, WUX::FocusState focus) {
            // Do nothing if the Tab's lifetime is expired or pane isn't new.
            auto tab{ weakThis.get() };

            if (tab)
            {
                if (sender != tab->_activePane)
                {
                    auto senderIsChild = tab->_activePane->_HasChild(sender);

                    // Only move focus if we the program moved focus, or the
                    // user moved with their mouse. This is a problem because a
                    // pane isn't a control itself, and if we have the parent
                    // focused we are fine if the terminal control is focused,
                    // but we don't want to update the active pane.
                    if (!senderIsChild ||
                        (focus == WUX::FocusState::Programmatic && tab->_changingActivePane) ||
                        focus == WUX::FocusState::Pointer)
                    {
                        tab->_UpdateActivePane(sender);
                        tab->_RecalculateAndApplyTabColor();
                    }
                }
                tab->_focusState = WUX::FocusState::Programmatic;
                // This tab has gained focus, remove the bell indicator if it is active
                if (tab->_tabStatus.BellIndicator())
                {
                    tab->ShowBellIndicator(false);
                }
            }
        });

        auto lostFocusToken = pane->LostFocus([weakThis](std::shared_ptr<Pane> /*sender*/) {
            // Do nothing if the Tab's lifetime is expired or pane isn't new.
            auto tab{ weakThis.get() };

            if (tab)
            {
                // update this tab's focus state
                tab->_focusState = WUX::FocusState::Unfocused;
            }
        });

        // Add a Closed event handler to the Pane. If the pane closes out from
        // underneath us, and it's zoomed, we want to be able to make sure to
        // update our state accordingly to un-zoom that pane. See GH#7252.
        auto closedToken = pane->Closed([weakThis, weakPane](auto&& /*s*/, auto&& /*e*/) {
            if (auto tab{ weakThis.get() })
            {
                if (tab->_zoomedPane)
                {
                    tab->Content(tab->_rootPane->GetRootElement());
                    tab->ExitZoom();
                }

                if (auto pane = weakPane.lock())
                {
                    // When a parent pane is selected, but one of its children
                    // close out under it we still need to update title/focus information
                    // but the GotFocus handler will rightly see that the _activePane
                    // did not actually change. Triggering
                    if (pane != tab->_activePane && !tab->_activePane->_IsLeaf())
                    {
                        tab->_UpdateActivePane(tab->_activePane);
                    }

                    for (auto i = tab->_mruPanes.begin(); i != tab->_mruPanes.end(); ++i)
                    {
                        if (*i == pane->Id())
                        {
                            tab->_mruPanes.erase(i);
                            break;
                        }
                    }
                }
            }
        });

        // box the event token so that we can give a reference to it in the
        // event handler.
        auto detachedToken = std::make_shared<winrt::event_token>();
        // Add a Detached event handler to the Pane to clean up tab state
        // and other event handlers when a pane is removed from this tab.
        *detachedToken = pane->Detached([weakThis, weakPane, gotFocusToken, lostFocusToken, closedToken, detachedToken](std::shared_ptr<Pane> /*sender*/) {
            // Make sure we do this at most once
            if (auto pane{ weakPane.lock() })
            {
                pane->Detached(*detachedToken);
                pane->GotFocus(gotFocusToken);
                pane->LostFocus(lostFocusToken);
                pane->Closed(closedToken);

                if (auto tab{ weakThis.get() })
                {
                    tab->_DetachEventHandlersFromContent(pane->Id().value());

                    for (auto i = tab->_mruPanes.begin(); i != tab->_mruPanes.end(); ++i)
                    {
                        if (*i == pane->Id())
                        {
                            tab->_mruPanes.erase(i);
                            break;
                        }
                    }
                }
            }
        });
    }

    void Tab::_AppendMoveMenuItems(winrt::Windows::UI::Xaml::Controls::MenuFlyout flyout)
    {
        auto weakThis{ get_weak() };

        // Move to new window
        {
            Controls::FontIcon moveTabToNewWindowTabSymbol;
            moveTabToNewWindowTabSymbol.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
            moveTabToNewWindowTabSymbol.Glyph(L"\xE8A7");

            _moveToNewWindowMenuItem.Click([weakThis](auto&&, auto&&) {
                if (auto tab{ weakThis.get() })
                {
                    MoveTabArgs args{ L"new", MoveTabDirection::Forward };
                    ActionAndArgs actionAndArgs{ ShortcutAction::MoveTab, args };
                    tab->_dispatch.DoAction(*tab, actionAndArgs);
                }
            });
            _moveToNewWindowMenuItem.Text(RS_(L"MoveTabToNewWindowText"));
            _moveToNewWindowMenuItem.Icon(moveTabToNewWindowTabSymbol);

            const auto moveTabToNewWindowToolTip = RS_(L"MoveTabToNewWindowToolTip");
            WUX::Controls::ToolTipService::SetToolTip(_moveToNewWindowMenuItem, box_value(moveTabToNewWindowToolTip));
            Automation::AutomationProperties::SetHelpText(_moveToNewWindowMenuItem, moveTabToNewWindowToolTip);
        }

        // Move left
        {
            _moveLeftMenuItem.Click([weakThis](auto&&, auto&&) {
                if (auto tab{ weakThis.get() })
                {
                    MoveTabArgs args{ hstring{}, MoveTabDirection::Backward };
                    ActionAndArgs actionAndArgs{ ShortcutAction::MoveTab, args };
                    tab->_dispatch.DoAction(*tab, actionAndArgs);
                }
            });
            _moveLeftMenuItem.Text(RS_(L"TabMoveLeft"));
        }

        // Move right
        {
            _moveRightMenuItem.Click([weakThis](auto&&, auto&&) {
                if (auto tab{ weakThis.get() })
                {
                    MoveTabArgs args{ hstring{}, MoveTabDirection::Forward };
                    ActionAndArgs actionAndArgs{ ShortcutAction::MoveTab, args };
                    tab->_dispatch.DoAction(*tab, actionAndArgs);
                }
            });
            _moveRightMenuItem.Text(RS_(L"TabMoveRight"));
        }

        // Create a sub-menu for our extended move tab items.
        Controls::MenuFlyoutSubItem moveSubMenu;
        moveSubMenu.Text(RS_(L"TabMoveSubMenu"));
        moveSubMenu.Items().Append(_moveToNewWindowMenuItem);
        moveSubMenu.Items().Append(_moveRightMenuItem);
        moveSubMenu.Items().Append(_moveLeftMenuItem);
        flyout.Items().Append(moveSubMenu);
    }

    // Method Description:
    // - Append the close menu items to the context menu flyout
    // Arguments:
    // - flyout - the menu flyout to which the close items must be appended
    // Return Value:
    // - the sub-item that we use for all the nested "close" entries. This
    //   enables subclasses to add their own entries to this menu.
    winrt::Windows::UI::Xaml::Controls::MenuFlyoutSubItem Tab::_AppendCloseMenuItems(winrt::Windows::UI::Xaml::Controls::MenuFlyout flyout)
    {
        auto weakThis{ get_weak() };

        // Close tabs after
        _closeTabsAfterMenuItem.Click([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                CloseTabsAfterArgs args{ tab->_TabViewIndex };
                ActionAndArgs closeTabsAfter{ ShortcutAction::CloseTabsAfter, args };
                tab->_dispatch.DoAction(*tab, closeTabsAfter);
            }
        });
        _closeTabsAfterMenuItem.Text(RS_(L"TabCloseAfter"));
        const auto closeTabsAfterToolTip = RS_(L"TabCloseAfterToolTip");

        WUX::Controls::ToolTipService::SetToolTip(_closeTabsAfterMenuItem, box_value(closeTabsAfterToolTip));
        Automation::AutomationProperties::SetHelpText(_closeTabsAfterMenuItem, closeTabsAfterToolTip);

        // Close other tabs
        _closeOtherTabsMenuItem.Click([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                CloseOtherTabsArgs args{ tab->_TabViewIndex };
                ActionAndArgs closeOtherTabs{ ShortcutAction::CloseOtherTabs, args };
                tab->_dispatch.DoAction(*tab, closeOtherTabs);
            }
        });
        _closeOtherTabsMenuItem.Text(RS_(L"TabCloseOther"));
        const auto closeOtherTabsToolTip = RS_(L"TabCloseOtherToolTip");

        WUX::Controls::ToolTipService::SetToolTip(_closeOtherTabsMenuItem, box_value(closeOtherTabsToolTip));
        Automation::AutomationProperties::SetHelpText(_closeOtherTabsMenuItem, closeOtherTabsToolTip);

        // Close
        Controls::MenuFlyoutItem closeTabMenuItem;
        Controls::FontIcon closeSymbol;
        closeSymbol.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
        closeSymbol.Glyph(L"\xE711");

        closeTabMenuItem.Click([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->CloseRequested.raise(nullptr, nullptr);
            }
        });
        closeTabMenuItem.Text(RS_(L"TabClose"));
        closeTabMenuItem.Icon(closeSymbol);
        const auto closeTabToolTip = RS_(L"TabCloseToolTip");

        WUX::Controls::ToolTipService::SetToolTip(closeTabMenuItem, box_value(closeTabToolTip));
        Automation::AutomationProperties::SetHelpText(closeTabMenuItem, closeTabToolTip);

        // Create a sub-menu for our extended close items.
        Controls::MenuFlyoutSubItem closeSubMenu;
        closeSubMenu.Text(RS_(L"TabCloseSubMenu"));
        closeSubMenu.Items().Append(_closeTabsAfterMenuItem);
        closeSubMenu.Items().Append(_closeOtherTabsMenuItem);
        flyout.Items().Append(closeSubMenu);

        flyout.Items().Append(closeTabMenuItem);

        return closeSubMenu;
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

        // "Change tab color..."
        Controls::MenuFlyoutItem chooseColorMenuItem;
        {
            Controls::FontIcon colorPickSymbol;
            colorPickSymbol.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
            colorPickSymbol.Glyph(L"\xE790");

            chooseColorMenuItem.Click({ get_weak(), &Tab::_chooseColorClicked });
            chooseColorMenuItem.Text(RS_(L"TabColorChoose"));
            chooseColorMenuItem.Icon(colorPickSymbol);

            const auto chooseColorToolTip = RS_(L"ChooseColorToolTip");

            WUX::Controls::ToolTipService::SetToolTip(chooseColorMenuItem, box_value(chooseColorToolTip));
            Automation::AutomationProperties::SetHelpText(chooseColorMenuItem, chooseColorToolTip);
        }

        Controls::MenuFlyoutItem renameTabMenuItem;
        {
            // "Rename tab"
            Controls::FontIcon renameTabSymbol;
            renameTabSymbol.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
            renameTabSymbol.Glyph(L"\xE8AC"); // Rename

            renameTabMenuItem.Click({ get_weak(), &Tab::_renameTabClicked });
            renameTabMenuItem.Text(RS_(L"RenameTabText"));
            renameTabMenuItem.Icon(renameTabSymbol);

            const auto renameTabToolTip = RS_(L"RenameTabToolTip");

            WUX::Controls::ToolTipService::SetToolTip(renameTabMenuItem, box_value(renameTabToolTip));
            Automation::AutomationProperties::SetHelpText(renameTabMenuItem, renameTabToolTip);
        }

        Controls::MenuFlyoutItem duplicateTabMenuItem;
        {
            // "Duplicate tab"
            Controls::FontIcon duplicateTabSymbol;
            duplicateTabSymbol.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
            duplicateTabSymbol.Glyph(L"\xF5ED");

            duplicateTabMenuItem.Click({ get_weak(), &Tab::_duplicateTabClicked });
            duplicateTabMenuItem.Text(RS_(L"DuplicateTabText"));
            duplicateTabMenuItem.Icon(duplicateTabSymbol);

            const auto duplicateTabToolTip = RS_(L"DuplicateTabToolTip");

            WUX::Controls::ToolTipService::SetToolTip(duplicateTabMenuItem, box_value(duplicateTabToolTip));
            Automation::AutomationProperties::SetHelpText(duplicateTabMenuItem, duplicateTabToolTip);
        }

        Controls::MenuFlyoutItem splitTabMenuItem;
        {
            // "Split tab"
            Controls::FontIcon splitTabSymbol;
            splitTabSymbol.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
            splitTabSymbol.Glyph(L"\xF246"); // ViewDashboard

            splitTabMenuItem.Click({ get_weak(), &Tab::_splitTabClicked });
            splitTabMenuItem.Text(RS_(L"SplitTabText"));
            splitTabMenuItem.Icon(splitTabSymbol);

            const auto splitTabToolTip = RS_(L"SplitTabToolTip");

            WUX::Controls::ToolTipService::SetToolTip(splitTabMenuItem, box_value(splitTabToolTip));
            Automation::AutomationProperties::SetHelpText(splitTabMenuItem, splitTabToolTip);
        }

        Controls::MenuFlyoutItem closePaneMenuItem = _closePaneMenuItem;
        {
            // "Close pane"
            closePaneMenuItem.Click({ get_weak(), &Tab::_closePaneClicked });
            closePaneMenuItem.Text(RS_(L"ClosePaneText"));

            const auto closePaneToolTip = RS_(L"ClosePaneToolTip");

            WUX::Controls::ToolTipService::SetToolTip(closePaneMenuItem, box_value(closePaneToolTip));
            Automation::AutomationProperties::SetHelpText(closePaneMenuItem, closePaneToolTip);
        }

        Controls::MenuFlyoutItem exportTabMenuItem;
        {
            // "Export tab"
            Controls::FontIcon exportTabSymbol;
            exportTabSymbol.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
            exportTabSymbol.Glyph(L"\xE74E"); // Save

            exportTabMenuItem.Click({ get_weak(), &Tab::_exportTextClicked });
            exportTabMenuItem.Text(RS_(L"ExportTabText"));
            exportTabMenuItem.Icon(exportTabSymbol);

            const auto exportTabToolTip = RS_(L"ExportTabToolTip");

            WUX::Controls::ToolTipService::SetToolTip(exportTabMenuItem, box_value(exportTabToolTip));
            Automation::AutomationProperties::SetHelpText(exportTabMenuItem, exportTabToolTip);
        }

        Controls::MenuFlyoutItem findMenuItem;
        {
            // "Find"
            Controls::FontIcon findSymbol;
            findSymbol.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
            findSymbol.Glyph(L"\xF78B"); // SearchMedium

            findMenuItem.Click({ get_weak(), &Tab::_findClicked });
            findMenuItem.Text(RS_(L"FindText"));
            findMenuItem.Icon(findSymbol);

            const auto findToolTip = RS_(L"FindToolTip");

            WUX::Controls::ToolTipService::SetToolTip(findMenuItem, box_value(findToolTip));
            Automation::AutomationProperties::SetHelpText(findMenuItem, findToolTip);
        }

        Controls::MenuFlyoutItem restartConnectionMenuItem = _restartConnectionMenuItem;
        {
            // "Restart connection"
            Controls::FontIcon restartConnectionSymbol;
            restartConnectionSymbol.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
            restartConnectionSymbol.Glyph(L"\xE72C");

            restartConnectionMenuItem.Click([weakThis](auto&&, auto&&) {
                if (auto tab{ weakThis.get() })
                {
                    tab->_RestartActivePaneConnection();
                }
            });
            restartConnectionMenuItem.Text(RS_(L"RestartConnectionText"));
            restartConnectionMenuItem.Icon(restartConnectionSymbol);

            const auto restartConnectionToolTip = RS_(L"RestartConnectionToolTip");

            WUX::Controls::ToolTipService::SetToolTip(restartConnectionMenuItem, box_value(restartConnectionToolTip));
            Automation::AutomationProperties::SetHelpText(restartConnectionMenuItem, restartConnectionToolTip);
        }

        // Build the menu
        Controls::MenuFlyout contextMenuFlyout;
        Controls::MenuFlyoutSeparator menuSeparator;
        contextMenuFlyout.Items().Append(chooseColorMenuItem);
        contextMenuFlyout.Items().Append(renameTabMenuItem);
        contextMenuFlyout.Items().Append(duplicateTabMenuItem);
        contextMenuFlyout.Items().Append(splitTabMenuItem);
        _AppendMoveMenuItems(contextMenuFlyout);
        contextMenuFlyout.Items().Append(exportTabMenuItem);
        contextMenuFlyout.Items().Append(findMenuItem);
        contextMenuFlyout.Items().Append(restartConnectionMenuItem);
        contextMenuFlyout.Items().Append(menuSeparator);

        auto closeSubMenu = _AppendCloseMenuItems(contextMenuFlyout);
        closeSubMenu.Items().Append(closePaneMenuItem);

        // GH#5750 - When the context menu is dismissed with ESC, toss the focus
        // back to our control.
        contextMenuFlyout.Closed([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                // GH#10112 - if we're opening the tab renamer, don't
                // immediately toss focus to the control. We don't want to steal
                // focus from the tab renamer.
                const auto& terminalControl{ tab->GetActiveTerminalControl() }; // maybe null
                // If we're
                // * NOT in a rename
                // * AND (the content isn't a TermControl, OR the term control doesn't have focus in the search box)
                if (!tab->_headerControl.InRename() &&
                    (terminalControl == nullptr || !terminalControl.SearchBoxEditInFocus()))
                {
                    tab->RequestFocusActiveControl.raise();
                }
            }
        });

        TabViewItem().ContextFlyout(contextMenuFlyout);
    }

    // Method Description:
    // - Enable menu items based on tab index and total number of tabs
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_EnableMenuItems()
    {
        const auto tabIndex = TabViewIndex();
        const auto numOfTabs = TabViewNumTabs();

        // enabled if there are other tabs
        _closeOtherTabsMenuItem.IsEnabled(numOfTabs > 1);

        // enabled if there are other tabs on the right
        _closeTabsAfterMenuItem.IsEnabled(tabIndex < numOfTabs - 1);

        // enabled if not left-most tab
        _moveLeftMenuItem.IsEnabled(tabIndex > 0);

        // enabled if not last tab
        _moveRightMenuItem.IsEnabled(tabIndex < numOfTabs - 1);
    }

    void Tab::UpdateTabViewIndex(const uint32_t idx, const uint32_t numTabs)
    {
        ASSERT_UI_THREAD();

        TabViewIndex(idx);
        TabViewNumTabs(numTabs);
        _EnableMenuItems();
        _UpdateSwitchToTabKeyChord();
    }

    // Method Description:
    // Returns the tab color, if any
    // Arguments:
    // - <none>
    // Return Value:
    // - The tab's color, if any
    std::optional<winrt::Windows::UI::Color> Tab::GetTabColor()
    {
        ASSERT_UI_THREAD();

        std::optional<winrt::Windows::UI::Color> contentTabColor;
        if (const auto& content{ GetActiveContent() })
        {
            if (const auto color = content.TabColor())
            {
                contentTabColor = color.Value();
            }
        }

        // A Tab's color will be the result of layering a variety of sources,
        // from the bottom up:
        //
        // Color                |             | Set by
        // -------------------- | --          | --
        // Runtime Color        | _optional_  | Color Picker / `setTabColor` action
        // Content Tab Color    | _optional_  | Profile's `tabColor`, or a color set by VT (whatever the tab's content wants)
        // Theme Tab Background | _optional_  | `tab.backgroundColor` in the theme (handled in _RecalculateAndApplyTabColor)
        // Tab Default Color    | **default** | TabView in XAML
        //
        // coalesce will get us the first of these values that's
        // actually set, with nullopt being our sentinel for "use the default
        // tabview color" (and clear out any colors we've set).

        return til::coalesce(_runtimeTabColor,
                             contentTabColor,
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
        ASSERT_UI_THREAD();

        _runtimeTabColor.emplace(color);
        _RecalculateAndApplyTabColor();
        _tabStatus.TabColorIndicator(color);
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
        ASSERT_UI_THREAD();

        _runtimeTabColor.reset();
        _RecalculateAndApplyTabColor();
        _tabStatus.TabColorIndicator(GetTabColor().value_or(Windows::UI::Colors::Transparent()));
    }

    winrt::Windows::UI::Xaml::Media::Brush Tab::_BackgroundBrush()
    {
        Media::Brush terminalBrush{ nullptr };
        if (const auto& c{ GetActiveContent() })
        {
            terminalBrush = c.BackgroundBrush();
        }
        return terminalBrush;
    }

    // - Get the total number of leaf panes in this tab. This will be the number
    //   of actual controls hosted by this tab.
    // Arguments:
    // - <none>
    // Return Value:
    // - The total number of leaf panes hosted by this tab.
    int Tab::GetLeafPaneCount() const noexcept
    {
        ASSERT_UI_THREAD();

        return _rootPane->GetLeafPaneCount();
    }

    // Method Description:
    // - Calculate if a split is possible with the given direction and size.
    // - Converts Automatic splits to an appropriate direction depending on space.
    // Arguments:
    // - splitType: what direction to split.
    // - splitSize: how big the new split should be.
    // - availableSpace: how much space there is to work with.
    // Return value:
    // - This will return nullopt if a split of the given size/direction was not possible,
    //   or it will return the split direction with automatic converted to a cardinal direction.
    std::optional<SplitDirection> Tab::PreCalculateCanSplit(SplitDirection splitType,
                                                            const float splitSize,
                                                            winrt::Windows::Foundation::Size availableSpace) const
    {
        ASSERT_UI_THREAD();

        return _rootPane->PreCalculateCanSplit(_activePane, splitType, splitSize, availableSpace).value_or(std::nullopt);
    }

    // Method Description:
    // - Updates the zoomed pane when the focus changes
    // Arguments:
    // - newFocus: the new pane to be zoomed
    // Return Value:
    // - <none>
    void Tab::UpdateZoom(std::shared_ptr<Pane> newFocus)
    {
        ASSERT_UI_THREAD();

        // clear the existing content so the old zoomed pane can be added back to the root tree
        Content(nullptr);
        _rootPane->Restore(_zoomedPane);
        _zoomedPane = newFocus;
        _rootPane->Maximize(_zoomedPane);
        Content(_zoomedPane->GetRootElement());
    }

    // Method Description:
    // - Toggle our zoom state.
    //   * If we're not zoomed, then zoom the active pane, making it take the
    //     full size of the tab. We'll achieve this by changing our response to
    //     Tab::GetTabContent, so that it'll return the zoomed pane only.
    //   *  If we're currently zoomed on a pane, un-zoom that pane.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::ToggleZoom()
    {
        ASSERT_UI_THREAD();

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
        ASSERT_UI_THREAD();

        // Clear the content first, because with parent focusing it is possible
        // to zoom the root pane, but setting the content will not trigger the
        // property changed event since it is the same and you would end up with
        // an empty tab.
        Content(nullptr);
        _zoomedPane = _activePane;
        _rootPane->Maximize(_zoomedPane);
        // Update the tab header to show the magnifying glass
        _tabStatus.IsPaneZoomed(true);
        Content(_zoomedPane->GetRootElement());
    }
    void Tab::ExitZoom()
    {
        ASSERT_UI_THREAD();

        Content(nullptr);
        _rootPane->Restore(_zoomedPane);
        _zoomedPane = nullptr;
        // Update the tab header to hide the magnifying glass
        _tabStatus.IsPaneZoomed(false);
        Content(_rootPane->GetRootElement());
    }

    bool Tab::IsZoomed()
    {
        ASSERT_UI_THREAD();

        return _zoomedPane != nullptr;
    }

    TermControl _termControlFromPane(const auto& pane)
    {
        if (const auto content{ pane->GetContent() })
        {
            if (const auto termContent{ content.try_as<winrt::TerminalApp::TerminalPaneContent>() })
            {
                return termContent.GetTermControl();
            }
        }
        return nullptr;
    }

    // Method Description:
    // - Toggle read-only mode on the active pane
    // - If a parent pane is selected, this will ensure that all children have
    //   the same read-only status.
    void Tab::TogglePaneReadOnly()
    {
        ASSERT_UI_THREAD();

        auto hasReadOnly = false;
        auto allReadOnly = true;
        _activePane->WalkTree([&](const auto& p) {
            if (const auto& control{ _termControlFromPane(p) })
            {
                hasReadOnly |= control.ReadOnly();
                allReadOnly &= control.ReadOnly();
            }
        });
        _activePane->WalkTree([&](const auto& p) {
            if (const auto& control{ _termControlFromPane(p) })
            {
                // If all controls have the same read only state then just toggle
                if (allReadOnly || !hasReadOnly)
                {
                    control.ToggleReadOnly();
                }
                // otherwise set to all read only.
                else if (!control.ReadOnly())
                {
                    control.ToggleReadOnly();
                }
            }
        });
    }

    // Method Description:
    // - Set read-only mode on the active pane
    // - If a parent pane is selected, this will ensure that all children have
    //   the same read-only status.
    void Tab::SetPaneReadOnly(const bool readOnlyState)
    {
        auto hasReadOnly = false;
        auto allReadOnly = true;
        _activePane->WalkTree([&](const auto& p) {
            if (const auto& control{ _termControlFromPane(p) })
            {
                hasReadOnly |= control.ReadOnly();
                allReadOnly &= control.ReadOnly();
            }
        });
        _activePane->WalkTree([&](const auto& p) {
            if (const auto& control{ _termControlFromPane(p) })
            {
                // If all controls have the same read only state then just disable
                if (allReadOnly || !hasReadOnly)
                {
                    control.SetReadOnly(readOnlyState);
                }
                // otherwise set to all read only.
                else if (!control.ReadOnly())
                {
                    control.SetReadOnly(readOnlyState);
                }
            }
        });
    }

    // Method Description:
    // - Calculates if the tab is read-only.
    // The tab is considered read-only if one of the panes is read-only.
    // If after the calculation the tab is read-only we hide the close button on the tab view item
    void Tab::_RecalculateAndApplyReadOnly()
    {
        const auto control = GetActiveTerminalControl();
        if (control)
        {
            const auto isReadOnlyActive = control.ReadOnly();
            _tabStatus.IsReadOnlyActive(isReadOnlyActive);
        }

        ReadOnly(_rootPane->ContainsReadOnly());
        _updateIsClosable();

        // Update all the visuals on all our panes, so they can update their
        // border colors accordingly.
        _rootPane->WalkTree([](const auto& p) { p->UpdateVisuals(); });
    }

    std::shared_ptr<Pane> Tab::GetActivePane() const
    {
        ASSERT_UI_THREAD();

        return _activePane;
    }

    // Method Description:
    // - Creates a text for the title run in the tool tip by returning tab title
    // or <profile name>: <tab title> in the case the profile name differs from the title
    // Arguments:
    // - <none>
    // Return Value:
    // - The value to populate in the title run of the tool tip
    winrt::hstring Tab::_CreateToolTipTitle()
    {
        if (const auto profile{ GetFocusedProfile() })
        {
            const auto profileName{ profile.Name() };
            if (profileName != Title())
            {
                return winrt::hstring{ fmt::format(FMT_COMPILE(L"{}: {}"), profileName, Title()) };
            }
        }

        return Title();
    }

    // Method Description:
    // - Toggle broadcasting input to all the panes in this tab.
    void Tab::ToggleBroadcastInput()
    {
        const bool newIsBroadcasting = !_tabStatus.IsInputBroadcastActive();
        _tabStatus.IsInputBroadcastActive(newIsBroadcasting);
        _rootPane->EnableBroadcast(newIsBroadcasting);

        auto weakThis{ get_weak() };

        // When we change the state of broadcasting, add or remove event
        // handlers appropriately, so that controls won't be propagating events
        // needlessly if no one is listening.

        _rootPane->WalkTree([&](const auto& p) {
            const auto paneId = p->Id();
            if (!paneId.has_value())
            {
                return;
            }
            if (const auto& control{ _termControlFromPane(p) })
            {
                auto it = _contentEvents.find(*paneId);
                if (it != _contentEvents.end())
                {
                    auto& events = it->second;

                    // Always clear out old ones, just in case.
                    events.KeySent.revoke();
                    events.CharSent.revoke();
                    events.StringSent.revoke();

                    if (newIsBroadcasting)
                    {
                        _addBroadcastHandlers(control, events);
                    }
                }
            }
        });
    }

    void Tab::_addBroadcastHandlers(const TermControl& termControl, ContentEventTokens& events)
    {
        auto weakThis{ get_weak() };
        // ADD EVENT HANDLERS HERE
        events.KeySent = termControl.KeySent(winrt::auto_revoke, [weakThis](auto&& sender, auto&& e) {
            if (const auto tab{ weakThis.get() })
            {
                if (tab->_tabStatus.IsInputBroadcastActive())
                {
                    tab->_rootPane->BroadcastKey(sender.try_as<TermControl>(),
                                                 e.VKey(),
                                                 e.ScanCode(),
                                                 e.Modifiers(),
                                                 e.KeyDown());
                }
            }
        });

        events.CharSent = termControl.CharSent(winrt::auto_revoke, [weakThis](auto&& sender, auto&& e) {
            if (const auto tab{ weakThis.get() })
            {
                if (tab->_tabStatus.IsInputBroadcastActive())
                {
                    tab->_rootPane->BroadcastChar(sender.try_as<TermControl>(),
                                                  e.Character(),
                                                  e.ScanCode(),
                                                  e.Modifiers());
                }
            }
        });

        events.StringSent = termControl.StringSent(winrt::auto_revoke, [weakThis](auto&& sender, auto&& e) {
            if (const auto tab{ weakThis.get() })
            {
                if (tab->_tabStatus.IsInputBroadcastActive())
                {
                    tab->_rootPane->BroadcastString(sender.try_as<TermControl>(),
                                                    e.Text());
                }
            }
        });
    }

    void Tab::ThemeColor(const winrt::Microsoft::Terminal::Settings::Model::ThemeColor& focused,
                         const winrt::Microsoft::Terminal::Settings::Model::ThemeColor& unfocused,
                         const til::color& tabRowColor)
    {
        ASSERT_UI_THREAD();

        _themeColor = focused;
        _unfocusedThemeColor = unfocused;
        _tabRowColor = tabRowColor;
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
        // GetTabColor will return the color set by the color picker, or the
        // color specified in the profile. If neither of those were set,
        // then look to _themeColor to see if there's a value there.
        // Otherwise, clear our color, falling back to the TabView defaults.
        const auto currentColor = GetTabColor();
        if (currentColor.has_value())
        {
            _ApplyTabColorOnUIThread(currentColor.value());
        }
        else if (_themeColor != nullptr)
        {
            // Safely get the active control's brush.
            const Media::Brush terminalBrush{ _BackgroundBrush() };

            if (const auto themeBrush{ _themeColor.Evaluate(Application::Current().Resources(), terminalBrush, false) })
            {
                // ThemeColor.Evaluate will get us a Brush (because the
                // TermControl could have an acrylic BG, for example). Take
                // that brush, and get the color out of it. We don't really
                // want to have the tab items themselves be acrylic.
                _ApplyTabColorOnUIThread(til::color{ ThemeColor::ColorFromBrush(themeBrush) });
            }
            else
            {
                _ClearTabBackgroundColor();
            }
        }
        else
        {
            _ClearTabBackgroundColor();
        }
    }

    // Method Description:
    // - Applies the given color to the background of this tab's TabViewItem.
    // - Sets the tab foreground color depending on the luminance of
    // the background color
    // - This method should only be called on the UI thread.
    // Arguments:
    // - uiColor: the color the user picked for their tab
    // Return Value:
    // - <none>
    void Tab::_ApplyTabColorOnUIThread(const winrt::Windows::UI::Color& uiColor)
    {
        constexpr auto lightnessThreshold = 0.6f;
        const til::color color{ uiColor };
        Media::SolidColorBrush selectedTabBrush{};
        Media::SolidColorBrush deselectedTabBrush{};
        Media::SolidColorBrush fontBrush{};
        Media::SolidColorBrush deselectedFontBrush{};
        Media::SolidColorBrush secondaryFontBrush{};
        Media::SolidColorBrush hoverTabBrush{};
        Media::SolidColorBrush subtleFillColorSecondaryBrush;
        Media::SolidColorBrush subtleFillColorTertiaryBrush;

        // calculate the luminance of the current color and select a font
        // color based on that
        // see https://www.w3.org/TR/WCAG20/#relativeluminancedef
        if (ColorFix::GetLightness(color) >= lightnessThreshold)
        {
            auto subtleFillColorSecondary = winrt::Windows::UI::Colors::Black();
            subtleFillColorSecondary.A = 0x09;
            subtleFillColorSecondaryBrush.Color(subtleFillColorSecondary);
            auto subtleFillColorTertiary = winrt::Windows::UI::Colors::Black();
            subtleFillColorTertiary.A = 0x06;
            subtleFillColorTertiaryBrush.Color(subtleFillColorTertiary);
        }
        else
        {
            auto subtleFillColorSecondary = winrt::Windows::UI::Colors::White();
            subtleFillColorSecondary.A = 0x0F;
            subtleFillColorSecondaryBrush.Color(subtleFillColorSecondary);
            auto subtleFillColorTertiary = winrt::Windows::UI::Colors::White();
            subtleFillColorTertiary.A = 0x0A;
            subtleFillColorTertiaryBrush.Color(subtleFillColorTertiary);
        }

        // The tab font should be based on the evaluated appearance of the tab color layered on tab row.
        const auto layeredTabColor = color.layer_over(_tabRowColor);
        if (ColorFix::GetLightness(layeredTabColor) >= lightnessThreshold)
        {
            fontBrush.Color(winrt::Windows::UI::Colors::Black());
            auto secondaryFontColor = winrt::Windows::UI::Colors::Black();
            // For alpha value see: https://github.com/microsoft/microsoft-ui-xaml/blob/7a33ad772d77d908aa6b316ec24e6d2eb3ebf571/dev/CommonStyles/Common_themeresources_any.xaml#L269
            secondaryFontColor.A = 0x9E;
            secondaryFontBrush.Color(secondaryFontColor);
        }
        else
        {
            fontBrush.Color(winrt::Windows::UI::Colors::White());
            auto secondaryFontColor = winrt::Windows::UI::Colors::White();
            // For alpha value see: https://github.com/microsoft/microsoft-ui-xaml/blob/7a33ad772d77d908aa6b316ec24e6d2eb3ebf571/dev/CommonStyles/Common_themeresources_any.xaml#L14
            secondaryFontColor.A = 0xC5;
            secondaryFontBrush.Color(secondaryFontColor);
        }

        selectedTabBrush.Color(color);

        // Start with the current tab color, set to Opacity=.3
        auto deselectedTabColor = color.with_alpha(77); // 255 * .3 = 77

        // If we DON'T have a color set from the color picker, or the profile's
        // tabColor, but we do have a unfocused color in the theme, use the
        // unfocused theme color here instead.
        if (!GetTabColor().has_value() &&
            _unfocusedThemeColor != nullptr)
        {
            // Safely get the active control's brush.
            const Media::Brush terminalBrush{ _BackgroundBrush() };

            // Get the color of the brush.
            if (const auto themeBrush{ _unfocusedThemeColor.Evaluate(Application::Current().Resources(), terminalBrush, false) })
            {
                // We did figure out the brush. Get the color out of it. If it
                // was "accent" or "terminalBackground", then we're gonna set
                // the alpha to .3 manually here.
                // (ThemeColor::UnfocusedTabOpacity will do this for us). If the
                // user sets both unfocused and focused tab.background to
                // terminalBackground, this will allow for some differentiation
                // (and is generally just sensible).
                deselectedTabColor = til::color{ ThemeColor::ColorFromBrush(themeBrush) }.with_alpha(_unfocusedThemeColor.UnfocusedTabOpacity());
            }
        }

        // currently if a tab has a custom color, a deselected state is
        // signified by using the same color with a bit of transparency
        deselectedTabBrush.Color(deselectedTabColor.with_alpha(255));
        deselectedTabBrush.Opacity(deselectedTabColor.a / 255.f);

        hoverTabBrush.Color(color);
        hoverTabBrush.Opacity(0.6);

        // Account for the color of the tab row when setting the color of text
        // on inactive tabs. Consider:
        // * black active tabs
        // * on a white tab row
        // * with a transparent inactive tab color
        //
        // We don't want that to result in white text on a white tab row for
        // inactive tabs.
        const auto deselectedActualColor = deselectedTabColor.layer_over(_tabRowColor);
        if (ColorFix::GetLightness(deselectedActualColor) >= lightnessThreshold)
        {
            deselectedFontBrush.Color(winrt::Windows::UI::Colors::Black());
        }
        else
        {
            deselectedFontBrush.Color(winrt::Windows::UI::Colors::White());
        }

        // Add the empty theme dictionaries
        const auto& tabItemThemeResources{ TabViewItem().Resources().ThemeDictionaries() };
        ResourceDictionary lightThemeDictionary;
        ResourceDictionary darkThemeDictionary;
        ResourceDictionary highContrastThemeDictionary;
        tabItemThemeResources.Insert(winrt::box_value(L"Light"), lightThemeDictionary);
        tabItemThemeResources.Insert(winrt::box_value(L"Dark"), darkThemeDictionary);
        tabItemThemeResources.Insert(winrt::box_value(L"HighContrast"), highContrastThemeDictionary);

        // Apply the color to the tab
        TabViewItem().Background(deselectedTabBrush);

        // Now actually set the resources we want in them.
        // Before, we used to put these on the ResourceDictionary directly.
        // However, HighContrast mode may require some adjustments. So let's just add
        //   all three so we can make those adjustments on the HighContrast version.
        for (const auto& [k, v] : tabItemThemeResources)
        {
            const bool isHighContrast = winrt::unbox_value<hstring>(k) == L"HighContrast";
            const auto& currentDictionary = v.as<ResourceDictionary>();

            // TabViewItem.Background
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderBackground"), selectedTabBrush);
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderBackgroundSelected"), selectedTabBrush);
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderBackgroundPointerOver"), isHighContrast ? fontBrush : hoverTabBrush);
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderBackgroundPressed"), selectedTabBrush);

            // TabViewItem.Foreground (aka text)
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderForeground"), deselectedFontBrush);
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderForegroundSelected"), fontBrush);
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderForegroundPointerOver"), isHighContrast ? selectedTabBrush : fontBrush);
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderForegroundPressed"), fontBrush);

            // TabViewItem.CloseButton.Foreground (aka X)
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderCloseButtonForeground"), deselectedFontBrush);
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderCloseButtonForegroundPressed"), isHighContrast ? deselectedFontBrush : secondaryFontBrush);
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderCloseButtonForegroundPointerOver"), isHighContrast ? deselectedFontBrush : fontBrush);

            // TabViewItem.CloseButton.Foreground _when_ interacting with the tab
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderPressedCloseButtonForeground"), fontBrush);
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderPointerOverCloseButtonForeground"), isHighContrast ? selectedTabBrush : fontBrush);
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderSelectedCloseButtonForeground"), fontBrush);

            // TabViewItem.CloseButton.Background (aka X button)
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderCloseButtonBackgroundPressed"), isHighContrast ? selectedTabBrush : subtleFillColorTertiaryBrush);
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderCloseButtonBackgroundPointerOver"), isHighContrast ? selectedTabBrush : subtleFillColorSecondaryBrush);

            // A few miscellaneous resources that WinUI said may be removed in the future
            currentDictionary.Insert(winrt::box_value(L"TabViewButtonForegroundActiveTab"), fontBrush);
            currentDictionary.Insert(winrt::box_value(L"TabViewButtonForegroundPressed"), fontBrush);
            currentDictionary.Insert(winrt::box_value(L"TabViewButtonForegroundPointerOver"), fontBrush);

            // Add a few extra ones for high contrast mode
            // BODGY: contrary to the docs, Insert() seems to throw if the value already exists
            //   Make sure you don't touch any that already exist here!
            if (isHighContrast)
            {
                // TabViewItem.CloseButton.Border: in HC mode, the border makes the button more clearly visible
                currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderCloseButtonBorderBrushPressed"), fontBrush);
                currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderCloseButtonBorderBrushPointerOver"), fontBrush);
                currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderCloseButtonBorderBrushSelected"), fontBrush);
            }
        }

        _RefreshVisualState();
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
        static const winrt::hstring keys[] = {
            // TabViewItem.Background
            L"TabViewItemHeaderBackground",
            L"TabViewItemHeaderBackgroundSelected",
            L"TabViewItemHeaderBackgroundPointerOver",
            L"TabViewItemHeaderBackgroundPressed",

            // TabViewItem.Foreground (aka text)
            L"TabViewItemHeaderForeground",
            L"TabViewItemHeaderForegroundSelected",
            L"TabViewItemHeaderForegroundPointerOver",
            L"TabViewItemHeaderForegroundPressed",

            // TabViewItem.CloseButton.Foreground (aka X)
            L"TabViewItemHeaderCloseButtonForeground",
            L"TabViewItemHeaderForegroundSelected",
            L"TabViewItemHeaderCloseButtonForegroundPointerOver",
            L"TabViewItemHeaderCloseButtonForegroundPressed",

            // TabViewItem.CloseButton.Foreground _when_ interacting with the tab
            L"TabViewItemHeaderPressedCloseButtonForeground",
            L"TabViewItemHeaderPointerOverCloseButtonForeground",
            L"TabViewItemHeaderSelectedCloseButtonForeground",

            // TabViewItem.CloseButton.Background (aka X button)
            L"TabViewItemHeaderCloseButtonBackground",
            L"TabViewItemHeaderCloseButtonBackgroundPressed",
            L"TabViewItemHeaderCloseButtonBackgroundPointerOver",

            // A few miscellaneous resources that WinUI said may be removed in the future
            L"TabViewButtonForegroundActiveTab",
            L"TabViewButtonForegroundPressed",
            L"TabViewButtonForegroundPointerOver",

            // TabViewItem.CloseButton.Border: in HC mode, the border makes the button more clearly visible
            L"TabViewItemHeaderCloseButtonBorderBrushPressed",
            L"TabViewItemHeaderCloseButtonBorderBrushPointerOver",
            L"TabViewItemHeaderCloseButtonBorderBrushSelected"
        };

        const auto& tabItemThemeResources{ TabViewItem().Resources().ThemeDictionaries() };

        // simply clear any of the colors in the tab's dict
        for (const auto& keyString : keys)
        {
            const auto key = winrt::box_value(keyString);
            for (const auto& [_, v] : tabItemThemeResources)
            {
                const auto& themeDictionary = v.as<ResourceDictionary>();
                themeDictionary.Remove(key);
            }
        }

        // GH#11382 DON'T set the background to null. If you do that, then the
        // tab won't be hit testable at all. Transparent, however, is a totally
        // valid hit test target. That makes sense.
        TabViewItem().Background(WUX::Media::SolidColorBrush{ Windows::UI::Colors::Transparent() });

        _RefreshVisualState();
    }

    // Method Description:
    // BODGY
    // - Toggles the requested theme of the tab view item,
    //   so that changes to the tab color are reflected immediately
    // - Prior to MUX 2.8, we only toggled the visual state here, but that seemingly
    //   doesn't work in 2.8.
    // - Just changing the Theme also doesn't seem to work by itself - there
    //   seems to be a way for the tab to set the deselected foreground onto
    //   itself as it becomes selected. If the mouse isn't over the tab, that
    //   can result in mismatched fg/bg's (see GH#15184). So that's right, we
    //   need to do both.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void Tab::_RefreshVisualState()
    {
        const auto& item{ TabViewItem() };

        const auto& reqTheme = TabViewItem().RequestedTheme();
        item.RequestedTheme(ElementTheme::Light);
        item.RequestedTheme(ElementTheme::Dark);
        item.RequestedTheme(reqTheme);

        if (TabViewItem().IsSelected())
        {
            VisualStateManager::GoToState(item, L"Normal", true);
            VisualStateManager::GoToState(item, L"Selected", true);
        }
        else
        {
            VisualStateManager::GoToState(item, L"Selected", true);
            VisualStateManager::GoToState(item, L"Normal", true);
        }
    }

    TabCloseButtonVisibility Tab::CloseButtonVisibility()
    {
        return _closeButtonVisibility;
    }

    // Method Description:
    // - set our internal state to track if we were requested to have a visible
    //   tab close button or not.
    // - This is called every time the active tab changes. That way, the changes
    //   in focused tab can be reflected for the "ActiveOnly" state.
    void Tab::CloseButtonVisibility(TabCloseButtonVisibility visibility)
    {
        _closeButtonVisibility = visibility;
        _updateIsClosable();
    }

    // Method Description:
    // - Update our close button's visibility, to reflect both the ReadOnly
    //   state of the tab content, and also if if we were told to have a visible
    //   close button at all.
    //   - the tab being read-only takes precedence. That will always suppress
    //     the close button.
    //   - Otherwise we'll use the state set in CloseButtonVisibility to control
    //     the tab's visibility.
    void Tab::_updateIsClosable()
    {
        bool isClosable = true;

        if (ReadOnly())
        {
            isClosable = false;
        }
        else
        {
            switch (_closeButtonVisibility)
            {
            case TabCloseButtonVisibility::Never:
                isClosable = false;
                break;
            case TabCloseButtonVisibility::Hover:
                isClosable = true;
                break;
            case TabCloseButtonVisibility::ActiveOnly:
                isClosable = _focused();
                break;
            default:
                isClosable = true;
                break;
            }
        }
        TabViewItem().IsClosable(isClosable);
    }

    bool Tab::_focused() const noexcept
    {
        return _focusState != FocusState::Unfocused;
    }

    void Tab::_chooseColorClicked(const winrt::Windows::Foundation::IInspectable& /* sender */,
                                  const winrt::Windows::UI::Xaml::RoutedEventArgs& /* args */)
    {
        _dispatch.DoAction(*this, { ShortcutAction::OpenTabColorPicker, nullptr });
    }
    void Tab::_renameTabClicked(const winrt::Windows::Foundation::IInspectable& /* sender */,
                                const winrt::Windows::UI::Xaml::RoutedEventArgs& /* args */)
    {
        ActivateTabRenamer();
    }
    void Tab::_duplicateTabClicked(const winrt::Windows::Foundation::IInspectable& /* sender */,
                                   const winrt::Windows::UI::Xaml::RoutedEventArgs& /* args */)
    {
        ActionAndArgs actionAndArgs{ ShortcutAction::DuplicateTab, nullptr };
        _dispatch.DoAction(*this, actionAndArgs);
    }
    void Tab::_splitTabClicked(const winrt::Windows::Foundation::IInspectable& /* sender */,
                               const winrt::Windows::UI::Xaml::RoutedEventArgs& /* args */)
    {
        ActionAndArgs actionAndArgs{ ShortcutAction::SplitPane, SplitPaneArgs{ SplitType::Duplicate } };
        _dispatch.DoAction(*this, actionAndArgs);
    }
    void Tab::_closePaneClicked(const winrt::Windows::Foundation::IInspectable& /* sender */,
                                const winrt::Windows::UI::Xaml::RoutedEventArgs& /* args */)
    {
        ClosePane();
    }
    void Tab::_exportTextClicked(const winrt::Windows::Foundation::IInspectable& /* sender */,
                                 const winrt::Windows::UI::Xaml::RoutedEventArgs& /* args */)
    {
        ActionAndArgs actionAndArgs{};
        actionAndArgs.Action(ShortcutAction::ExportBuffer);
        _dispatch.DoAction(*this, actionAndArgs);
    }
    void Tab::_findClicked(const winrt::Windows::Foundation::IInspectable& /* sender */,
                           const winrt::Windows::UI::Xaml::RoutedEventArgs& /* args */)
    {
        ActionAndArgs actionAndArgs{ ShortcutAction::Find, nullptr };
        _dispatch.DoAction(*this, actionAndArgs);
    }
    void Tab::_bubbleRestartTerminalRequested(TerminalApp::TerminalPaneContent sender,
                                              const winrt::Windows::Foundation::IInspectable& args)
    {
        RestartTerminalRequested.raise(sender, args);
    }
}

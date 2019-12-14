// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include "ColorPickupFlyout.h"
#include "Tab.h"
#include "Utils.h"
#include "winrt/Windows.UI.Popups.h"

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
    _rootPane = std::make_shared<Pane>(profile, control, true);

    _rootPane->Closed([=](auto&& /*s*/, auto&& /*e*/) {
        _ClosedHandlers(nullptr, nullptr);
    });

    _activePane = _rootPane;

    _MakeTabViewItem();
}

void Tab::_MakeTabViewItem()
{
    _tabViewItem = ::winrt::MUX::Controls::TabViewItem{};
    _CreateContextMenu();
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
    return _activePane->GetTerminalControl();
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

void Tab::UpdateIcon(const winrt::hstring iconPath)
{
    // Don't reload our icon if it hasn't changed.
    if (iconPath == _lastIconPath)
    {
        return;
    }

    _lastIconPath = iconPath;

    std::weak_ptr<Tab> weakThis{ shared_from_this() };

    _tabViewItem.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [weakThis]() {
        if (auto tab{ weakThis.lock() })
        {
            tab->_tabViewItem.IconSource(GetColoredIcon<winrt::MUX::Controls::IconSource>(tab->_lastIconPath));
        }
    });
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
void Tab::SetTabText(const winrt::hstring& text)
{
    // Copy the hstring, so we don't capture a dead reference
    winrt::hstring textCopy{ text };
    std::weak_ptr<Tab> weakThis{ shared_from_this() };

    _tabViewItem.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [text = std::move(textCopy), weakThis]() {
        if (auto tab{ weakThis.lock() })
        {
            tab->_tabViewItem.Header(winrt::box_value(text));
        }
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
    auto control = GetActiveTerminalControl();
    control.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [control, delta]() {
        const auto currentOffset = control.GetScrollOffset();
        control.KeyboardScrollViewport(currentOffset + delta);
    });
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

    _AttachEventHandlersToControl(control);

    // Add a event handlers to the new panes' GotFocus event. When the pane
    // gains focus, we'll mark it as the new active pane.
    _AttachEventHandlersToPane(first);
    _AttachEventHandlersToPane(second);
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
    // NOTE: This _must_ be called on the root pane, so that it can propogate
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
    // NOTE: This _must_ be called on the root pane, so that it can propogate
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
    // NOTE: This _must_ be called on the root pane, so that it can propogate
    // throughout the entire tree.
    _rootPane->NavigateFocus(direction);
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
    std::weak_ptr<Tab> weakThis{ shared_from_this() };

    pane->GotFocus([weakThis](std::shared_ptr<Pane> sender) {
        // Do nothing if the Tab's lifetime is expired or pane isn't new.
        auto tab{ weakThis.lock() };
        if (tab && sender != tab->_activePane)
        {
            // Clear the active state of the entire tree, and mark only the sender as active.
            tab->_rootPane->ClearActive();
            tab->_activePane = sender;
            tab->_activePane->SetActive();

            // Update our own title text to match the newly-active pane.
            tab->SetTabText(tab->GetActiveTitle());

            // Raise our own ActivePaneChanged event.
            tab->_ActivePaneChangedHandlers();
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
    auto newTabFlyout = Controls::MenuFlyout{};
    auto menuSeparator = Controls::MenuFlyoutSeparator{};
    auto chooseColorMenuItem = Controls::MenuFlyoutItem{};
    auto resetColorMenuItem = Controls::MenuFlyoutItem{};
    auto closeTabMenuItem = Controls::MenuFlyoutItem{};

    auto colorPickSymbol = Controls::FontIcon{};
    colorPickSymbol.FontFamily(Media::FontFamily{ L"Segoe MDL2 Assets" });
    colorPickSymbol.Glyph(L"\xE790");

    auto closeSymbol = Controls::FontIcon{};
    closeSymbol.FontFamily(Media::FontFamily{ L"Segoe MDL2 Assets" });
    closeSymbol.Glyph(L"\xE8BB");

    auto clearAllSymbol = Controls::FontIcon{};
    clearAllSymbol.FontFamily(Media::FontFamily{ L"Segoe MDL2 Assets" });
    clearAllSymbol.Glyph(L"\xED62");

    auto tabClose = RS_(L"TabClose");
    closeTabMenuItem.Text(tabClose);
    closeTabMenuItem.Icon(closeSymbol);

    closeTabMenuItem.Click([this](auto&&, auto&&) {
        ClosePane();
    });

    _tabColorPickup.ColorSelected([this](auto newTabColor) {
        _SetTabColor(newTabColor);
    });

    _tabColorPickup.ColorCleared([this]() {
        _ResetTabColor();
    });

    chooseColorMenuItem.Click([this](auto&&, auto&&) {
        _tabColorPickup.ShowAt(_tabViewItem);
    });
    chooseColorMenuItem.Icon(colorPickSymbol);

    auto tabChooseColorString = RS_(L"TabColorChoose");
    chooseColorMenuItem.Text(tabChooseColorString);

    newTabFlyout.Items().Append(chooseColorMenuItem);
    newTabFlyout.Items().Append(menuSeparator);
    newTabFlyout.Items().Append(closeTabMenuItem);
    _tabViewItem.ContextFlyout(newTabFlyout);
}

// Method Description:
// - Sets the tab background color to the color chosen by the user
// - Sets the tab foreground color depending on the luminance of
// the background color
// Arguments:
// - color: the shiny color the user picked for their tab
// Return Value:
// - <none>
void Tab::_SetTabColor(const winrt::Windows::UI::Color& color)
{
    _tabViewItem.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [this, color]() {
        Media::SolidColorBrush selectedTabBrush{};
        Media::SolidColorBrush deselectedTabBrush{};
        Media::SolidColorBrush fontBrush{};

        // see https://en.wikipedia.org/wiki/Luma_(video)#Rec._601_luma_versus_Rec._709_luma_coefficients
        float c[] = { color.R / 255.f, color.G / 255.f, color.B / 255.f };
        for (int i = 0; i < 3; i++)
        {
            if (c[i] <= 0.03928)
            {
                c[i] = c[i] / 12.92f;
            }
            else
            {
                c[i] = std::pow((c[i] + 0.055f) / 1.055f, 2.4f);
            }
        }

        // calculate the luminance of the current color and select a font
        // color based on that
        auto luminance = 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
        if (luminance > 0.179)
        {
            fontBrush.Color(winrt::Windows::UI::Colors::Black());
        }
        else
        {
            fontBrush.Color(winrt::Windows::UI::Colors::White());
        }

        selectedTabBrush.Color(color);

        // currently if a tab has a custom color, a deselected state is
        // signified by using the same color with a bit ot transparency
        auto deselectedTabColor = color;
        deselectedTabColor.A = 64;
        deselectedTabBrush.Color(deselectedTabColor);
        _tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackgroundSelected"), selectedTabBrush);
        _tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackground"), deselectedTabBrush);
        _tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackgroundPointerOver"), selectedTabBrush);
        _tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderForeground"), fontBrush);
        _tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderForegroundSelected"), fontBrush);
        _tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderForegroundPointerOver"), fontBrush);
        //switch the visual state so that the colors are updated immediately
        if (_focused)
        {
            winrt::Windows::UI::Xaml::VisualStateManager::GoToState(_tabViewItem, L"Normal", true);
            winrt::Windows::UI::Xaml::VisualStateManager::GoToState(_tabViewItem, L"Selected", true);
        }
        else
        {
            winrt::Windows::UI::Xaml::VisualStateManager::GoToState(_tabViewItem, L"Selected", true);
            winrt::Windows::UI::Xaml::VisualStateManager::GoToState(_tabViewItem, L"Normal", true);
        }
    });
}

// Method Description:
// Clear the custom color of the tab, if any
// the background color
// Arguments:
// - <none>
// Return Value:
// - <none>
void Tab::_ResetTabColor()
{
    _tabViewItem.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [this]() {
        winrt::hstring keys[] = {
            L"TabViewItemHeaderBackground",
            L"TabViewItemHeaderBackgroundSelected",
            L"TabViewItemHeaderBackgroundPointerOver",
            L"TabViewItemHeaderForeground",
            L"TabViewItemHeaderForegroundSelected",
            L"TabViewItemHeaderForegroundPointerOver",
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

        //switch the visual state so that the colors are updated
        if (_focused)
        {
            winrt::Windows::UI::Xaml::VisualStateManager::GoToState(_tabViewItem, L"Normal", true);
            winrt::Windows::UI::Xaml::VisualStateManager::GoToState(_tabViewItem, L"Selected", true);
        }
        else
        {
            winrt::Windows::UI::Xaml::VisualStateManager::GoToState(_tabViewItem, L"Selected", true);
            winrt::Windows::UI::Xaml::VisualStateManager::GoToState(_tabViewItem, L"Normal", true);
        }
    });
}

UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"TerminalApp/Resources")
DEFINE_EVENT(Tab, ActivePaneChanged, _ActivePaneChangedHandlers, winrt::delegate<>);

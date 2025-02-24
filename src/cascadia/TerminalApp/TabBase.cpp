// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <LibraryResources.h>
#include "TabBase.h"
#include "TabBase.g.cpp"
#include "Utils.h"
#include "ColorHelper.h"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::System;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

#define ASSERT_UI_THREAD() assert(TabViewItem().Dispatcher().HasThreadAccess())

namespace winrt::TerminalApp::implementation
{

    // Method Description:
    // - Prepares this tab for being removed from the UI hierarchy
    void TabBase::Shutdown()
    {
        ASSERT_UI_THREAD();

        // NOTE: `TerminalPage::_HandleCloseTabRequested` relies on the content being null after this call.
        Content(nullptr);
    }

    // Method Description:
    // - Creates a context menu attached to the tab.
    // Currently contains elements allowing the user to close the selected tab
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TabBase::_CreateContextMenu()
    {
        auto weakThis{ get_weak() };

        // Build the menu
        Controls::MenuFlyout contextMenuFlyout;
        // GH#5750 - When the context menu is dismissed with ESC, toss the focus
        // back to our control.
        contextMenuFlyout.Closed([weakThis](auto&&, auto&&) {
            if (auto tab{ weakThis.get() })
            {
                tab->RequestFocusActiveControl.raise();
            }
        });
        _AppendMoveMenuItems(contextMenuFlyout);
        _AppendCloseMenuItems(contextMenuFlyout);
        TabViewItem().ContextFlyout(contextMenuFlyout);
    }

    void TabBase::_AppendMoveMenuItems(winrt::Windows::UI::Xaml::Controls::MenuFlyout flyout)
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
    winrt::Windows::UI::Xaml::Controls::MenuFlyoutSubItem TabBase::_AppendCloseMenuItems(winrt::Windows::UI::Xaml::Controls::MenuFlyout flyout)
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
    // - Enable menu items based on tab index and total number of tabs
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TabBase::_EnableMenuItems()
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

    void TabBase::UpdateTabViewIndex(const uint32_t idx, const uint32_t numTabs)
    {
        ASSERT_UI_THREAD();

        TabViewIndex(idx);
        TabViewNumTabs(numTabs);
        _EnableMenuItems();
        _UpdateSwitchToTabKeyChord();
    }

    void TabBase::SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch)
    {
        ASSERT_UI_THREAD();

        _dispatch = dispatch;
    }

    void TabBase::SetActionMap(const Microsoft::Terminal::Settings::Model::IActionMapView& actionMap)
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
    void TabBase::_UpdateSwitchToTabKeyChord()
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
    // - Creates a text for the title run in the tool tip by returning tab title
    // Arguments:
    // - <none>
    // Return Value:
    // - The value to populate in the title run of the tool tip
    winrt::hstring TabBase::_CreateToolTipTitle()
    {
        return _Title;
    }

    // Method Description:
    // - Sets tab tool tip to a concatenation of title and key chord
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TabBase::_UpdateToolTip()
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
    // - Initializes a TabViewItem for this Tab instance.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TabBase::_MakeTabViewItem()
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
    }

    std::optional<winrt::Windows::UI::Color> TabBase::GetTabColor()
    {
        ASSERT_UI_THREAD();

        return std::nullopt;
    }

    void TabBase::ThemeColor(const winrt::Microsoft::Terminal::Settings::Model::ThemeColor& focused,
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
    void TabBase::_RecalculateAndApplyTabColor()
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
    // - color: the color the user picked for their tab
    // Return Value:
    // - <none>
    void TabBase::_ApplyTabColorOnUIThread(const winrt::Windows::UI::Color& color)
    {
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
        if (TerminalApp::ColorHelper::IsBrightColor(color))
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
        const auto layeredTabColor = til::color{ color }.layer_over(_tabRowColor);
        if (TerminalApp::ColorHelper::IsBrightColor(layeredTabColor))
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
        til::color deselectedTabColor{ color };
        deselectedTabColor = deselectedTabColor.with_alpha(77); // 255 * .3 = 77

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
        if (TerminalApp::ColorHelper::IsBrightColor(deselectedActualColor))
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
            currentDictionary.Insert(winrt::box_value(L"TabViewItemHeaderBackground"), deselectedTabBrush);
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
    void TabBase::_ClearTabBackgroundColor()
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
    void TabBase::_RefreshVisualState()
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

    TabCloseButtonVisibility TabBase::CloseButtonVisibility()
    {
        return _closeButtonVisibility;
    }

    // Method Description:
    // - set our internal state to track if we were requested to have a visible
    //   tab close button or not.
    // - This is called every time the active tab changes. That way, the changes
    //   in focused tab can be reflected for the "ActiveOnly" state.
    void TabBase::CloseButtonVisibility(TabCloseButtonVisibility visibility)
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
    void TabBase::_updateIsClosable()
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

    bool TabBase::_focused() const noexcept
    {
        return _focusState != FocusState::Unfocused;
    }

}

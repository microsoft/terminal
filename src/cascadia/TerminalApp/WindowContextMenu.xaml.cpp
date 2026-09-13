#include "pch.h"
#include "WindowContextMenu.xaml.h"
#if __has_include("WindowContextMenu.g.cpp")
#include "WindowContextMenu.g.cpp"
#endif

#include <windowsx.h>

namespace winrt::TerminalApp::implementation
{
    WindowContextMenu::WindowContextMenu(uint64_t handle)
    {
        InitializeComponent();
        _AttachToParentWindow(reinterpret_cast<HWND>(handle));
    }

    WindowContextMenu::~WindowContextMenu()
    {
        _RemoveContextMenuSubClassProc();
    }

    // Method Description:
    // - Static window subclass procedure used to route Win32 messages back to the
    //   WindowContextMenu class instance.
    // Arguments:
    // - window: the handle to the window receiving the message
    // - message: the message ID
    // - wparam: additional message-specific information
    // - lparam: additional message-specific information
    // - uSubClass: the subclass ID (unused)
    // - dwRefData: the pointer to the WindowContextMenu instance
    // Return Value:
    // - The result of the message processing, or DefSubclassProc if unhandled.
    [[nodiscard]] LRESULT __stdcall WindowContextMenu::_StaticContextMenuSubClassProc(HWND const window, UINT const message, WPARAM const wparam, LPARAM const lparam, UINT_PTR const /*uSubClass*/, DWORD_PTR const dwRefData) noexcept
    {
        WINRT_ASSERT(window);

        if (auto windowContextMenu{ reinterpret_cast<WindowContextMenu*>(dwRefData) })
        {
            return windowContextMenu->_ContextMenuMessageHandler(message, wparam, lparam);
        }
        return DefSubclassProc(window, message, wparam, lparam);
    }

    // Method Description:
    // - Instance-level message handler. Intercepts style changes to update menu items
    //   and intercepts context menu invocations to show our custom XAML menu.
    // Arguments:
    // - message: the message ID
    // - wparam: additional message-specific information
    // - lparam: additional message-specific information
    // Return Value:
    // - 0 if the message is handled (e.g. context menu shown), otherwise falls back to DefSubclassProc.
    [[nodiscard]] LRESULT WindowContextMenu::_ContextMenuMessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept
    {
        switch (message)
        {
        case WM_STYLECHANGED:
        {
            if (wparam == GWL_STYLE)
            {
                const auto style = reinterpret_cast<STYLESTRUCT*>(lparam);
                _IsResizable(WI_IsFlagSet(style->styleNew, WS_THICKFRAME));
            }
            break;
        }
        case WM_NCRBUTTONUP:
        case WM_NCRBUTTONDOWN:
        case WM_CONTEXTMENU:
            _ShowMenu(lparam);
            return 0;
        case WM_SIZE:
            _IsMaximized(wparam == SIZE_MAXIMIZED);
            break;
        case WM_NCDESTROY:
            _RemoveContextMenuSubClassProc();
            break;
        }
        return DefSubclassProc(_parentWindow, message, wparam, lparam);
    }

    // Method Description:
    // - Safely removes the window subclass hook from the parent window if it's currently attached.
    // Return Value:
    // - <none>
    void WindowContextMenu::_RemoveContextMenuSubClassProc()
    {
        if (std::exchange(_subClassIsPresent, false) && _parentWindow)
        {
            RemoveWindowSubclass(_parentWindow, &WindowContextMenu::_StaticContextMenuSubClassProc, contextMenuSubClassId);
        }
    }

    // Method Description:
    // - Displays the XAML context menu. Translates physical screen coordinates from the
    //   Win32 message into DPI-aware logical coordinates for XAML. Also handles keyboard
    //   invocation (Shift+F10) which provides -1, -1 for coordinates.
    // Arguments:
    // - lparam: contains the x and y screen coordinates, or -1 for keyboard invocation.
    // Return Value:
    // - <none>
    void WindowContextMenu::_ShowMenu(LPARAM const lparam)
    {
        if (auto systemMenu{ GetSystemMenu(_parentWindow, FALSE) })
        {
            SendMessageW(_parentWindow, WM_INITMENU, reinterpret_cast<WPARAM>(systemMenu), 0);
            SendMessageW(_parentWindow, WM_INITMENUPOPUP, reinterpret_cast<WPARAM>(systemMenu), MAKELPARAM(0, TRUE));

            const auto systemMenuItemCount = GetMenuItemCount(systemMenu);
            if (systemMenuItemCount > contextMenuItemCount)
            {
                _AddMenuItems(systemMenu, systemMenuItemCount);
            }
        }

        til::point point{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
        if (point.x == -1 && point.y == -1)
        {
            // Keyboard invocation: position the menu at the center of the window.
            til::rect rect;
            GetWindowRect(_parentWindow, rect.as_win32_rect());

            point.x = rect.left + (rect.right - rect.left) / 2;
            point.y = rect.top + (rect.bottom - rect.top) / 2;
        }
        ScreenToClient(_parentWindow, point.as_win32_point());

        const float scale = static_cast<float>(GetDpiForWindow(_parentWindow)) / USER_DEFAULT_SCREEN_DPI;
        point.x = gsl::narrow_cast<til::CoordType>(std::lround(point.x / scale));
        point.y = gsl::narrow_cast<til::CoordType>(std::lround(point.y / scale));

        __super::ShowAt(_TargetElement, point.to_winrt_point());
    }

    // Method Description:
    // - Attaches the subclass hook to the provided parent window, queries its initial
    //   maximized/resizable states, and pulls the localized strings from the Win32 system menu.
    // Arguments:
    // - handle: the HWND of the window to attach to
    // Return Value:
    // - <none>
    void WindowContextMenu::_AttachToParentWindow(gsl::not_null<HWND> handle)
    {
        _parentWindow = handle.get();

        _subClassIsPresent = SetWindowSubclass(
            _parentWindow,
            &WindowContextMenu::_StaticContextMenuSubClassProc,
            contextMenuSubClassId,
            reinterpret_cast<DWORD_PTR>(this));

        _IsMaximized(IsZoomed(_parentWindow));

        const auto style = GetWindowLongPtrW(_parentWindow, GWL_STYLE);
        _IsResizable(WI_IsFlagSet(style, WS_THICKFRAME));

        auto systemMenu{ GetSystemMenu(_parentWindow, FALSE) };

        if (systemMenu)
        {
            const auto systemMenuItemCount = GetMenuItemCount(systemMenu);

            if (LOG_LAST_ERROR_IF(systemMenuItemCount == -1))
            {
                return;
            }

            if (systemMenuItemCount >= nonResizableContextMenuItemCount)
            {
                RestoreMenuFlyoutItem().Text(_GetMenuItemText(systemMenu, restoreMenuItemIndex));
                CloseMenuFlyoutItem().Text(_GetMenuItemText(systemMenu, closeMenuItemIndex));
            }

            if (systemMenuItemCount >= contextMenuItemCount)
            {
                MoveMenuFlyoutItem().Text(_GetMenuItemText(systemMenu, moveMenuItemIndex));
                SizeMenuFlyoutItem().Text(_GetMenuItemText(systemMenu, sizeMenuItemIndex));
                MinimizeMenuFlyoutItem().Text(_GetMenuItemText(systemMenu, minimizeMenuItemIndex));
                MaximizeMenuFlyoutItem().Text(_GetMenuItemText(systemMenu, maximizeMenuItemIndex));
            }
        }
    }

    // Method Description:
    // - Updates the visibility of the resizing-related context menu items (Size, Maximize, etc.)
    //   based on whether the window currently has the WS_THICKFRAME style.
    // Arguments:
    // - value: true if the window is resizable
    // Return Value:
    // - <none>
    void WindowContextMenu::_IsResizable(const bool value)
    {
        const auto visibility = value ?
                                    winrt::Windows::UI::Xaml::Visibility::Visible :
                                    winrt::Windows::UI::Xaml::Visibility::Collapsed;

        RestoreMenuFlyoutItem().Visibility(visibility);
        SizeMenuFlyoutItem().Visibility(visibility);
        MaximizeMenuFlyoutItem().Visibility(visibility);
        MinimizeMenuFlyoutItem().Visibility(visibility);
        ContextMenuSeparator().Visibility(visibility);
    }

    // Method Description:
    // - Toggles the enabled state of the Restore/Size/Maximize menu items based on
    //   the window's maximized state.
    // Arguments:
    // - value: true if the window is currently maximized
    // Return Value:
    // - <none>
    void WindowContextMenu::_IsMaximized(const bool value)
    {
        RestoreMenuFlyoutItem().IsEnabled(value);
        SizeMenuFlyoutItem().IsEnabled(!value);
        MaximizeMenuFlyoutItem().IsEnabled(!value);
    }

    // Method Description:
    // - Dynamically appends extra items from the Win32 system menu (e.g. from third-party tools)
    //   to the XAML menu flyout.
    // Arguments:
    // - menu: the system menu handle to inspect
    // - itemsCount: the total number of items in the system menu
    // Return Value:
    // - <none>
    void WindowContextMenu::_AddMenuItems(gsl::not_null<HMENU> menu, int32_t itemsCount)
    {
        auto menuItems = Items();

        while (menuItems.Size() > contextMenuItemCount)
        {
            menuItems.RemoveAtEnd();
        }

        for (int32_t i = contextMenuItemCount; i < itemsCount; ++i)
        {
            MENUITEMINFOW info{
                .cbSize = sizeof(MENUITEMINFOW),
                .fMask = MIIM_FTYPE | MIIM_ID | MIIM_STATE
            };

            if (!GetMenuItemInfoW(menu, i, TRUE, &info))
            {
                continue;
            }

            if (WI_IsFlagSet(info.fType, MFT_SEPARATOR))
            {
                menuItems.Append(winrt::Windows::UI::Xaml::Controls::MenuFlyoutSeparator{});
            }
            else
            {
                auto menuFlyoutItem = winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem{};
                menuFlyoutItem.Height(32.0);
                menuFlyoutItem.Padding(winrt::Windows::UI::Xaml::Thickness{ 8, 5, 5, 5 });
                menuFlyoutItem.Text(_GetMenuItemText(menu, i));

                if ((info.fState & MFS_DISABLED) != 0 || (info.fState & MFS_GRAYED) != 0)
                {
                    menuFlyoutItem.IsEnabled(false);
                }

                uint32_t cmdId = info.wID;
                menuFlyoutItem.Click([weak = get_weak(), parent = _parentWindow, cmdId](auto&&, auto&&) {
                    if (auto strongThis{ weak.get() })
                    {
                        strongThis->Hide();
                    }
                    LOG_IF_WIN32_BOOL_FALSE(PostMessageW(parent, WM_SYSCOMMAND, cmdId, 0));
                });
                menuItems.Append(menuFlyoutItem);
            }
        }
    }

    // Method Description:
    // - Cleans up the menu text by collapsing escaped ampersands (&& -> &),
    //   ensuring the string renders appropriately in the XAML UI.
    // Arguments:
    // - text: the string to be cleaned in-place
    // Return Value:
    // - <none>
    void WindowContextMenu::_CleanMenuItemText(std::wstring& text)
    {
        uint32_t writePos{ 0 };

        for (uint32_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == L'&')
            {
                if (i + 1 < text.size() && text[i + 1] == L'&')
                {
                    text[writePos++] = L'&';
                    ++i;
                }
            }
            else
            {
                text[writePos++] = text[i];
            }
        }
        text.resize(writePos);
    }

    // Method Description:
    // - Queries the Win32 system menu for the localized string at a specific index,
    //   and cleans it up (removing tab separators and fixing ampersands) for XAML.
    // Arguments:
    // - menu: the system menu handle
    // - itemIndex: the index of the item to fetch
    // Return Value:
    // - The cleaned menu item text as a std::wstring, or an empty string on failure.
    std::wstring WindowContextMenu::_GetMenuItemText(gsl::not_null<HMENU> menu, uint32_t itemIndex)
    {
        MENUITEMINFOW menuItemInfo{
            .cbSize = sizeof(MENUITEMINFOW),
            .fMask = MIIM_STRING
        };

        if (LOG_LAST_ERROR_IF(!GetMenuItemInfoW(menu.get(), itemIndex, TRUE, &menuItemInfo) || menuItemInfo.cch == 0))
        {
            return {};
        }

        std::wstring menuItemText(menuItemInfo.cch + 1, L'\0');
        menuItemInfo.dwTypeData = menuItemText.data();
        menuItemInfo.cch++;

        if (LOG_LAST_ERROR_IF(!GetMenuItemInfoW(menu.get(), itemIndex, TRUE, &menuItemInfo)))
        {
            return {};
        }

        menuItemText.resize(menuItemInfo.cch);

        // Strip out the shortcut keys usually separated by a tab
        if (const auto tabPos = menuItemText.find(L'\t'); tabPos != std::wstring::npos)
        {
            menuItemText.resize(tabPos);
        }

        _CleanMenuItemText(menuItemText);
        return menuItemText;
    }

    // XAML Click event handlers for standard system menu commands
    void WindowContextMenu::RestoreMenuItemOnClick(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        Hide();
        LOG_IF_WIN32_BOOL_FALSE(PostMessageW(_parentWindow, WM_SYSCOMMAND, SC_RESTORE, 0));
    }

    void WindowContextMenu::MoveMenuItemOnClick(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        Hide();
        LOG_IF_WIN32_BOOL_FALSE(PostMessageW(_parentWindow, WM_SYSCOMMAND, SC_MOVE, 0));
    }

    void WindowContextMenu::SizeMenuItemOnClick(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        Hide();
        LOG_IF_WIN32_BOOL_FALSE(PostMessageW(_parentWindow, WM_SYSCOMMAND, SC_SIZE, 0));
    }

    void WindowContextMenu::MinimizeMenuItemOnClick(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        Hide();
        LOG_IF_WIN32_BOOL_FALSE(PostMessageW(_parentWindow, WM_SYSCOMMAND, SC_MINIMIZE, 0));
    }

    void WindowContextMenu::MaximizeMenuItemOnClick(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        Hide();
        LOG_IF_WIN32_BOOL_FALSE(PostMessageW(_parentWindow, WM_SYSCOMMAND, SC_MAXIMIZE, 0));
    }

    void WindowContextMenu::CloseMenuItemOnClick(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        Hide();
        LOG_IF_WIN32_BOOL_FALSE(PostMessageW(_parentWindow, WM_SYSCOMMAND, SC_CLOSE, 0));
    }
}

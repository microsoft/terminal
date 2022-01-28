// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "icon.h"
#include "NotificationIcon.h"
#include "CustomWindowMessages.h"

#include <ScopedResourceLoader.h>
#include <LibraryResources.h>

using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal;

NotificationIcon::NotificationIcon(const HWND owningHwnd) :
    _owningHwnd{ owningHwnd }
{
    CreateNotificationIcon();
}

NotificationIcon::~NotificationIcon()
{
    RemoveIconFromNotificationArea();
}

void NotificationIcon::_CreateWindow()
{
    WNDCLASSW wc{};
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hInstance = wil::GetModuleInstanceHandle();
    wc.lpszClassName = L"NOTIFICATION_ICON_HOSTING_WINDOW_CLASS";
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hIcon = static_cast<HICON>(GetActiveAppIconHandle(true));
    RegisterClass(&wc);

    _notificationIconHwnd = wil::unique_hwnd(CreateWindowW(wc.lpszClassName,
                                                           wc.lpszClassName,
                                                           WS_DISABLED,
                                                           CW_USEDEFAULT,
                                                           CW_USEDEFAULT,
                                                           CW_USEDEFAULT,
                                                           CW_USEDEFAULT,
                                                           HWND_MESSAGE,
                                                           nullptr,
                                                           wc.hInstance,
                                                           nullptr));

    WINRT_VERIFY(_notificationIconHwnd);
}

// Method Description:
// - Creates and adds an icon to the notification area.
// If an icon already exists, update the HWND associated
// to the icon with this window's HWND.
// Arguments:
// - <unused>
// Return Value:
// - <none>
void NotificationIcon::CreateNotificationIcon()
{
    if (!_notificationIconHwnd)
    {
        // Creating a disabled, non visible window just so we can set it
        // as the foreground window when showing the context menu.
        // This is done so that the context menu can be dismissed
        // when clicking outside of it.
        _CreateWindow();
    }

    NOTIFYICONDATA nid{};
    nid.cbSize = sizeof(NOTIFYICONDATA);

    // This HWND will receive the callbacks sent by the notification icon.
    nid.hWnd = _owningHwnd;

    // App-defined identifier of the icon. The HWND and ID are used
    // to identify which icon to operate on when calling Shell_NotifyIcon.
    // Multiple icons can be associated with one HWND, but here we're only
    // going to be showing one so the ID doesn't really matter.
    nid.uID = 1;

    nid.uCallbackMessage = CM_NOTIFY_FROM_NOTIFICATION_AREA;

    // AppName happens to be in the ContextMenu's Resources, see GH#12264
    ScopedResourceLoader loader{ L"TerminalApp/ContextMenu" };
    const auto appNameLoc = loader.GetLocalizedString(L"AppName");

    nid.hIcon = static_cast<HICON>(GetActiveAppIconHandle(true));
    StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), appNameLoc.c_str());
    nid.uFlags = NIF_MESSAGE | NIF_SHOWTIP | NIF_TIP | NIF_ICON;
    Shell_NotifyIcon(NIM_ADD, &nid);

    // For whatever reason, the NIM_ADD call doesn't seem to set the version
    // properly, resulting in us being unable to receive the expected notification
    // events. We actually have to make a separate NIM_SETVERSION call for it to
    // work properly.
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIcon(NIM_SETVERSION, &nid);

    _notificationIconData = nid;
}

// Method Description:
// - This creates our context menu and displays it at the given
//   screen coordinates.
// Arguments:
// - coord: The coordinates where we should be showing the context menu.
// - peasants: The map of all peasants that should be available in the context menu.
// Return Value:
// - <none>
void NotificationIcon::ShowContextMenu(const til::point coord,
                                       const IVectorView<winrt::Microsoft::Terminal::Remoting::PeasantInfo>& peasants)
{
    if (const auto hMenu = _CreateContextMenu(peasants))
    {
        // We'll need to set our window to the foreground before calling
        // TrackPopupMenuEx or else the menu won't dismiss when clicking away.
        SetForegroundWindow(_notificationIconHwnd.get());

        // User can select menu items with the left and right buttons.
        UINT uFlags = TPM_RIGHTBUTTON;

        // Nonzero if drop-down menus are right-aligned with the corresponding menu-bar item
        // 0 if the menus are left-aligned.
        if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
        {
            uFlags |= TPM_RIGHTALIGN;
        }
        else
        {
            uFlags |= TPM_LEFTALIGN;
        }

        TrackPopupMenuEx(hMenu, uFlags, coord.x, coord.y, _owningHwnd, NULL);
    }
}

// Method Description:
// - This creates the context menu for our notification icon.
// Arguments:
// - peasants: A map of all peasants' ID to their window name.
// Return Value:
// - The handle to the newly created context menu.
HMENU NotificationIcon::_CreateContextMenu(const IVectorView<winrt::Microsoft::Terminal::Remoting::PeasantInfo>& peasants)
{
    auto hMenu = CreatePopupMenu();
    if (hMenu)
    {
        MENUINFO mi{};
        mi.cbSize = sizeof(MENUINFO);
        mi.fMask = MIM_STYLE | MIM_APPLYTOSUBMENUS | MIM_MENUDATA;
        mi.dwStyle = MNS_NOTIFYBYPOS;
        mi.dwMenuData = NULL;
        SetMenuInfo(hMenu, &mi);

        // Focus Current Terminal Window
        AppendMenu(hMenu, MF_STRING, gsl::narrow<UINT_PTR>(NotificationIconMenuItemAction::FocusTerminal), RS_(L"NotificationIconFocusTerminal").c_str());
        AppendMenu(hMenu, MF_SEPARATOR, 0, L"");

        // Submenu for Windows
        if (auto submenu = CreatePopupMenu())
        {
            for (const auto& p : peasants)
            {
                std::wstringstream displayText;
                displayText << L"#" << p.Id;

                if (!p.TabTitle.empty())
                {
                    displayText << L": " << std::wstring_view{ p.TabTitle };
                }

                if (!p.Name.empty())
                {
                    displayText << L" [" << std::wstring_view{ p.Name } << L"]";
                }

                AppendMenu(submenu, MF_STRING, gsl::narrow<UINT_PTR>(p.Id), displayText.str().c_str());
            }

            MENUINFO submenuInfo{};
            submenuInfo.cbSize = sizeof(MENUINFO);
            submenuInfo.fMask = MIM_MENUDATA;
            submenuInfo.dwStyle = MNS_NOTIFYBYPOS;
            submenuInfo.dwMenuData = (UINT_PTR)NotificationIconMenuItemAction::SummonWindow;
            SetMenuInfo(submenu, &submenuInfo);

            AppendMenu(hMenu, MF_POPUP, (UINT_PTR)submenu, RS_(L"NotificationIconWindowSubmenu").c_str());
        }
    }
    return hMenu;
}

// Method Description:
// - This is the handler for when one of the menu items are selected within
//   the notification icon's context menu.
// Arguments:
// - menu: The handle to the menu that holds the menu item that was selected.
// - menuItemIndex: The index of the menu item within the given menu.
// Return Value:
// - <none>
void NotificationIcon::MenuItemSelected(const HMENU menu, const UINT menuItemIndex)
{
    // Check the menu's data for a specific action.
    MENUINFO mi{};
    mi.cbSize = sizeof(MENUINFO);
    mi.fMask = MIM_MENUDATA;
    GetMenuInfo(menu, &mi);
    if (mi.dwMenuData)
    {
        if (gsl::narrow<NotificationIconMenuItemAction>(mi.dwMenuData) == NotificationIconMenuItemAction::SummonWindow)
        {
            winrt::Microsoft::Terminal::Remoting::SummonWindowSelectionArgs args{};
            args.WindowID(GetMenuItemID(menu, menuItemIndex));
            args.SummonBehavior().ToggleVisibility(false);
            args.SummonBehavior().MoveToCurrentDesktop(false);
            args.SummonBehavior().ToMonitor(Remoting::MonitorBehavior::InPlace);
            _SummonWindowRequestedHandlers(args);
            return;
        }
    }

    // Now check the menu item itself for an action.
    const auto action = gsl::narrow<NotificationIconMenuItemAction>(GetMenuItemID(menu, menuItemIndex));
    switch (action)
    {
    case NotificationIconMenuItemAction::FocusTerminal:
    {
        winrt::Microsoft::Terminal::Remoting::SummonWindowSelectionArgs args{};
        args.SummonBehavior().ToggleVisibility(false);
        args.SummonBehavior().MoveToCurrentDesktop(false);
        args.SummonBehavior().ToMonitor(Remoting::MonitorBehavior::InPlace);
        _SummonWindowRequestedHandlers(args);
        break;
    }
    }
}

// Method Description:
// - This is the handler for when the notification icon itself is left-clicked.
// Arguments:
// - <none>
// Return Value:
// - <none>
void NotificationIcon::NotificationIconPressed()
{
    // No name in the args means summon the mru window.
    winrt::Microsoft::Terminal::Remoting::SummonWindowSelectionArgs args{};
    args.SummonBehavior().MoveToCurrentDesktop(false);
    args.SummonBehavior().ToMonitor(Remoting::MonitorBehavior::InPlace);
    args.SummonBehavior().ToggleVisibility(false);
    _SummonWindowRequestedHandlers(args);
}

// Method Description:
// - Re-add a notification icon using our currently saved notification icon data.
// Arguments:
// - <none>
// Return Value:
// - <none>
void NotificationIcon::ReAddNotificationIcon()
{
    Shell_NotifyIcon(NIM_ADD, &_notificationIconData);
    Shell_NotifyIcon(NIM_SETVERSION, &_notificationIconData);
}

// Method Description:
// - Deletes our notification icon.
// Arguments:
// - <none>
// Return Value:
// - <none>
void NotificationIcon::RemoveIconFromNotificationArea()
{
    Shell_NotifyIcon(NIM_DELETE, &_notificationIconData);
}

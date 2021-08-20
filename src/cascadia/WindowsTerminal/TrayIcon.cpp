// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "icon.h"
#include "TrayIcon.h"
#include "CustomWindowMessages.h"

#include <ScopedResourceLoader.h>
#include <LibraryResources.h>

using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal;

TrayIcon::TrayIcon(const HWND owningHwnd) :
    _owningHwnd{ owningHwnd }
{
    CreateTrayIcon();
}

TrayIcon::~TrayIcon()
{
    RemoveIconFromTray();
}

void TrayIcon::_CreateWindow()
{
    WNDCLASSW wc{};
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hInstance = wil::GetModuleInstanceHandle();
    wc.lpszClassName = L"TRAY_ICON_HOSTING_WINDOW_CLASS";
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hIcon = static_cast<HICON>(GetActiveAppIconHandle(true));
    RegisterClass(&wc);

    _trayIconHwnd = wil::unique_hwnd(CreateWindowW(wc.lpszClassName,
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

    WINRT_VERIFY(_trayIconHwnd);
}

// Method Description:
// - Creates and adds an icon to the notification tray.
// If an icon already exists, update the HWND associated
// to the icon with this window's HWND.
// Arguments:
// - <unused>
// Return Value:
// - <none>
void TrayIcon::CreateTrayIcon()
{
    if (!_trayIconHwnd)
    {
        // Creating a disabled, non visible window just so we can set it
        // as the foreground window when showing the context menu.
        // This is done so that the context menu can be dismissed
        // when clicking outside of it.
        _CreateWindow();
    }

    NOTIFYICONDATA nid{};
    nid.cbSize = sizeof(NOTIFYICONDATA);

    // This HWND will receive the callbacks sent by the tray icon.
    nid.hWnd = _owningHwnd;

    // App-defined identifier of the icon. The HWND and ID are used
    // to identify which icon to operate on when calling Shell_NotifyIcon.
    // Multiple icons can be associated with one HWND, but here we're only
    // going to be showing one so the ID doesn't really matter.
    nid.uID = 1;

    nid.uCallbackMessage = CM_NOTIFY_FROM_TRAY;

    // AppName happens to be in CascadiaPackage's Resources.
    ScopedResourceLoader loader{ L"Resources" };
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

    _trayIconData = nid;
}

// Method Description:
// - This creates our context menu and displays it at the given
//   screen coordinates.
// Arguments:
// - coord: The coordinates where we should be showing the context menu.
// - peasants: The map of all peasants that should be available in the context menu.
// Return Value:
// - <none>
void TrayIcon::ShowTrayContextMenu(const til::point coord,
                                   IMapView<uint64_t, winrt::hstring> peasants)
{
    if (const auto hMenu = _CreateTrayContextMenu(peasants))
    {
        // We'll need to set our window to the foreground before calling
        // TrackPopupMenuEx or else the menu won't dismiss when clicking away.
        SetForegroundWindow(_trayIconHwnd.get());

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

        TrackPopupMenuEx(hMenu, uFlags, gsl::narrow_cast<int>(coord.x()), gsl::narrow_cast<int>(coord.y()), _owningHwnd, NULL);
    }
}

// Method Description:
// - This creates the context menu for our tray icon.
// Arguments:
// - peasants: A map of all peasants' ID to their window name.
// Return Value:
// - The handle to the newly created context menu.
HMENU TrayIcon::_CreateTrayContextMenu(IMapView<uint64_t, winrt::hstring> peasants)
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
        AppendMenu(hMenu, MF_STRING, gsl::narrow<UINT_PTR>(TrayMenuItemAction::FocusTerminal), RS_(L"TrayIconFocusTerminal").c_str());
        AppendMenu(hMenu, MF_SEPARATOR, 0, L"");

        // Submenu for Windows
        if (auto submenu = CreatePopupMenu())
        {
            const auto locWindow = RS_(L"WindowIdLabel");
            const auto locUnnamed = RS_(L"UnnamedWindowName");
            for (const auto [id, name] : peasants)
            {
                winrt::hstring displayText = name;
                if (name.empty())
                {
                    displayText = fmt::format(L"{} {} - <{}>", locWindow, id, locUnnamed);
                }

                AppendMenu(submenu, MF_STRING, gsl::narrow<UINT_PTR>(id), displayText.c_str());
            }

            MENUINFO submenuInfo{};
            submenuInfo.cbSize = sizeof(MENUINFO);
            submenuInfo.fMask = MIM_MENUDATA;
            submenuInfo.dwStyle = MNS_NOTIFYBYPOS;
            submenuInfo.dwMenuData = (UINT_PTR)TrayMenuItemAction::SummonWindow;
            SetMenuInfo(submenu, &submenuInfo);

            AppendMenu(hMenu, MF_POPUP, (UINT_PTR)submenu, RS_(L"TrayIconWindowSubmenu").c_str());
        }
    }
    return hMenu;
}

// Method Description:
// - This is the handler for when one of the menu items are selected within
//   the tray icon's context menu.
// Arguments:
// - menu: The handle to the menu that holds the menu item that was selected.
// - menuItemIndex: The index of the menu item within the given menu.
// Return Value:
// - <none>
void TrayIcon::TrayMenuItemSelected(const HMENU menu, const UINT menuItemIndex)
{
    // Check the menu's data for a specific action.
    MENUINFO mi{};
    mi.cbSize = sizeof(MENUINFO);
    mi.fMask = MIM_MENUDATA;
    GetMenuInfo(menu, &mi);
    if (mi.dwMenuData)
    {
        if (gsl::narrow<TrayMenuItemAction>(mi.dwMenuData) == TrayMenuItemAction::SummonWindow)
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
    const auto action = gsl::narrow<TrayMenuItemAction>(GetMenuItemID(menu, menuItemIndex));
    switch (action)
    {
    case TrayMenuItemAction::FocusTerminal:
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
// - This is the handler for when the tray icon itself is left-clicked.
// Arguments:
// - <none>
// Return Value:
// - <none>
void TrayIcon::TrayIconPressed()
{
    // No name in the args means summon the mru window.
    winrt::Microsoft::Terminal::Remoting::SummonWindowSelectionArgs args{};
    args.SummonBehavior().MoveToCurrentDesktop(false);
    args.SummonBehavior().ToMonitor(Remoting::MonitorBehavior::InPlace);
    args.SummonBehavior().ToggleVisibility(false);
    _SummonWindowRequestedHandlers(args);
}

// Method Description:
// - Re-add a tray icon using our currently saved tray icon data.
// Arguments:
// - <none>
// Return Value:
// - <none>
void TrayIcon::ReAddTrayIcon()
{
    Shell_NotifyIcon(NIM_ADD, &_trayIconData);
    Shell_NotifyIcon(NIM_SETVERSION, &_trayIconData);
}

// Method Description:
// - Deletes our tray icon.
// Arguments:
// - <none>
// Return Value:
// - <none>
void TrayIcon::RemoveIconFromTray()
{
    Shell_NotifyIcon(NIM_DELETE, &_trayIconData);
}

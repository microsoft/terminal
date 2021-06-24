#include "pch.h"
#include "icon.h"
#include "TrayIcon.h"

#include <LibraryResources.h>

using namespace winrt::Windows::Foundation::Collections;

TrayIcon::TrayIcon(const HWND owningHwnd) :
    _owningHwnd{ owningHwnd }
{
    CreateTrayIcon();
}

TrayIcon::~TrayIcon()
{
    DestroyTrayIcon();
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

    nid.hIcon = static_cast<HICON>(GetActiveAppIconHandle(true));
    StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), RS_(L"AppName").c_str());
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
// - The coordinates where we should be showing the context menu.
// Return Value:
// - <none>
void TrayIcon::ShowTrayContextMenu(const til::point coord,
                                   IMapView<uint64_t, winrt::hstring> peasants)
{
    if (auto hmenu = _CreateTrayContextMenu())
    {
        // Submenu for Windows
        if (auto windowSubmenu = _CreateWindowSubmenu(peasants))
        {
            MENUINFO submenuInfo{};
            submenuInfo.cbSize = sizeof(MENUINFO);
            submenuInfo.fMask = MIM_MENUDATA;
            submenuInfo.dwStyle = MNS_NOTIFYBYPOS;
            submenuInfo.dwMenuData = gsl::narrow<UINT_PTR>(TrayMenuItemAction::SummonWindow);
            SetMenuInfo(windowSubmenu, &submenuInfo);

            AppendMenu(hmenu, MF_POPUP, reinterpret_cast<UINT_PTR>(windowSubmenu), RS_(L"TrayIconWindowSubmenu").c_str());
        }

        // We'll need to set our window to the foreground before calling
        // TrackPopupMenuEx or else the menu won't dismiss when clicking away.
        SetForegroundWindow(_owningHwnd);

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

        TrackPopupMenuEx(hmenu, uFlags, gsl::narrow<int>(coord.x()), gsl::narrow<int>(coord.y()), _owningHwnd, NULL);
    }
}

// Method Description:
// - This creates the context menu for our tray icon.
// Arguments:
// - <none>
// Return Value:
// - The handle to the newly created context menu.
HMENU TrayIcon::_CreateTrayContextMenu()
{
    auto hmenu = CreatePopupMenu();
    if (hmenu)
    {
        MENUINFO mi{};
        mi.cbSize = sizeof(MENUINFO);
        mi.fMask = MIM_STYLE | MIM_APPLYTOSUBMENUS | MIM_MENUDATA;
        mi.dwStyle = MNS_NOTIFYBYPOS;
        mi.dwMenuData = NULL;
        SetMenuInfo(hmenu, &mi);

        // Focus Current Terminal Window
        AppendMenu(hmenu, MF_STRING, gsl::narrow<UINT_PTR>(TrayMenuItemAction::FocusTerminal), RS_(L"TrayIconFocusTerminal").c_str());
        AppendMenu(hmenu, MF_SEPARATOR, 0, L"");
    }
    return hmenu;
}

// Method Description:
// - Create a menu with a menu item for each window available to summon.
//   If a window is unnamed, we'll use its ID but still mention that it's unnamed.
// Arguments:
// - <none>
// Return Value:
// - The handle to the newly created window submenu.
HMENU TrayIcon::_CreateWindowSubmenu(IMapView<uint64_t, winrt::hstring> peasants)
{
    if (auto hmenu = CreatePopupMenu())
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

            AppendMenu(hmenu, MF_STRING, gsl::narrow<UINT_PTR>(id), displayText.c_str());
        }
        return hmenu;
    }
    return nullptr;
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
        _SummonWindowRequestedHandlers(args);
        break;
    }
    }
}

void TrayIcon::TrayIconPressed()
{
    // No name in the args means summon the mru window.
    winrt::Microsoft::Terminal::Remoting::SummonWindowSelectionArgs args{};
    args.SummonBehavior().ToggleVisibility(false);
    _SummonWindowRequestedHandlers(args);
}

// Method Description:
// - Deletes our tray icon if we have one.
// Arguments:
// - <none>
// Return Value:
// - <none>
void TrayIcon::DestroyTrayIcon()
{
    // We currently have a tray icon, but after a settings change
    // we shouldn't have one. In this case, we'll need to destroy our
    // tray icon and show any hidden windows.
    // For the sake of simplicity in this rare case, let's just summon
    // all the windows.
    // TODO: Put this section inside of AppHost.
    //if (_windowManager.IsMonarch())
    //{
    //    _windowManager.SummonAllWindows();
    //}
    Shell_NotifyIcon(NIM_DELETE, &_trayIconData);
}

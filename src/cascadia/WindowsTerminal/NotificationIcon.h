// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../cascadia/inc/cppwinrt_utils.h"

// This enumerates all the possible actions
// that our notification icon context menu could do.
enum class NotificationIconMenuItemAction
{
    FocusTerminal, // Focus the MRU terminal.
    SummonWindow
};

class NotificationIcon
{
public:
    NotificationIcon() = delete;
    NotificationIcon(const HWND owningHwnd);
    ~NotificationIcon();

    void CreateNotificationIcon();
    void RemoveIconFromNotificationArea();
    void ReAddNotificationIcon();

    void NotificationIconPressed();
    void ShowContextMenu(const til::point& coord, IMapView<uint64_t, winrt::hstring> peasants);
    void MenuItemSelected(const HMENU menu, const UINT menuItemIndex);

    WINRT_CALLBACK(SummonWindowRequested, winrt::delegate<void(winrt::Microsoft::Terminal::Remoting::SummonWindowSelectionArgs)>);

private:
    void _CreateWindow();
    HMENU _CreateContextMenu(IMapView<uint64_t, winrt::hstring> peasant);

    wil::unique_hwnd _notificationIconHwnd;
    HWND _owningHwnd;
    NOTIFYICONDATA _notificationIconData;
};

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../cascadia/inc/cppwinrt_utils.h"

// This enumerates all the possible actions
// that our tray icon context menu could do.
enum class TrayMenuItemAction
{
    FocusTerminal, // Focus the MRU terminal.
    SummonWindow
};

class TrayIcon
{
public:
    TrayIcon() = delete;
    TrayIcon(const HWND owningHwnd);
    ~TrayIcon();

    void CreateTrayIcon();
    void RemoveIconFromTray();
    void ReAddTrayIcon();

    void TrayIconPressed();
    void ShowTrayContextMenu(const til::point coord, winrt::Windows::Foundation::Collections::IMapView<uint64_t, winrt::hstring> peasants);
    void TrayMenuItemSelected(const HMENU menu, const UINT menuItemIndex);

    WINRT_CALLBACK(SummonWindowRequested, winrt::delegate<void(winrt::Microsoft::Terminal::Remoting::SummonWindowSelectionArgs)>);

private:
    void _CreateWindow();
    HMENU _CreateTrayContextMenu(winrt::Windows::Foundation::Collections::IMapView<uint64_t, winrt::hstring> peasants);

    wil::unique_hwnd _trayIconHwnd;
    HWND _owningHwnd;
    NOTIFYICONDATA _trayIconData;
};

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
    void ShowTrayContextMenu(const til::point& coord, const winrt::Windows::Foundation::Collections::IVectorView<winrt::Microsoft::Terminal::Remoting::PeasantInfo>& peasants);
    void TrayMenuItemSelected(const HMENU menu, const UINT menuItemIndex);

    WINRT_CALLBACK(SummonWindowRequested, winrt::delegate<void(winrt::Microsoft::Terminal::Remoting::SummonWindowSelectionArgs)>);

private:
    void _CreateWindow();
    HMENU _CreateTrayContextMenu(const winrt::Windows::Foundation::Collections::IVectorView<winrt::Microsoft::Terminal::Remoting::PeasantInfo>& peasants);

    wil::unique_hwnd _trayIconHwnd;
    HWND _owningHwnd;
    NOTIFYICONDATA _trayIconData;
};

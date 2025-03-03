// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "BaseWindow.h"

struct SystemMenuItemInfo
{
    winrt::hstring label;
    winrt::delegate<void()> callback;
};

class IslandWindow :
    public BaseWindow<IslandWindow>
{
public:
    static bool IsCursorHidden() noexcept;
    static void HideCursor() noexcept;
    static void ShowCursorMaybe(const UINT message) noexcept;

    IslandWindow() noexcept;
    virtual ~IslandWindow() override;

    virtual void MakeWindow() noexcept;
    virtual void Close();

    virtual void OnSize(const UINT width, const UINT height);
    HWND GetInteropHandle() const;

    [[nodiscard]] virtual LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept override;

    [[nodiscard]] LRESULT OnNcCreate(WPARAM wParam, LPARAM lParam) noexcept override;

    void OnResize(const UINT width, const UINT height) override;
    void OnMinimize() override;
    void OnRestore() override;
    virtual void OnAppInitialized();
    virtual void SetContent(winrt::Windows::UI::Xaml::UIElement content);
    virtual void OnApplicationThemeChanged(const winrt::Windows::UI::Xaml::ElementTheme& requestedTheme);
    virtual til::rect GetNonClientFrame(const UINT dpi) const noexcept;
    virtual til::size GetTotalNonClientExclusiveSize(const UINT dpi) const noexcept;

    virtual void Initialize();

    void SetCreateCallback(std::function<void(const HWND, const til::rect&)> pfn) noexcept;

    void SetSnapDimensionCallback(std::function<float(bool widthOrHeight, float dimension)> pfn) noexcept;

    void FocusModeChanged(const bool focusMode);
    void FullscreenChanged(const bool fullscreen);
    void SetAlwaysOnTop(const bool alwaysOnTop);
    void ShowWindowChanged(const bool showOrHide);
    virtual void SetShowTabsFullscreen(const bool newShowTabsFullscreen);

    void FlashTaskbar();
    void SetTaskbarProgress(const size_t state, const size_t progress);

    void SummonWindow(winrt::TerminalApp::SummonWindowBehavior args);

    bool IsQuakeWindow() const noexcept;
    void IsQuakeWindow(bool isQuakeWindow) noexcept;
    void SetAutoHideWindow(bool autoHideWindow) noexcept;

    void HideWindow();

    void SetMinimizeToNotificationAreaBehavior(bool MinimizeToNotificationArea) noexcept;

    void OpenSystemMenu(const std::optional<int> mouseX, const std::optional<int> mouseY) const noexcept;
    void AddToSystemMenu(const winrt::hstring& itemLabel, winrt::delegate<void()> callback);
    void RemoveFromSystemMenu(const winrt::hstring& itemLabel);

    void UseDarkTheme(const bool v);
    virtual void UseMica(const bool newValue, const double titlebarOpacity);

    til::event<winrt::delegate<>> DragRegionClicked;
    til::event<winrt::delegate<>> WindowCloseButtonClicked;
    til::event<winrt::delegate<void(winrt::Windows::Foundation::Point, int32_t)>> MouseScrolled;
    til::event<winrt::delegate<void(bool)>> WindowActivated;
    til::event<winrt::delegate<void()>> NotifyNotificationIconPressed;
    til::event<winrt::delegate<void()>> NotifyWindowHidden;
    til::event<winrt::delegate<void(HMENU, UINT)>> NotifyNotificationIconMenuItemSelected;
    til::event<winrt::delegate<void()>> NotifyReAddNotificationIcon;
    til::event<winrt::delegate<void()>> ShouldExitFullscreen;
    til::event<winrt::delegate<void(bool)>> MaximizeChanged;

    til::event<winrt::delegate<void()>> WindowMoved;
    til::event<winrt::delegate<void(bool)>> WindowVisibilityChanged;

protected:
    void ForceResize()
    {
        // Do a quick resize to force the island to paint
        const auto size = GetPhysicalSize();
        OnSize(size.width, size.height);
    }

    HWND _interopWindowHandle;

    winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource _source; // nulled in ctor
    winrt::Windows::UI::Xaml::Controls::Grid _rootGrid; // nulled in ctor
    wil::com_ptr<ITaskbarList3> _taskbar;

    std::function<void(const HWND, const til::rect&)> _pfnCreateCallback;
    std::function<float(bool, float)> _pfnSnapDimensionCallback;

    void _HandleCreateWindow(const WPARAM wParam, const LPARAM lParam) noexcept;
    [[nodiscard]] LRESULT _OnSizing(const WPARAM wParam, const LPARAM lParam);
    [[nodiscard]] LRESULT _OnMoving(const WPARAM wParam, const LPARAM lParam);

    bool _borderless{ false };
    bool _alwaysOnTop{ false };
    bool _fullscreen{ false };
    bool _showTabsFullscreen{ false };
    bool _fWasMaximizedBeforeFullscreen{ false };
    RECT _rcWindowBeforeFullscreen{};
    RECT _rcWorkBeforeFullscreen{};
    UINT _dpiBeforeFullscreen{ 96 };

    virtual void _SetIsBorderless(const bool borderlessEnabled);
    virtual void _SetIsFullscreen(const bool fullscreenEnabled);

    void _RestoreFullscreenPosition(const RECT& rcWork);
    void _SetFullscreenPosition(const RECT& rcMonitor, const RECT& rcWork);

    LONG _getDesiredWindowStyle() const;

    void _OnGetMinMaxInfo(const WPARAM wParam, const LPARAM lParam);

    void _globalActivateWindow(const uint32_t dropdownDuration,
                               const winrt::TerminalApp::MonitorBehavior toMonitor);
    void _dropdownWindow(const uint32_t dropdownDuration,
                         const winrt::TerminalApp::MonitorBehavior toMonitor);
    void _slideUpWindow(const uint32_t dropdownDuration);
    void _doSlideAnimation(const uint32_t dropdownDuration, const bool down);
    void _globalDismissWindow(const uint32_t dropdownDuration);

    static MONITORINFO _getMonitorForCursor();
    static MONITORINFO _getMonitorForWindow(HWND foregroundWindow);
    void _moveToMonitor(HWND foregroundWindow, const winrt::TerminalApp::MonitorBehavior toMonitor);
    void _moveToMonitorOfMouse();
    void _moveToMonitorOf(HWND foregroundWindow);
    void _moveToMonitor(const MONITORINFO activeMonitor);

    bool _isQuakeWindow{ false };
    bool _autoHideWindow{ false };

    void _enterQuakeMode();
    til::rect _getQuakeModeSize(HMONITOR hmon);

    bool _minimizeToNotificationArea{ false };

    std::unordered_map<UINT, SystemMenuItemInfo> _systemMenuItems;
    UINT _systemMenuNextItemId = 0;
    void _resetSystemMenu();

private:
    // This minimum width allows for width the tabs fit
    static constexpr float minimumWidth = 460;

    // We run with no height requirement for client area,
    // though the total height will take into account the non-client area
    // and the requirements of components hosted in the client area
    static constexpr float minimumHeight = 0;

    inline static bool _cursorHidden;
};

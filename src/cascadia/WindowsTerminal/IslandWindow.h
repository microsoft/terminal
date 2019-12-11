// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BaseWindow.h"
#include "../types/IUiaWindow.h"
#include "WindowUiaProvider.hpp"
#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include <winrt/TerminalApp.h>
#include "../../cascadia/inc/cppwinrt_utils.h"

class IslandWindow :
    public BaseWindow<IslandWindow>,
    public IUiaWindow
{
public:
    IslandWindow() noexcept;
    virtual ~IslandWindow() override;

    void MakeWindow() noexcept;
    void Close();
    virtual void OnSize(const UINT width, const UINT height);

    [[nodiscard]] virtual LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept override;
    IRawElementProviderSimple* _GetUiaProvider();
    void OnResize(const UINT width, const UINT height) override;
    void OnMinimize() override;
    void OnRestore() override;
    virtual void OnAppInitialized();
    virtual void SetContent(winrt::Windows::UI::Xaml::UIElement content);
    virtual void OnApplicationThemeChanged(const winrt::Windows::UI::Xaml::ElementTheme& requestedTheme);

    virtual void Initialize();

    void SetCreateCallback(std::function<void(const HWND, const RECT, winrt::TerminalApp::LaunchMode& launchMode)> pfn) noexcept;

    void ToggleFullscreen();

#pragma region IUiaWindow
    void ChangeViewport(const SMALL_RECT /*NewWindow*/)
    {
        // TODO GitHub #1352: Hook up ScreenInfoUiaProvider to WindowUiaProvider
        // Relevant comment from zadjii-msft:
        /*
        In my head for designing this, I'd then have IslandWindow::ChangeViewport
        call a callback that AppHost sets, where AppHost will then call into the
        TerminalApp to have TerminalApp handle the ChangeViewport call.
        (See IslandWindow::SetCreateCallback as an example of a similar
        pattern we're using today.) That way, if someone else were trying
        to resuse this, they could have their own AppHost (or TerminalApp
        equivalent) handle the ChangeViewport call their own way.
        */
        return;
    };

    HWND GetWindowHandle() const noexcept override
    {
        return BaseWindow::GetHandle();
    };

    [[nodiscard]] HRESULT SignalUia(_In_ EVENTID /*id*/) override { return E_NOTIMPL; };
    [[nodiscard]] HRESULT UiaSetTextAreaFocus() override { return E_NOTIMPL; };

    RECT GetWindowRect() const noexcept override
    {
        return BaseWindow::GetWindowRect();
    };

#pragma endregion

    DECLARE_EVENT(DragRegionClicked, _DragRegionClickedHandlers, winrt::delegate<>);
    DECLARE_EVENT(WindowCloseButtonClicked, _windowCloseButtonClickedHandler, winrt::delegate<>);

protected:
    void ForceResize()
    {
        // Do a quick resize to force the island to paint
        const auto size = GetPhysicalSize();
        OnSize(size.cx, size.cy);
    }

    HWND _interopWindowHandle;
    WindowUiaProvider* _pUiaProvider;

    winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource _source;

    winrt::Windows::UI::Xaml::Controls::Grid _rootGrid;

    std::function<void(const HWND, const RECT, winrt::TerminalApp::LaunchMode& launchMode)> _pfnCreateCallback;

    void _HandleCreateWindow(const WPARAM wParam, const LPARAM lParam) noexcept;

    bool _fullscreen{ false };
    RECT _fullscreenWindowSize;
    RECT _nonFullscreenWindowSize;

    virtual void _SetIsFullscreen(const bool fullscreenEnabled);
    void _BackupWindowSizes(const bool currentIsInFullscreen);
    void _ApplyWindowSize();

    // See _SetIsFullscreen for details on this method.
    virtual bool _ShouldUpdateStylesOnFullscreen() const { return true; };
};

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BaseWindow.h"
#include "../types/IConsoleWindow.hpp"
#include "../types/WindowUiaProvider.h"
#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include <winrt/TerminalApp.h>

class IslandWindow :
    public BaseWindow<IslandWindow>,
    public IConsoleWindow
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

    virtual void Initialize();

    void SetCreateCallback(std::function<void(const HWND, const RECT)> pfn) noexcept;

    void UpdateTheme(const winrt::Windows::UI::Xaml::ElementTheme& requestedTheme);
#pragma region IConsoleWindow

    BOOL EnableBothScrollBars() override
    {
        return false;
    };
    int UpdateScrollBar(_In_ bool isVertical,
                        _In_ bool isAltBuffer,
                        _In_ UINT pageSize,
                        _In_ int maxSize,
                        _In_ int viewportPosition) override { return -1; };

    bool IsInFullscreen() const override { return false; };
    void SetIsFullscreen(const bool fFullscreenEnabled) override{};
    void ChangeViewport(const SMALL_RECT NewWindow) override{};

    void CaptureMouse() override{};
    BOOL ReleaseMouse() override { return false; };

    HWND GetWindowHandle() const noexcept override
    {
        return BaseWindow::GetHandle();
    };

    void SetOwner() override{};

    BOOL GetCursorPosition(_Out_ LPPOINT lpPoint) override { return false; };
    BOOL GetClientRectangle(_Out_ LPRECT lpRect) override { return false; };
    int MapPoints(_Inout_updates_(cPoints) LPPOINT lpPoints, _In_ UINT cPoints) override { return -1; };
    BOOL ConvertScreenToClient(_Inout_ LPPOINT lpPoint) override { return false; };

    BOOL SendNotifyBeep() const override { return false; };

    BOOL PostUpdateScrollBars() const override { return false; };

    BOOL PostUpdateWindowSize() const override { return false; };

    void UpdateWindowSize(const COORD coordSizeInChars) override{};
    void UpdateWindowText() override{};

    void HorizontalScroll(const WORD wScrollCommand,
                          const WORD wAbsoluteChange) override{};
    void VerticalScroll(const WORD wScrollCommand,
                        const WORD wAbsoluteChange) override{};
    [[nodiscard]] HRESULT SignalUia(_In_ EVENTID id) override { return E_NOTIMPL; };
    [[nodiscard]] HRESULT UiaSetTextAreaFocus() override { return E_NOTIMPL; };

    RECT GetWindowRect() const noexcept override
    {
        return BaseWindow::GetWindowRect();
    };

#pragma endregion

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

    std::function<void(const HWND, const RECT)> _pfnCreateCallback;

    void _HandleCreateWindow(const WPARAM wParam, const LPARAM lParam) noexcept;
};

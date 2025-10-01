/*++
Copyright (c) Microsoft Corporation

Module Name:
- NonClientIslandWindow.h

Abstract:
- This class represents a window hosting two XAML Islands. One is in the client
  area of the window, as it is in the base IslandWindow class. The second is in
  the titlebar of the window, in the "non-client" area of the window. This
  enables an app to place xaml content in the titlebar.
- Placing content in the frame is enabled with DwmExtendFrameIntoClientArea. See
  https://docs.microsoft.com/en-us/windows/desktop/dwm/customframe
  for information on how this is done.

Author(s):
    Mike Griese (migrie) April-2019
--*/

#pragma once
#include "IslandWindow.h"

class NonClientIslandWindow : public IslandWindow
{
public:
    // this is the same for all DPIs
    static constexpr const int topBorderVisibleHeight = 1;

    NonClientIslandWindow(const winrt::Windows::UI::Xaml::ElementTheme& requestedTheme) noexcept;
    ~NonClientIslandWindow();

    virtual void Close() override;
    void MakeWindow() noexcept override;
    virtual void OnSize(const UINT width, const UINT height) override;

    [[nodiscard]] virtual LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept;

    virtual til::rect GetNonClientFrame(UINT dpi) const noexcept override;
    virtual til::size GetTotalNonClientExclusiveSize(UINT dpi) const noexcept override;

    void Initialize() override;

    void OnAppInitialized() override;
    void SetContent(winrt::Windows::UI::Xaml::UIElement content) override;
    void SetTitlebarContent(winrt::Windows::UI::Xaml::UIElement content);
    void OnApplicationThemeChanged(const winrt::Windows::UI::Xaml::ElementTheme& requestedTheme) override;

    void SetTitlebarBackground(winrt::Windows::UI::Xaml::Media::Brush brush);
    void SetShowTabsFullscreen(const bool newShowTabsFullscreen) override;

    virtual void UseMica(const bool newValue, const double titlebarOpacity) override;

private:
    std::optional<til::point> _oldIslandPos;

    winrt::TerminalApp::TitlebarControl _titlebar{ nullptr };

    wil::unique_hbrush _backgroundBrush;
    til::color _backgroundBrushColor;

    winrt::Windows::UI::Xaml::Controls::Border _dragBar{ nullptr };
    wil::unique_hwnd _dragBarWindow;

    winrt::Windows::UI::Xaml::ElementTheme _theme;

    bool _useMica{ false };
    double _titlebarOpacity{ 1.0 };

    bool _isMaximized;
    bool _trackingMouse{ false };

    [[nodiscard]] static LRESULT __stdcall _StaticInputSinkWndProc(HWND const window, UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept;
    [[nodiscard]] LRESULT _InputSinkMessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept;

    void _ResizeDragBarWindow() noexcept;

    int _GetResizeHandleHeight() const noexcept;
    til::rect _GetDragAreaRect() const noexcept;
    int _GetTopBorderHeight() const noexcept;
    LRESULT _dragBarNcHitTest(const til::point pointer);

    [[nodiscard]] LRESULT _OnNcCalcSize(const WPARAM wParam, const LPARAM lParam) noexcept;
    [[nodiscard]] LRESULT _OnNcHitTest(POINT ptMouse) const noexcept;
    [[nodiscard]] LRESULT _OnPaint() noexcept;
    [[nodiscard]] LRESULT _OnSetCursor(WPARAM wParam, LPARAM lParam) const noexcept;
    void _OnMaximizeChange() noexcept;
    void _OnDragBarSizeChanged(winrt::Windows::Foundation::IInspectable sender, winrt::Windows::UI::Xaml::SizeChangedEventArgs eventArgs);

    void _SetIsBorderless(const bool borderlessEnabled) override;
    void _SetIsFullscreen(const bool fullscreenEnabled) override;
    bool _IsTitlebarVisible() const;

    void _UpdateFrameMargins() const noexcept;
    void _UpdateMaximizedState();
    void _UpdateIslandPosition(const UINT windowWidth, const UINT windowHeight);
    void _UpdateTitlebarVisibility();

    struct Revokers
    {
        winrt::Windows::UI::Xaml::Controls::Border::SizeChanged_revoker dragBarSizeChanged;
        winrt::Windows::UI::Xaml::Controls::Grid::SizeChanged_revoker rootGridSizeChanged;
        winrt::TerminalApp::TitlebarControl::Loaded_revoker titlebarLoaded;
    } _callbacks{};
};

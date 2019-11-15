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

#include "pch.h"
#include "IslandWindow.h"
#include "../../types/inc/Viewport.hpp"
#include <dwmapi.h>
#include <wil/resource.h>

class NonClientIslandWindow : public IslandWindow
{
public:
    // this is the same for all DPIs
    static constexpr const int topBorderVisibleHeight = 1;

    NonClientIslandWindow(const winrt::Windows::UI::Xaml::ElementTheme& requestedTheme) noexcept;
    virtual ~NonClientIslandWindow() override;

    virtual void OnSize(const UINT width, const UINT height) override;

    [[nodiscard]] virtual LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept override;

    void Initialize() override;

    void OnAppInitialized() override;
    void SetContent(winrt::Windows::UI::Xaml::UIElement content) override;
    void SetTitlebarContent(winrt::Windows::UI::Xaml::UIElement content);
    void OnApplicationThemeChanged(const winrt::Windows::UI::Xaml::ElementTheme& requestedTheme) override;

private:
    std::optional<COORD> _oldIslandPos;

    winrt::TerminalApp::TitlebarControl _titlebar{ nullptr };
    winrt::Windows::UI::Xaml::UIElement _clientContent{ nullptr };

    wil::unique_hbrush _backgroundBrush;
    COLORREF _backgroundBrushColor;

    winrt::Windows::UI::Xaml::Controls::Border _dragBar{ nullptr };
    wil::unique_hrgn _dragBarRegion;

    winrt::Windows::UI::Xaml::ElementTheme _theme;

    bool _isMaximized;

    int _GetResizeHandleHeight() const noexcept;
    RECT _GetDragAreaRect() const noexcept;
    int _GetTopBorderHeight() const noexcept;

    [[nodiscard]] LRESULT _OnNcCreate(WPARAM wParam, LPARAM lParam) noexcept override;
    [[nodiscard]] LRESULT _OnNcCalcSize(const WPARAM wParam, const LPARAM lParam) noexcept;
    [[nodiscard]] LRESULT _OnNcHitTest(POINT ptMouse) const noexcept;
    [[nodiscard]] LRESULT _OnPaint() noexcept;
    void _OnMaximizeChange() noexcept;
    void _OnDragBarSizeChanged(winrt::Windows::Foundation::IInspectable sender, winrt::Windows::UI::Xaml::SizeChangedEventArgs eventArgs) const;

    void _SetIsFullscreen(const bool fFullscreenEnabled) override;
    // See IslandWindow::_SetIsFullscreen for details on this method.
    bool _ShouldUpdateStylesOnFullscreen() const override { return false; };
    bool _IsTitlebarVisible() const;

    void _UpdateFrameMargins() const noexcept;
    void _UpdateMaximizedState();
    void _UpdateIslandPosition(const UINT windowWidth, const UINT windowHeight);
    void _UpdateIslandRegion() const;
    void _UpdateFrameTheme() const;
};

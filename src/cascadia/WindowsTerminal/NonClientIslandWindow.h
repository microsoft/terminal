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
#include <wil\resource.h>

class NonClientIslandWindow : public IslandWindow
{
public:
    NonClientIslandWindow() noexcept;
    virtual ~NonClientIslandWindow() override;

    virtual void OnSize(const UINT width, const UINT height) override;

    [[nodiscard]] virtual LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept override;

    int GetTopBorderHeight();

    void Initialize() override;

    void OnAppInitialized() override;
    void SetContent(winrt::Windows::UI::Xaml::UIElement content) override;
    void SetTitlebarContent(winrt::Windows::UI::Xaml::UIElement content);

private:
    int _oldIslandX, _oldIslandY;

    winrt::TerminalApp::TitlebarControl _titlebar{ nullptr };
    winrt::Windows::UI::Xaml::UIElement _clientContent{ nullptr };

    wil::unique_hbrush _backgroundBrush;
    wil::unique_hrgn _dragBarRegion;

    bool _isMaximized;
    winrt::Windows::UI::Xaml::Controls::Border _dragBar{ nullptr };

    RECT _GetDragAreaRect() const noexcept;

    [[nodiscard]] LRESULT _OnNcCalcSize(const WPARAM wParam, const LPARAM lParam) noexcept;
    [[nodiscard]] LRESULT _OnNcHitTest(POINT ptMouse) const noexcept;

    [[nodiscard]] HRESULT _UpdateFrameMargins() const noexcept;

    void _HandleActivateWindow();

    void _OnMaximizeChange() noexcept;

    void _UpdateMaximizedState();
    void _UpdateIslandPosition(const UINT windowWidth, const UINT windowHeight);
    void _UpdateDragRegion();
    void _UpdateFrameTheme();

    virtual void _OnNcCreate() noexcept;


    void OnDragBarSizeChanged(winrt::Windows::Foundation::IInspectable sender, winrt::Windows::UI::Xaml::SizeChangedEventArgs eventArgs);
};

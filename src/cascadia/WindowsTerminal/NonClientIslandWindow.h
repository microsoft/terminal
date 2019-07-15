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
#include <windowsx.h>
#include <wil\resource.h>

class NonClientIslandWindow : public IslandWindow
{
public:
    NonClientIslandWindow() noexcept;
    virtual ~NonClientIslandWindow() override;

    virtual void OnSize(const UINT width, const UINT height) override;

    [[nodiscard]] virtual LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept override;

    MARGINS GetFrameMargins() const noexcept;

    void Initialize() override;

    void OnAppInitialized() override;
    void SetContent(winrt::Windows::UI::Xaml::UIElement content) override;
    void SetTitlebarContent(winrt::Windows::UI::Xaml::UIElement content);

private:
    winrt::TerminalApp::TitlebarControl _titlebar{ nullptr };
    winrt::Windows::UI::Xaml::UIElement _clientContent{ nullptr };

    wil::unique_hbrush _backgroundBrush;
    wil::unique_hrgn _dragBarRegion;

    MARGINS _maximizedMargins = { 0 };
    bool _isMaximized;
    winrt::Windows::UI::Xaml::Controls::Border _dragBar{ nullptr };

    RECT GetDragAreaRect() const noexcept;

    [[nodiscard]] LRESULT HitTestNCA(POINT ptMouse) const noexcept;

    [[nodiscard]] HRESULT _UpdateFrameMargins() const noexcept;

    void _HandleActivateWindow();
    bool _HandleWindowPosChanging(WINDOWPOS* const windowPos);
    void _UpdateDragRegion();

    void OnDragBarSizeChanged(winrt::Windows::Foundation::IInspectable sender, winrt::Windows::UI::Xaml::SizeChangedEventArgs eventArgs);

    RECT GetMaxWindowRectInPixels(const RECT* const prcSuggested, _Out_opt_ UINT* pDpiSuggested);
};

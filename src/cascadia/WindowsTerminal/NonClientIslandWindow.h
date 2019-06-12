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

class NonClientIslandWindow : public IslandWindow
{
public:
    NonClientIslandWindow() noexcept;
    virtual ~NonClientIslandWindow() override;

    virtual void OnSize() override;

    [[nodiscard]] virtual LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept override;

    void SetNonClientContent(winrt::Windows::UI::Xaml::UIElement content);

    virtual void Initialize() override;

    MARGINS GetFrameMargins() const noexcept;

    void SetNonClientHeight(const int contentHeight) noexcept;

private:
    winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource _nonClientSource;

    HWND _nonClientInteropWindowHandle;
    winrt::Windows::UI::Xaml::Controls::Grid _nonClientRootGrid;

    int _windowMarginBottom = 2;
    int _windowMarginSides = 2;
    int _titlebarMarginRight = 0;
    int _titlebarMarginTop = 2;
    int _titlebarMarginBottom = 0;

    int _titlebarUnscaledContentHeight = 0;

    ::Microsoft::Console::Types::Viewport GetTitlebarContentArea() const noexcept;
    ::Microsoft::Console::Types::Viewport GetClientContentArea() const noexcept;

    MARGINS _maximizedMargins;
    bool _isMaximized;

    [[nodiscard]] LRESULT HitTestNCA(POINT ptMouse) const noexcept;

    [[nodiscard]] HRESULT _UpdateFrameMargins() const noexcept;

    void _HandleActivateWindow();
    bool _HandleWindowPosChanging(WINDOWPOS* const windowPos);

    RECT GetMaxWindowRectInPixels(const RECT* const prcSuggested, _Out_opt_ UINT* pDpiSuggested);
};

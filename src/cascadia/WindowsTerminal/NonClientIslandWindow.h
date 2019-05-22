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

    virtual LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept override;

    MARGINS GetFrameMargins() const noexcept;

    static double NonClientDragBarWidth;

private:

    MARGINS _maximizedMargins = { 0 };
    bool _isMaximized;

    LRESULT HitTestNCA(POINT ptMouse) const noexcept;
    HRESULT _UpdateFrameMargins() const noexcept;

    void _HandleActivateWindow();
    bool _HandleWindowPosChanging(WINDOWPOS* const windowPos);

    RECT GetMaxWindowRectInPixels(const RECT * const prcSuggested, _Out_opt_ UINT * pDpiSuggested);
};

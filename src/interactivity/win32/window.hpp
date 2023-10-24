/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- window.hpp

Abstract:
- This module is used for managing the main console window

Author(s):
- Michael Niksa (MiNiksa) 14-Oct-2014
- Paul Campbell (PaulCam) 14-Oct-2014
--*/
#pragma once

#include "../inc/IConsoleWindow.hpp"

namespace Microsoft::Console::Render::Atlas
{
    class AtlasEngine;
}

namespace Microsoft::Console::Render
{
    using AtlasEngine = Atlas::AtlasEngine;
    class DxEngine;
    class GdiEngine;
}

namespace Microsoft::Console::Interactivity::Win32
{
    class WindowUiaProvider;

    class Window final : public Microsoft::Console::Types::IConsoleWindow
    {
    public:
        [[nodiscard]] static NTSTATUS CreateInstance(_In_ Settings* const pSettings,
                                                     _In_ SCREEN_INFORMATION* const pScreen);

        [[nodiscard]] NTSTATUS ActivateAndShow(const WORD wShowWindow);

        ~Window();

        til::rect GetWindowRect() const noexcept;
        HWND GetWindowHandle() const;
        SCREEN_INFORMATION& GetScreenInfo();
        const SCREEN_INFORMATION& GetScreenInfo() const;

        BYTE GetWindowOpacity() const;
        void SetWindowOpacity(const BYTE bOpacity);
        void ApplyWindowOpacity() const;
        void ChangeWindowOpacity(const short sOpacityDelta);

        bool IsInMaximized() const;

        bool IsInFullscreen() const;
        void SetIsFullscreen(const bool fFullscreenEnabled);
        void ToggleFullscreen();

        void ChangeViewport(const til::inclusive_rect& NewWindow);

        void VerticalScroll(const WORD wScrollCommand,
                            const WORD wAbsoluteChange);
        void HorizontalScroll(const WORD wScrollCommand,
                              const WORD wAbsoluteChange);

        BOOL EnableBothScrollBars();
        int UpdateScrollBar(bool isVertical,
                            bool isAltBuffer,
                            UINT pageSize,
                            int maxSize,
                            int viewportPosition);

        void UpdateWindowSize(const til::size coordSizeInChars);
        void UpdateWindowPosition(_In_ const til::point ptNewPos) const;
        void UpdateWindowText();

        void CaptureMouse();
        BOOL ReleaseMouse();

        // Dispatchers (requests from other parts of the
        // console get dispatched onto the window message
        // queue/thread)
        BOOL SendNotifyBeep() const;

        BOOL PostUpdateScrollBars() const;
        BOOL PostUpdateWindowSize() const;
        BOOL PostUpdateExtendedEditKeys() const;

        [[nodiscard]] HRESULT SignalUia(_In_ EVENTID id);

        void SetOwner();
        BOOL GetCursorPosition(_Out_ til::point* lpPoint);
        BOOL GetClientRectangle(_Out_ til::rect* lpRect);
        BOOL MapRect(_Inout_ til::rect* lpRect);
        BOOL ConvertScreenToClient(_Inout_ til::point* lpPoint);

        [[nodiscard]] HRESULT UiaSetTextAreaFocus();

    protected:
        // prevent accidental generation of copies
        Window(const Window&) = delete;
        void operator=(const Window&) = delete;

    private:
        Window();

        // Registration/init
        [[nodiscard]] static NTSTATUS s_RegisterWindowClass();
        [[nodiscard]] NTSTATUS _MakeWindow(_In_ Settings* const pSettings,
                                           _In_ SCREEN_INFORMATION* const pScreen);
        void _CloseWindow() const;

        static ATOM s_atomWindowClass;
        Settings* _pSettings;

        HWND _hWnd;

        Render::GdiEngine* pGdiEngine = nullptr;
#if TIL_FEATURE_CONHOSTDXENGINE_ENABLED
        Render::DxEngine* pDxEngine = nullptr;
#endif
#if TIL_FEATURE_CONHOSTATLASENGINE_ENABLED
        Render::AtlasEngine* pAtlasEngine = nullptr;
#endif

        [[nodiscard]] NTSTATUS _InternalSetWindowSize();
        void _UpdateWindowSize(const til::size sizeNew);

        void _UpdateSystemMetrics() const;

        // Wndproc
        [[nodiscard]] static LRESULT CALLBACK s_ConsoleWindowProc(_In_ HWND hwnd,
                                                                  _In_ UINT uMsg,
                                                                  _In_ WPARAM wParam,
                                                                  _In_ LPARAM lParam);
        [[nodiscard]] LRESULT CALLBACK ConsoleWindowProc(_In_ HWND,
                                                         _In_ UINT uMsg,
                                                         _In_ WPARAM wParam,
                                                         _In_ LPARAM lParam);

        // Wndproc helpers
        void _HandleDrop(const WPARAM wParam) const;
        [[nodiscard]] HRESULT _HandlePaint() const;
        void _HandleWindowPosChanged(const LPARAM lParam);

        // Accessibility/UI Automation
        [[nodiscard]] LRESULT _HandleGetObject(const HWND hwnd,
                                               const WPARAM wParam,
                                               const LPARAM lParam);
        IRawElementProviderSimple* _GetUiaProvider();
        WRL::ComPtr<WindowUiaProvider> _pUiaProvider;

        // Dynamic Settings helpers
        [[nodiscard]] static LRESULT s_RegPersistWindowPos(_In_ PCWSTR const pwszTitle,
                                                           const BOOL fAutoPos,
                                                           const Window* const pWindow);
        [[nodiscard]] static LRESULT s_RegPersistWindowOpacity(_In_ PCWSTR const pwszTitle,
                                                               const Window* const pWindow);

        // The size/position of the window on the most recent update.
        // This is remembered so we can figure out which
        // size the client was resized from.
        til::rect _rcClientLast;

        // Full screen
        void _RestoreFullscreenPosition(const RECT& rcWork);
        void _SetFullscreenPosition(const RECT& rcMonitor, const RECT& rcWork);
        bool _fIsInFullscreen;
        bool _fWasMaximizedBeforeFullscreen;
        RECT _rcWindowBeforeFullscreen;
        RECT _rcWorkBeforeFullscreen;
        UINT _dpiBeforeFullscreen;

        // math helpers
        void _CalculateWindowRect(const til::size coordWindowInChars,
                                  _Inout_ til::rect* const prectWindow) const;
        static void s_CalculateWindowRect(const til::size coordWindowInChars,
                                          const int iDpi,
                                          const til::size coordFontSize,
                                          const til::size coordBufferSize,
                                          _In_opt_ HWND const hWnd,
                                          _Inout_ til::rect* const prectWindow);

        static void s_ReinitializeFontsForDPIChange();

        bool _fInDPIChange = false;

        static void s_ConvertWindowPosToWindowRect(const LPWINDOWPOS lpWindowPos,
                                                   _Out_ til::rect* const prc);
    };
}

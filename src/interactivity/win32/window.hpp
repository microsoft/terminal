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

#include "..\types\IConsoleWindow.hpp"

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

        RECT GetWindowRect() const noexcept;
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

        void ChangeViewport(const SMALL_RECT NewWindow);

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

        void UpdateWindowSize(const COORD coordSizeInChars);
        void UpdateWindowPosition(_In_ POINT const ptNewPos) const;
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
        BOOL GetCursorPosition(_Out_ LPPOINT lpPoint);
        BOOL GetClientRectangle(_Out_ LPRECT lpRect);
        int MapPoints(_Inout_updates_(cPoints) LPPOINT lpPoints,
                      _In_ UINT cPoints);
        BOOL ConvertScreenToClient(_Inout_ LPPOINT lpPoint);

        [[nodiscard]] HRESULT UiaSetTextAreaFocus();

    protected:
        // prevent accidental generation of copies
        Window(Window const&) = delete;
        void operator=(Window const&) = delete;

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
        static Window* s_Instance;

        [[nodiscard]] NTSTATUS _InternalSetWindowSize();
        void _UpdateWindowSize(const SIZE sizeNew);

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
        RECT _rcClientLast;

        // Full screen
        void _BackupWindowSizes(const bool fCurrentIsInFullscreen);
        void _ApplyWindowSize();

        bool _fIsInFullscreen;
        RECT _rcFullscreenWindowSize;
        RECT _rcNonFullscreenWindowSize;

        // math helpers
        void _CalculateWindowRect(const COORD coordWindowInChars,
                                  _Inout_ RECT* const prectWindow) const;
        static void s_CalculateWindowRect(const COORD coordWindowInChars,
                                          const int iDpi,
                                          const COORD coordFontSize,
                                          const COORD coordBufferSize,
                                          _In_opt_ HWND const hWnd,
                                          _Inout_ RECT* const prectWindow);

        static void s_ReinitializeFontsForDPIChange();

        bool _fInDPIChange = false;

        static void s_ConvertWindowPosToWindowRect(const LPWINDOWPOS lpWindowPos,
                                                   _Out_ RECT* const prc);
    };
}

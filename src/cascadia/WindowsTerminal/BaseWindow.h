// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

template<typename T>
class BaseWindow
{
public:
    static constexpr UINT CM_UPDATE_TITLE = WM_USER + 0;

    static T* GetThisFromHandle(HWND const window) noexcept
    {
        return reinterpret_cast<T*>(GetWindowLongPtr(window, GWLP_USERDATA));
    }

    [[nodiscard]] static LRESULT __stdcall WndProc(HWND const window, UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept
    {
        WINRT_ASSERT(window);

        if (WM_NCCREATE == message)
        {
            auto cs = reinterpret_cast<CREATESTRUCT*>(lparam);
            T* that = static_cast<T*>(cs->lpCreateParams);
            WINRT_ASSERT(that);
            WINRT_ASSERT(!that->_window);
            that->_window = wil::unique_hwnd(window);

            return that->OnNcCreate(wparam, lparam);
        }
        else if (T* that = GetThisFromHandle(window))
        {
            return that->MessageHandler(message, wparam, lparam);
        }

        return DefWindowProc(window, message, wparam, lparam);
    }

    [[nodiscard]] LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept
    {
        switch (message)
        {
        case WM_DPICHANGED:
        {
            return HandleDpiChange(_window.get(), wparam, lparam);
        }
        case WM_SIZE:
        {
            UINT width = LOWORD(lparam);
            UINT height = HIWORD(lparam);

            switch (wparam)
            {
            case SIZE_MAXIMIZED:
                [[fallthrough]];
            case SIZE_RESTORED:
                if (_minimized)
                {
                    _minimized = false;
                    static_cast<T*>(this)->OnRestore();
                }

                // We always need to fire the resize event, even when we're transitioning from minimized.
                // We might be transitioning directly from minimized to maximized, and we'll need
                // to trigger any size-related content changes.
                static_cast<T*>(this)->OnResize(width, height);
                break;
            case SIZE_MINIMIZED:
                if (!_minimized)
                {
                    _minimized = true;
                    static_cast<T*>(this)->OnMinimize();
                }
                break;
            default:
                // do nothing.
                break;
            }
            break;
        }
        case CM_UPDATE_TITLE:
        {
            SetWindowTextW(_window.get(), _title.c_str());
            break;
        }
        }

        return DefWindowProc(_window.get(), message, wparam, lparam);
    }

    // DPI Change handler. on WM_DPICHANGE resize the window
    [[nodiscard]] LRESULT HandleDpiChange(const HWND hWnd, const WPARAM wParam, const LPARAM lParam)
    {
        _inDpiChange = true;
        const auto hWndStatic = GetWindow(hWnd, GW_CHILD);
        if (hWndStatic != nullptr)
        {
            const UINT uDpi = HIWORD(wParam);

            // Resize the window
            auto lprcNewScale = reinterpret_cast<RECT*>(lParam);

            SetWindowPos(hWnd, nullptr, lprcNewScale->left, lprcNewScale->top, lprcNewScale->right - lprcNewScale->left, lprcNewScale->bottom - lprcNewScale->top, SWP_NOZORDER | SWP_NOACTIVATE);

            _currentDpi = uDpi;
        }
        _inDpiChange = false;
        return 0;
    }

    RECT GetWindowRect() const noexcept
    {
        RECT rc = { 0 };
        ::GetWindowRect(_window.get(), &rc);
        return rc;
    }

    HWND GetHandle() const noexcept
    {
        return _window.get();
    }

    UINT GetCurrentDpi() const noexcept
    {
        return ::GetDpiForWindow(_window.get());
    }

    float GetCurrentDpiScale() const noexcept
    {
        const auto dpi = ::GetDpiForWindow(_window.get());
        const auto scale = static_cast<float>(dpi) / static_cast<float>(USER_DEFAULT_SCREEN_DPI);
        return scale;
    }

    //// Gets the physical size of the client area of the HWND in _window
    til::size GetPhysicalSize() const noexcept
    {
        RECT rect = {};
        GetClientRect(_window.get(), &rect);
        const auto windowsWidth = rect.right - rect.left;
        const auto windowsHeight = rect.bottom - rect.top;
        return { windowsWidth, windowsHeight };
    }

    //// Gets the logical (in DIPs) size of a physical size specified by the parameter physicalSize
    //// Remarks:
    //// XAML coordinate system is always in Display Independent Pixels (a.k.a DIPs or Logical). However Win32 GDI (because of legacy reasons)
    //// in DPI mode "Per-Monitor and Per-Monitor (V2) DPI Awareness" is always in physical pixels.
    //// The formula to transform is:
    ////     logical = (physical / dpi) + 0.5 // 0.5 is to ensure that we pixel snap correctly at the edges, this is necessary with odd DPIs like 1.25, 1.5, 1, .75
    //// See also:
    ////   https://docs.microsoft.com/en-us/windows/desktop/LearnWin32/dpi-and-device-independent-pixels
    ////   https://docs.microsoft.com/en-us/windows/desktop/hidpi/high-dpi-desktop-application-development-on-windows#per-monitor-and-per-monitor-v2-dpi-awareness
    winrt::Windows::Foundation::Size GetLogicalSize(const til::size physicalSize) const noexcept
    {
        const auto scale = GetCurrentDpiScale();
        // 0.5 is to ensure that we pixel snap correctly at the edges, this is necessary with odd DPIs like 1.25, 1.5, 1, .75
        const auto logicalWidth = (physicalSize.width / scale) + 0.5f;
        const auto logicalHeight = (physicalSize.height / scale) + 0.5f;
        return winrt::Windows::Foundation::Size(logicalWidth, logicalHeight);
    }

    winrt::Windows::Foundation::Size GetLogicalSize() const noexcept
    {
        return GetLogicalSize(GetPhysicalSize());
    }

    // Method Description:
    // - Sends a message to our message loop to update the title of the window.
    // Arguments:
    // - newTitle: a string to use as the new title of the window.
    // Return Value:
    // - <none>
    void UpdateTitle(std::wstring_view newTitle)
    {
        _title = newTitle;
        PostMessageW(_window.get(), CM_UPDATE_TITLE, 0, reinterpret_cast<LPARAM>(nullptr));
    }

    // Method Description:
    // Reset the current dpi of the window. This method is only called after we change the
    // initial launch position. This makes sure the dpi is consistent with the monitor on which
    // the window will launch
    void RefreshCurrentDPI()
    {
        _currentDpi = GetDpiForWindow(_window.get());
    }

protected:
    using base_type = BaseWindow<T>;
    wil::unique_hwnd _window;

    unsigned int _currentDpi = 0;
    bool _inDpiChange = false;

    std::wstring _title = L"";

    bool _minimized = false;

    void _setupUserData()
    {
        SetWindowLongPtr(_window.get(), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(static_cast<T*>(this)));
    }
    // Method Description:
    // - This method is called when the window receives the WM_NCCREATE message.
    // Return Value:
    // - The value returned from the window proc.
    [[nodiscard]] LRESULT OnNcCreate(WPARAM wParam, LPARAM lParam) noexcept
    {
        _setupUserData();

        EnableNonClientDpiScaling(_window.get());
        _currentDpi = GetDpiForWindow(_window.get());

        return DefWindowProc(_window.get(), WM_NCCREATE, wParam, lParam);
    };
};

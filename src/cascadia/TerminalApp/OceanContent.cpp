// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "OceanContent.h"
#include "PaneArgs.h"
#include "OceanContent.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    OceanContent::OceanContent()
    {
        _root = winrt::Windows::UI::Xaml::Controls::Grid{};
        _root.VerticalAlignment(VerticalAlignment::Stretch);
        _root.HorizontalAlignment(HorizontalAlignment::Stretch);

        auto res = Windows::UI::Xaml::Application::Current().Resources();
        auto bg = res.Lookup(winrt::box_value(L"UnfocusedBorderBrush"));
        _root.Background(bg.try_as<Media::Brush>());

        // _box = winrt::Windows::UI::Xaml::Controls::TextBox{};
        // _box.Margin({ 10, 10, 10, 10 });
        // _box.AcceptsReturn(true);
        // _box.TextWrapping(TextWrapping::Wrap);
        // _root.Children().Append(_box);

        _createOcean();

        // Add a size change handler to the root grid.
        _sizeChangedRevoker = _root.SizeChanged(winrt::auto_revoke, [this](auto&&, auto&&) {
            const auto widthInDips = _root.ActualWidth();
            const auto heightInDips = _root.ActualHeight();
            // wprintf(L"resized to: %f, %f\n", width, height);

            // adjust for DPI
            const auto dpi = Windows::Graphics::Display::DisplayInformation::GetForCurrentView().LogicalDpi();
            const auto dpiScale = dpi / 96.0f;
            const auto widthInPixels = widthInDips * dpiScale;
            const auto heightInPixels = heightInDips * dpiScale;

            // Get the actual location of our _root, relative to the screen
            const auto transform = _root.TransformToVisual(nullptr);
            const auto point = transform.TransformPoint({ 0, 0 });

            // scale from DIPs to pixels:

            // Resize our ocean HWND
            SetWindowPos(_window.get(),
                         nullptr,
                         gsl::narrow_cast<int>(point.X * dpiScale) + 48,
                         gsl::narrow_cast<int>(point.Y * dpiScale) + 48,
                         gsl::narrow_cast<int>(widthInPixels) - 96,
                         gsl::narrow_cast<int>(heightInPixels) - 96,
                         /* SWP_NOZORDER | SWP_NOMOVE */ SWP_NOACTIVATE);
        });
    }

    static OceanContent* GetThisFromHandle(HWND const window) noexcept
    {
        return reinterpret_cast<OceanContent*>(GetWindowLongPtr(window, GWLP_USERDATA));
    }
    LRESULT __stdcall OceanContent::s_WndProc(HWND const window, UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept
    {
        WINRT_ASSERT(window);

        if (WM_NCCREATE == message)
        {
            auto cs = reinterpret_cast<CREATESTRUCT*>(lparam);
            OceanContent* that = static_cast<OceanContent*>(cs->lpCreateParams);
            that->_window = wil::unique_hwnd(window);

            // return that->OnNcCreate(wparam, lparam);

            SetWindowLongPtr(that->_window.get(), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(that));

            return DefWindowProc(that->_window.get(), WM_NCCREATE, wparam, lparam);
        }
        else if (OceanContent* that = GetThisFromHandle(window))
        {
            return that->_messageHandler(message, wparam, lparam);
        }

        return DefWindowProc(window, message, wparam, lparam);
    }

    LRESULT OceanContent::_messageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam)
    {
        switch (message)
        {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(_window.get(), &ps);

            // All painting occurs here, between BeginPaint and EndPaint.
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_HIGHLIGHT + 1));

            EndPaint(_window.get(), &ps);
            break;
        }
        case WM_CLOSE:
        case WM_DESTROY:
        {
            // PostQuitMessage(0);
            CloseRequested.raise(*this, nullptr);
            break;
        }
        }
        return DefWindowProc(_window.get(), message, wparam, lparam);
    }

    void OceanContent::_createOcean()
    {
        static const auto oceanClassAtom = []() {
            static const auto SCRATCH_WINDOW_CLASS = L"ocean_window_class";
            WNDCLASSEXW scratchClass{ 0 };
            scratchClass.cbSize = sizeof(WNDCLASSEXW);
            scratchClass.style = CS_HREDRAW | CS_VREDRAW | /*CS_PARENTDC |*/ CS_DBLCLKS;
            scratchClass.lpszClassName = SCRATCH_WINDOW_CLASS;
            scratchClass.lpfnWndProc = s_WndProc;
            scratchClass.cbWndExtra = 0; // GWL_CONSOLE_WNDALLOC; // this is required to store the owning thread/process override in NTUSER
            auto windowClassAtom{ RegisterClassExW(&scratchClass) };
            return windowClassAtom;
        }();
        const auto style = 0; // no resize border, no caption, etc.
        const auto exStyle = 0;

        CreateWindowExW(exStyle, // WS_EX_LAYERED,
                        reinterpret_cast<LPCWSTR>(oceanClassAtom),
                        L"Hello World",
                        style,
                        200, // CW_USEDEFAULT,
                        200, // CW_USEDEFAULT,
                        200, // CW_USEDEFAULT,
                        200, // CW_USEDEFAULT,
                        0, // owner
                        nullptr,
                        nullptr,
                        this);

        SetWindowLong(_window.get(), GWL_STYLE, 0); //remove all window styles, cause it's created with WS_CAPTION even if we didn't ask for it
        ShowWindow(_window.get(), SW_SHOW); //display window
    }

    void OceanContent::SetHostingWindow(uint64_t hostingWindow) noexcept
    {
        const auto casted = reinterpret_cast<HWND>(hostingWindow);
        // Set our HWND as owned by the parent's. We're always on top of them, but not explicitly a _child_.
        ::SetWindowLongPtrW(_window.get(), GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(casted));
    }

    void OceanContent::UpdateSettings(const CascadiaSettings& /*settings*/)
    {
        // Nothing to do.
    }

    winrt::Windows::UI::Xaml::FrameworkElement OceanContent::GetRoot()
    {
        return _root;
    }
    winrt::Windows::Foundation::Size OceanContent::MinSize()
    {
        return { 1, 1 };
    }
    void OceanContent::Focus(winrt::Windows::UI::Xaml::FocusState /*reason*/)
    {
        // _box.Focus(reason);
    }
    void OceanContent::Close()
    {
        CloseRequested.raise(*this, nullptr);
    }

    NewTerminalArgs OceanContent::GetNewTerminalArgs(const bool /* asContent */) const
    {
        return nullptr;
    }

    winrt::hstring OceanContent::Icon() const
    {
        static constexpr std::wstring_view glyph{ L"\xe70b" }; // QuickNote
        return winrt::hstring{ glyph };
    }

    winrt::Windows::UI::Xaml::Media::Brush OceanContent::BackgroundBrush()
    {
        return _root.Background();
    }
}

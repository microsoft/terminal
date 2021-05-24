// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SampleBaseWindow.h"
#include "../../../src/cascadia/inc/cppwinrt_utils.h"

class SampleIslandWindow :
    public BaseWindow<SampleIslandWindow>
{
public:
    SampleIslandWindow() noexcept;
    virtual ~SampleIslandWindow() override;

    virtual void MakeWindow() noexcept;
    void Close();

    virtual void OnSize(const UINT width, const UINT height);

    [[nodiscard]] virtual LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept override;
    void OnResize(const UINT width, const UINT height) override;
    void OnMinimize() override;
    void OnRestore() override;
    virtual void OnAppInitialized();
    virtual void SetContent(winrt::Windows::UI::Xaml::UIElement content);
    virtual void OnApplicationThemeChanged(const winrt::Windows::UI::Xaml::ElementTheme& requestedTheme);

    virtual void Initialize();

protected:
    void ForceResize()
    {
        // Do a quick resize to force the island to paint
        const auto size = GetPhysicalSize();
        OnSize(size.cx, size.cy);
    }

    HWND _interopWindowHandle;

    winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource _source;

    winrt::Windows::UI::Xaml::Controls::Grid _rootGrid;

    void _HandleCreateWindow(const WPARAM wParam, const LPARAM lParam) noexcept;
};

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BaseWindow.h"
#include <winrt/TerminalApp.h>
#include "../../cascadia/inc/cppwinrt_utils.h"

class IslandWindow :
    public BaseWindow<IslandWindow>
{
public:
    IslandWindow() noexcept;
    virtual ~IslandWindow() override;

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
    virtual SIZE GetTotalNonClientExclusiveSize(const UINT dpi) const noexcept;

    virtual void Initialize();

    void SetCreateCallback(std::function<void(const HWND, const RECT, winrt::Microsoft::Terminal::Settings::Model::LaunchMode& launchMode)> pfn) noexcept;
    void SetSnapDimensionCallback(std::function<float(bool widthOrHeight, float dimension)> pfn) noexcept;

    void FocusModeChanged(const bool focusMode);
    void FullscreenChanged(const bool fullscreen);
    void SetAlwaysOnTop(const bool alwaysOnTop);

#pragma endregion

    DECLARE_EVENT(DragRegionClicked, _DragRegionClickedHandlers, winrt::delegate<>);
    DECLARE_EVENT(WindowCloseButtonClicked, _windowCloseButtonClickedHandler, winrt::delegate<>);
    WINRT_CALLBACK(MouseScrolled, winrt::delegate<void(til::point, int32_t)>);

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

    std::function<void(const HWND, const RECT, winrt::Microsoft::Terminal::Settings::Model::LaunchMode& launchMode)> _pfnCreateCallback;
    std::function<float(bool, float)> _pfnSnapDimensionCallback;

    void _HandleCreateWindow(const WPARAM wParam, const LPARAM lParam) noexcept;
    [[nodiscard]] LRESULT _OnSizing(const WPARAM wParam, const LPARAM lParam);

    bool _borderless{ false };
    bool _fullscreen{ false };
    bool _alwaysOnTop{ false };
    RECT _fullscreenWindowSize;
    RECT _nonFullscreenWindowSize;

    virtual void _SetIsBorderless(const bool borderlessEnabled);
    virtual void _SetIsFullscreen(const bool fullscreenEnabled);
    void _BackupWindowSizes(const bool currentIsInFullscreen);
    void _ApplyWindowSize();

    LONG _getDesiredWindowStyle() const;

private:
    // This minimum width allows for width the tabs fit
    static constexpr long minimumWidth = 460L;
};

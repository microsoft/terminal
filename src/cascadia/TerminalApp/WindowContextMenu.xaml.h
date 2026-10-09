#pragma once

#include "WindowContextMenu.g.h"

namespace winrt::TerminalApp::implementation
{
    struct WindowContextMenu : WindowContextMenuT<WindowContextMenu>
    {
        WindowContextMenu() = default;
        WindowContextMenu(uint64_t handle);

        ~WindowContextMenu();

        // XAML Click event handlers for standard system menu commands
        void RestoreMenuItemOnClick(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/);
        void MoveMenuItemOnClick(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/);
        void SizeMenuItemOnClick(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/);
        void MinimizeMenuItemOnClick(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/);
        void MaximizeMenuItemOnClick(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/);
        void CloseMenuItemOnClick(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/);

        WINRT_PROPERTY(winrt::Windows::UI::Xaml::UIElement, TargetElement, nullptr);

    private:
        void _IsResizable(const bool value);
        void _IsMaximized(const bool value);

        void _AddMenuItems(gsl::not_null<HMENU> menu, int32_t itemsCount);

        void _CleanMenuItemText(std::wstring& text);

        std::wstring _GetMenuItemText(gsl::not_null<HMENU> menu, uint32_t itemIndex);

        [[nodiscard]] static LRESULT __stdcall _StaticContextMenuSubClassProc(HWND const window, UINT const message, WPARAM const wparam, LPARAM const lparam, UINT_PTR const uSubClass, DWORD_PTR const dwRefData) noexcept;

        [[nodiscard]] LRESULT _ContextMenuMessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept;

        void _RemoveContextMenuSubClassProc();

        void _ShowMenu(LPARAM const lparam);

        void _AttachToParentWindow(gsl::not_null<HWND> handle);

        HWND _parentWindow{ nullptr };
        bool _subClassIsPresent{ false };
        bool _isMaximized{ false };

        static constexpr int restoreMenuItemIndex = 0;
        static constexpr int moveMenuItemIndex = 1;
        static constexpr int sizeMenuItemIndex = 2;
        static constexpr int minimizeMenuItemIndex = 3;
        static constexpr int maximizeMenuItemIndex = 4;
        static constexpr int closeMenuItemIndex = 6;

        static constexpr int contextMenuItemCount = 7;
        static constexpr int nonResizableContextMenuItemCount = 2;

        static constexpr UINT_PTR contextMenuSubClassId = 0x66006;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(WindowContextMenu);
}

#pragma once

namespace TestHook
{
    struct LayoutGuard
    {
        LayoutGuard() = default;
        LayoutGuard(HKL layout, bool needsUnload) noexcept;
        ~LayoutGuard();

        LayoutGuard(const LayoutGuard&) = delete;
        LayoutGuard& operator=(const LayoutGuard&) = delete;
        LayoutGuard(LayoutGuard&& other) noexcept;
        LayoutGuard& operator=(LayoutGuard&& other) noexcept;

        explicit operator bool() const noexcept;
        operator HKL() const noexcept;

    private:
        void _destroy() const noexcept;

        HKL _layout = nullptr;
        bool _needsUnload = false;
    };

    LayoutGuard SetTerminalInputKeyboardLayout(const wchar_t* klid);
}

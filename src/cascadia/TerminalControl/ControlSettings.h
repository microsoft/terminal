/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/
#pragma once
#include "../../inc/cppwinrt_utils.h"
#include "../../inc/ControlProperties.h"

#include <DefaultSettings.h>
#include <conattrs.hpp>
#include "ControlAppearance.h"

using IFontFeatureMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, uint32_t>;
using IFontAxesMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct ControlSettings : public winrt::implements<ControlSettings, Microsoft::Terminal::Control::IControlSettings, Microsoft::Terminal::Control::IControlAppearance, Microsoft::Terminal::Core::ICoreSettings, Microsoft::Terminal::Core::ICoreAppearance>
    {
        // Getters and setters for each *Setting member. We're not using
        // WINRT_PROPERTY for these, because they actually exist inside the
        // _focusedAppearance member. We don't need to reserve another member to
        // hold them.
#define SETTINGS_GEN(type, name, ...) WINRT_PROPERTY(type, name, __VA_ARGS__);
        CORE_SETTINGS(SETTINGS_GEN)
        CONTROL_SETTINGS(SETTINGS_GEN)
#undef SETTINGS_GEN

    private:
        winrt::com_ptr<ControlAppearance> _unfocusedAppearance{ nullptr };
        winrt::com_ptr<ControlAppearance> _focusedAppearance{ nullptr };
        bool _hasUnfocusedAppearance{ false };

    public:
        ControlSettings(const Control::IControlSettings& settings,
                        const Control::IControlAppearance& unfocusedAppearance)
        {
            _hasUnfocusedAppearance = unfocusedAppearance != nullptr;

            _focusedAppearance = winrt::make_self<implementation::ControlAppearance>(settings);
            _unfocusedAppearance = unfocusedAppearance ?
                                       winrt::make_self<implementation::ControlAppearance>(unfocusedAppearance) :
                                       _focusedAppearance;

            // Copy every value from the passed in settings, into us.
#define COPY_SETTING(type, name, ...) _##name = settings.name();
            CORE_SETTINGS(COPY_SETTING)
            CONTROL_SETTINGS(COPY_SETTING)
#undef COPY_SETTING
        }

        winrt::com_ptr<ControlAppearance> UnfocusedAppearance() { return _unfocusedAppearance; }
        winrt::com_ptr<ControlAppearance> FocusedAppearance() { return _focusedAppearance; }
        bool HasUnfocusedAppearance() { return _hasUnfocusedAppearance; }

        // Getters and setters for each Appearance member. We're not using
        // WINRT_PROPERTY for these, because they actually exist inside the
        // _focusedAppearance member. We don't need to reserve another member to
        // hold them.
        //
        // The Appearance members (including GetColorTableEntry below) are used
        // when this ControlSettings is cast to a IControlAppearance or
        // ICoreAppearance. In those cases, we'll always return the Focused
        // appearance's version of the member. Callers who care about which
        // appearance is being used should be more careful. Fortunately, this
        // situation is generally only used when a control is first created, or
        // when calling UpdateSettings.
#define APPEARANCE_GEN(type, name, ...)    \
    type name() const noexcept             \
    {                                      \
        return _focusedAppearance->name(); \
    }                                      \
    void name(const type& value) noexcept  \
    {                                      \
        _focusedAppearance->name(value);   \
    }

        CORE_APPEARANCE_SETTINGS(APPEARANCE_GEN)
        til::color SelectionBackground() const noexcept
        {
            return _focusedAppearance->SelectionBackground();
        }
        void SelectionBackground(const til::color& value) noexcept
        {
            _focusedAppearance->SelectionBackground(value);
        }
        double Opacity() const noexcept
        {
            return _focusedAppearance->Opacity();
        }
        void Opacity(const double& value) noexcept
        {
            _focusedAppearance->Opacity(value);
        }
        winrt::hstring BackgroundImage() const noexcept
        {
            return _focusedAppearance->BackgroundImage();
        }
        void BackgroundImage(const winrt::hstring& value) noexcept
        {
            _focusedAppearance->BackgroundImage(value);
        }
        double BackgroundImageOpacity() const noexcept
        {
            return _focusedAppearance->BackgroundImageOpacity();
        }
        void BackgroundImageOpacity(const double& value) noexcept
        {
            _focusedAppearance->BackgroundImageOpacity(value);
        }
        winrt::Windows::UI::Xaml::Media::Stretch BackgroundImageStretchMode() const noexcept
        {
            return _focusedAppearance->BackgroundImageStretchMode();
        }
        void BackgroundImageStretchMode(const winrt::Windows::UI::Xaml::Media::Stretch& value) noexcept
        {
            _focusedAppearance->BackgroundImageStretchMode(value);
        }
        winrt::Windows::UI::Xaml::HorizontalAlignment BackgroundImageHorizontalAlignment() const noexcept
        {
            return _focusedAppearance->BackgroundImageHorizontalAlignment();
        }
        void BackgroundImageHorizontalAlignment(const winrt::Windows::UI::Xaml::HorizontalAlignment& value) noexcept
        {
            _focusedAppearance->BackgroundImageHorizontalAlignment(value);
        }
        winrt::Windows::UI::Xaml::VerticalAlignment BackgroundImageVerticalAlignment() const noexcept
        {
            return _focusedAppearance->BackgroundImageVerticalAlignment();
        }
        void BackgroundImageVerticalAlignment(const winrt::Windows::UI::Xaml::VerticalAlignment& value) noexcept
        {
            _focusedAppearance->BackgroundImageVerticalAlignment(value);
        }
        bool RetroTerminalEffect() const noexcept
        {
            return _focusedAppearance->RetroTerminalEffect();
        }
        void RetroTerminalEffect(const bool& value) noexcept
        {
            _focusedAppearance->RetroTerminalEffect(value);
        }
        winrt::hstring PixelShaderPath() const noexcept
        {
            return _focusedAppearance->PixelShaderPath();
        }
        void PixelShaderPath(const winrt::hstring& value) noexcept
        {
            _focusedAppearance->PixelShaderPath(value);
        }
        bool UseAcrylic2() const noexcept
        {
            return _focusedAppearance->UseAcrylic2();
        }
        void UseAcrylic2(const bool& value) noexcept
        {
            _focusedAppearance->UseAcrylic2(value);
        }
#undef APPEARANCE_GEN

        winrt::Microsoft::Terminal::Core::Color GetColorTableEntry(int32_t index) noexcept
        {
            return _focusedAppearance->GetColorTableEntry(index);
        }
    };
}

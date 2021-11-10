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
    struct ControlSettings : public winrt::implements<ControlSettings, Microsoft::Terminal::Control::IControlSettings, Microsoft::Terminal::Core::ICoreSettings>
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

        // NOTABLY: ControlSettings is not an Appearance. Make sure to get the
        // Appearance you actually want out of it.
    };
}

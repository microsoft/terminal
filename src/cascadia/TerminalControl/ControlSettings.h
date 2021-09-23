/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/
#pragma once
#include "../../inc/cppwinrt_utils.h"

#include <DefaultSettings.h>
#include <conattrs.hpp>
#include "ControlAppearance.h"

using IFontFeatureMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, uint32_t>;
using IFontAxesMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    // --------------------------- Core Settings ---------------------------
    //  All of these settings are defined in ICoreSettings.
#define CORE_SETTINGS(X)                                                                                          \
    X(int32_t, HistorySize, DEFAULT_HISTORY_SIZE)                                                                 \
    X(int32_t, InitialRows, 30)                                                                                   \
    X(int32_t, InitialCols, 80)                                                                                   \
    X(bool, SnapOnInput, true)                                                                                    \
    X(bool, AltGrAliasing, true)                                                                                  \
    X(winrt::hstring, WordDelimiters, DEFAULT_WORD_DELIMITERS)                                                    \
    X(bool, CopyOnSelect, false)                                                                                  \
    X(bool, InputServiceWarning, true)                                                                            \
    X(bool, FocusFollowMouse, false)                                                                              \
    X(winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Core::Color>, TabColor, nullptr)         \
    X(winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Core::Color>, StartingTabColor, nullptr) \
    X(bool, TrimBlockSelection, false)                                                                            \
    X(bool, DetectURLs, true)

    // --------------------------- Control Settings ---------------------------
    //  All of these settings are defined in IControlSettings.
#define CONTROL_SETTINGS(X)                                                                                                                              \
    X(winrt::hstring, ProfileName)                                                                                                                       \
    X(bool, UseAcrylic, false)                                                                                                                           \
    X(winrt::hstring, Padding, DEFAULT_PADDING)                                                                                                          \
    X(winrt::hstring, FontFace, L"Consolas")                                                                                                             \
    X(int32_t, FontSize, DEFAULT_FONT_SIZE)                                                                                                              \
    X(winrt::Windows::UI::Text::FontWeight, FontWeight)                                                                                                  \
    X(winrt::Microsoft::Terminal::Control::IKeyBindings, KeyBindings, nullptr)                                                                           \
    X(winrt::hstring, Commandline)                                                                                                                       \
    X(winrt::hstring, StartingDirectory)                                                                                                                 \
    X(winrt::hstring, StartingTitle)                                                                                                                     \
    X(bool, SuppressApplicationTitle)                                                                                                                    \
    X(winrt::hstring, EnvironmentVariables)                                                                                                              \
    X(winrt::Microsoft::Terminal::Control::ScrollbarState, ScrollState, winrt::Microsoft::Terminal::Control::ScrollbarState::Visible)                    \
    X(winrt::Microsoft::Terminal::Control::TextAntialiasingMode, AntialiasingMode, winrt::Microsoft::Terminal::Control::TextAntialiasingMode::Grayscale) \
    X(bool, ForceFullRepaintRendering, false)                                                                                                            \
    X(bool, SoftwareRendering, false)                                                                                                                    \
    X(bool, ForceVTInput, false)

    struct ControlSettings //  : public winrt::implements<ControlSettings /*, Microsoft::Terminal::Core::ICoreSettings, Microsoft::Terminal::Control::IControlSettings*/>
    {
#define CORE_SETTINGS_GEN(type, name, ...) WINRT_PROPERTY(type, name, __VA_ARGS__);
        CORE_SETTINGS(CORE_SETTINGS_GEN)
#undef CORE_SETTINGS_GEN

#define CONTROL_SETTINGS_GEN(type, name, ...) WINRT_PROPERTY(type, name, __VA_ARGS__);
        CONTROL_SETTINGS(CONTROL_SETTINGS_GEN)
#undef CONTROL_SETTINGS_GEN

    public:
        winrt::com_ptr<ControlAppearance> UnfocusedAppearance()
        {
            return _unfocusedAppearance;
        };
        winrt::com_ptr<ControlAppearance> FocusedAppearance()
        {
            return _focusedAppearance;
        };

    private:
        winrt::com_ptr<ControlAppearance> _unfocusedAppearance{ nullptr };
        winrt::com_ptr<ControlAppearance> _focusedAppearance{ nullptr };

        // ControlSettings()
        // {
        //     const auto campbellSpan = ::Microsoft::Console::Utils::CampbellColorTable();
        //     std::transform(campbellSpan.begin(), campbellSpan.end(), _ColorTable.begin(), [](auto&& color) {
        //         return static_cast<winrt::Microsoft::Terminal::Core::Color>(til::color{ color });
        //     });
        // }
    };
}

// namespace winrt::Microsoft::Terminal::Control::factory_implementation
// {
//     BASIC_FACTORY(ControlSettings);
// }

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <til/static_map.h>

// Navigation tags used to identify pages in the Settings UI NavigationView.
// These tags are stored as the Tag property on NavigationViewItems.
namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    inline constexpr std::wstring_view openJsonTag{ L"OpenJson_Nav" };
    inline constexpr std::wstring_view launchTag{ L"Launch_Nav" };
    inline constexpr std::wstring_view interactionTag{ L"Interaction_Nav" };
    inline constexpr std::wstring_view renderingTag{ L"Rendering_Nav" };
    inline constexpr std::wstring_view compatibilityTag{ L"Compatibility_Nav" };
    inline constexpr std::wstring_view actionsTag{ L"Actions_Nav" };
    inline constexpr std::wstring_view newTabMenuTag{ L"NewTabMenu_Nav" };
    inline constexpr std::wstring_view extensionsTag{ L"Extensions_Nav" };
    inline constexpr std::wstring_view globalProfileTag{ L"GlobalProfile_Nav" };
    inline constexpr std::wstring_view addProfileTag{ L"AddProfile" };
    inline constexpr std::wstring_view colorSchemesTag{ L"ColorSchemes_Nav" };
    inline constexpr std::wstring_view globalAppearanceTag{ L"GlobalAppearance_Nav" };

    // Map from navigation tags to Segoe MDL2 Assets icon glyphs
    inline constexpr til::static_map NavTagIconMap{
        std::pair{ launchTag, L"\xE7B5" }, /* Set Lock Screen */
        std::pair{ interactionTag, L"\xE7C9" }, /* Touch Pointer */
        std::pair{ globalAppearanceTag, L"\xE771" }, /* Personalize */
        std::pair{ colorSchemesTag, L"\xE790" }, /* Color */
        std::pair{ renderingTag, L"\xE7F8" }, /* Device Laptop No Pic */
        std::pair{ compatibilityTag, L"\xEC7A" }, /* Developer Tools */
        std::pair{ actionsTag, L"\xE765" }, /* Keyboard Classic */
        std::pair{ newTabMenuTag, L"\xE71D" }, /* All Apps */
        std::pair{ extensionsTag, L"\xEA86" }, /* Puzzle */
        std::pair{ globalProfileTag, L"\xE81E" }, /* Map Layers */
        std::pair{ addProfileTag, L"\xE710" }, /* Add */
        std::pair{ openJsonTag, L"\xE713" }, /* Settings */
    };
}

/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- WindowSettings.h

Abstract:
- This class represents per-window settings. In the current WIP implementation,
  it delegates all property access to a GlobalAppSettings object, since we
  haven't yet split the actual storage into per-window instances.

Author(s):
- Mike Griese - April 2026

--*/
#pragma once

#include "WindowSettings.g.h"
#include "MTSMSettings.h"
#include "GlobalAppSettings.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct WindowSettings : WindowSettingsT<WindowSettings>
    {
    public:
        // Default constructor required by WinRT activation
        WindowSettings() = default;

        // Construct a WindowSettings that delegates to the given GlobalAppSettings.
        void Initialize(const com_ptr<GlobalAppSettings>& globals);

        hstring Name() const;

        winrt::guid DefaultProfile() const;
        void DefaultProfile(const winrt::guid& value);

        hstring UnparsedDefaultProfile() const;
        void UnparsedDefaultProfile(const hstring& value);
        bool HasUnparsedDefaultProfile() const;
        void ClearUnparsedDefaultProfile();

        // Delegate all MTSM_WINDOW_SETTINGS to GlobalAppSettings via inline methods.
#define WINDOW_SETTINGS_DELEGATE(type, name, ...)                             \
    type name() const { return _globals->name(); }                            \
    void name(const type& value) { _globals->name(value); }                  \
    bool Has##name() const { return _globals->Has##name(); }                  \
    void Clear##name() { _globals->Clear##name(); }
        MTSM_WINDOW_SETTINGS(WINDOW_SETTINGS_DELEGATE)
#undef WINDOW_SETTINGS_DELEGATE

    private:
        com_ptr<GlobalAppSettings> _globals{ nullptr };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(WindowSettings);
}

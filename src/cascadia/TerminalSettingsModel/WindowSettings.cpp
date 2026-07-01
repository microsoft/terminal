// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "WindowSettings.h"
#include "WindowSettings.g.cpp"

#include "GlobalAppSettings.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    void WindowSettings::Initialize(const com_ptr<GlobalAppSettings>& globals)
    {
        _globals = globals;
    }

    hstring WindowSettings::Name() const
    {
        // In the WIP implementation, there is only one set of window settings
        // (backed by GlobalAppSettings), so the name is always empty.
        return L"";
    }

    winrt::guid WindowSettings::DefaultProfile() const
    {
        return _globals->DefaultProfile();
    }

    void WindowSettings::DefaultProfile(const winrt::guid& value)
    {
        _globals->DefaultProfile(value);
    }

    hstring WindowSettings::UnparsedDefaultProfile() const
    {
        return _globals->UnparsedDefaultProfile();
    }

    void WindowSettings::UnparsedDefaultProfile(const hstring& value)
    {
        _globals->UnparsedDefaultProfile(value);
    }

    bool WindowSettings::HasUnparsedDefaultProfile() const
    {
        return _globals->HasUnparsedDefaultProfile();
    }

    void WindowSettings::ClearUnparsedDefaultProfile()
    {
        _globals->ClearUnparsedDefaultProfile();
    }

    // The MTSM_WINDOW_SETTINGS delegate methods are defined inline
    // in WindowSettings.h via the WINDOW_SETTINGS_DELEGATE macro.

}

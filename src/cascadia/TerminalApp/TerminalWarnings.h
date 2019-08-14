/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TerminalWarnings.h

Abstract:
- This file contains definitions for warnings, errors and exceptions used by the
  Windows Terminal

Author(s):
- Mike Griese - August 2019

--*/
#pragma once

namespace TerminalApp
{
    // SettingsLoadWarnings are scenarios where the settings contained
    // information we knew was invalid, but we could recover from.
    enum class SettingsLoadWarnings : uint32_t
    {
        MissingDefaultProfile = 0,
        DuplicateProfile = 1
    };

    // SettingsLoadWarnings are scenarios where the settings had invalid state
    // that we could not recover from.
    enum class SettingsLoadErrors : uint32_t
    {
        NoProfiles = 0
    };

    // This is a helper class to wrap up a SettingsLoadErrors into a proper
    // exception type.
    class TerminalException : public std::runtime_error
    {
    public:
        TerminalException(const SettingsLoadErrors& error) :
            std::runtime_error{ nullptr },
            _error{ error } {};

        // We don't use the what() method - we want to be able to display
        // localizable error messages. Catchers of this exception should use
        // _GetErrorText to get the localized exception string.
        const char* what() const override { return nullptr; };

        SettingsLoadErrors Error() const noexcept { return _error; };

    private:
        const SettingsLoadErrors _error;
    };
};

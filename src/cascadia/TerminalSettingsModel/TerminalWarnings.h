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

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    // This is a helper class to wrap up a SettingsLoadErrors into a proper
    // exception type.
    class SettingsException : public std::runtime_error
    {
    public:
        explicit SettingsException(const SettingsLoadErrors& error) :
            std::runtime_error{ nullptr },
            _error{ error } {};

        // We don't use the what() method - we want to be able to display
        // localizable error messages. Catchers of this exception should use
        // _GetErrorText (in App.cpp) to get the localized exception string.
        const char* what() const override
        {
            return "Exception while loading or validating Terminal settings";
        };

        SettingsLoadErrors Error() const noexcept { return _error; };

    private:
        const SettingsLoadErrors _error;
    };
};

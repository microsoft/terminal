// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

namespace Microsoft::Console::Utils
{
    //
    // A case-insensitive wide-character map is used to store environment variables
    // due to documented requirements:
    //
    //      "All strings in the environment block must be sorted alphabetically by name.
    //      The sort is case-insensitive, Unicode order, without regard to locale.
    //      Because the equal sign is a separator, it must not be used in the name of
    //      an environment variable."
    //      https://docs.microsoft.com/en-us/windows/desktop/ProcThread/changing-environment-variables
    //
    struct WStringCaseInsensitiveCompare
    {
        [[nodiscard]] bool operator()(const std::wstring& lhs, const std::wstring& rhs) const noexcept
        {
            return (::_wcsicmp(lhs.c_str(), rhs.c_str()) < 0);
        }
    };

    using EnvironmentVariableMapW = std::map<std::wstring, std::wstring, WStringCaseInsensitiveCompare>;

    [[nodiscard]] HRESULT UpdateEnvironmentMapW(EnvironmentVariableMapW& map) noexcept;

    [[nodiscard]] HRESULT EnvironmentMapToEnvironmentStringsW(EnvironmentVariableMapW& map,
                                                              std::vector<wchar_t>& newEnvVars) noexcept;

};

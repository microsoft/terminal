/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- alias.h

Abstract:
- Encapsulates the cmdline functions and structures specifically related to
        command alias functionality.
--*/
#pragma once

class Alias
{
public:
    static void s_ClearCmdExeAliases();

    static std::wstring s_MatchAndCopyAlias(std::wstring_view sourceText, const std::wstring& exeName, size_t& lineCount);

private:
#ifdef UNIT_TESTING
    static void s_TestAddAlias(std::wstring& exe,
                               std::wstring& alias,
                               std::wstring& target);

    static void s_TestClearAliases();

    friend class AliasTests;
#endif
};

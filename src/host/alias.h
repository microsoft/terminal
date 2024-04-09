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
    static std::deque<std::wstring> s_Tokenize(const std::wstring_view str);
    static std::wstring s_GetArgString(const std::wstring_view str);
    static size_t s_ReplaceMacros(std::wstring& str,
                                  const std::deque<std::wstring>& tokens,
                                  const std::wstring& fullArgString);

    static bool s_TryReplaceNumberedArgMacro(const wchar_t ch,
                                             std::wstring& appendToStr,
                                             const std::deque<std::wstring>& tokens);
    static bool s_TryReplaceWildcardArgMacro(const wchar_t ch,
                                             std::wstring& appendToStr,
                                             const std::wstring fullArgString);

    static bool s_TryReplaceInputRedirMacro(const wchar_t ch,
                                            std::wstring& appendToStr);
    static bool s_TryReplaceOutputRedirMacro(const wchar_t ch,
                                             std::wstring& appendToStr);
    static bool s_TryReplacePipeRedirMacro(const wchar_t ch,
                                           std::wstring& appendToStr);

    static bool s_TryReplaceNextCommandMacro(const wchar_t ch,
                                             std::wstring& appendToStr,
                                             size_t& lineCount);

    static void s_AppendCrLf(std::wstring& appendToStr,
                             size_t& lineCount);

#ifdef UNIT_TESTING
    static void s_TestAddAlias(std::wstring& exe,
                               std::wstring& alias,
                               std::wstring& target);

    static void s_TestClearAliases();

    friend class AliasTests;
#endif
};

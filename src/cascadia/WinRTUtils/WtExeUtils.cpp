// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "WtExeUtils.h"

// Method Description:
// - Escapes the given string so that it can be used as a command-line arg within a quoted string.
// - e.g. given `";foo\` will return `\"\;foo\\` so that the caller can construct a command-line
//   using something such as `fmt::format(L"wt --title \"{}\"", EscapeCommandlineArg(TabTitle()))`.
// Arguments:
// - arg - the command-line argument to escape.
// Return Value:
// - the escaped command-line argument.
std::wstring EscapeCommandlineArg(const std::wstring_view arg)
{
    std::wstringstream stream;
    for (auto it = arg.cbegin(); it != arg.cend(); ++it)
    {
        if (*it == L';' || *it == L'"')
        {
            stream << L'\\';
        }
        stream << *it;
    }
    if (arg.back() == L'\\')
    {
        stream << L'\\';
    }
    return stream.str();
}

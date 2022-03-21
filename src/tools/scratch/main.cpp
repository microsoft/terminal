// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "../../inc/LibraryIncludes.h"

// #include <windows.h>
// #include <stdio.h>
// #include <iostream>

#define GS L"\x1D"
#define US L"\x1F"
#define DEL L"\x7F"
#define ST L"\x07"

std::wstring trim(const std::wstring& str,
                  const std::wstring& whitespace = L" \t")
{
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::wstring::npos)
        return L""; // no content

    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

// This wmain exists for help in writing scratch programs while debugging.
int __cdecl wmain(int argc, WCHAR* argv[])
{
    wprintf(L"\x1b]9001;0\x07");

    std::wstring prefix{};
    std::wstring suffix{};

    for (int i = 0; i < argc; i++)
    {
        std::wstring arg{ argv[i] };
        if (arg == L"--prefix" && (i + 1) <= argc)
        {
            prefix = argv[i + 1];
            i++;
        }
        else if (arg == L"--suffix" && (i + 1) <= argc)
        {
            suffix = argv[i + 1];
            i++;
        }
    }

    for (std::wstring line; std::getline(std::wcin, line);)
    {
        std::wstring trimmed{ trim(line) };
        std::wstring command{ fmt::format(L"{}{}{}", prefix, trimmed, suffix) };
        std::wstring entry{ fmt::format(L"\x1b]9001;1;{}\x7f{}\x7f{}\x7f{}\x07", trimmed, L"a comment", command, L"whatever extras we want") };
        wprintf(entry.c_str());
    }

    return 0;
}

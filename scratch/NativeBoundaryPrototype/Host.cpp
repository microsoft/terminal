// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <objbase.h>
#include "Boundary.h"

#include <iostream>
#include <vector>

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: BoundaryHost.exe <absolute settings model DLL path>\n";
        return 2;
    }

    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized))
    {
        std::cerr << "CoInitializeEx failed: " << initialized << '\n';
        return 1;
    }

    int result = 0;
    try
    {
        const std::vector<boundary::LaunchValues> inputs{
            {},
            { L"pwsh.exe -NoLogo", L"C:\\initial", L"Original", L"PowerShell", 3, true },
            { L"echo \"hello\"", L"C:\\space dir", L"\u03a9 title", L"\u65e5\u672c\u8a9e", 0, false }
        };
        for (const auto& input : inputs)
        {
            for (const auto& title : { std::wstring{}, std::wstring{ L"Edited \u03a9" } })
            {
                auto expected = input;
                expected.title = title;
                expected.directory = L"D:\\new directory";
                auto request = boundary::Build(input, title, expected.directory, argv[1]);
                if (boundary::Read(request) != expected)
                {
                    std::cerr << "Launch argument round-trip mismatch\n";
                    result = 1;
                }
            }
        }
        if (result == 0)
        {
            std::cout << "6 real-model launch argument round trips passed\n";
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    catch (...)
    {
        std::cerr << "Non-standard exception in model activation or launch pipeline\n";
        result = 1;
    }

    CoUninitialize();
    return result;
}

// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "Utils.h"

std::wstring VisualizeControlCodes(std::wstring str) noexcept
{
    for (auto& ch : str)
    {
        if (ch < 0x20)
        {
            ch += 0x2400;
        }
        else if (ch == 0x20)
        {
            ch = 0x2423; // replace space with ␣
        }
        else if (ch == 0x7f)
        {
            ch = 0x2421; // replace del with ␡
        }
    }
    return str;
}

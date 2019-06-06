// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#ifndef __STRING_HELPER_H__
#define __STRING_HELPER_H__

#pragma once

void AppendFormat(
    _In_ std::string& to,
    _In_z_ _Printf_format_string_ const char* format,
    ...)
{
    THROW_HR_IF_NULL(E_INVALIDARG, format);

    va_list args;
    va_start(args, format);

    const auto currentLength = to.length();
    const auto appendLength = _vscprintf(format, args); // _vscprintf The value returned does not include the terminating null character.

    THROW_HR_IF(E_FAIL, appendLength < 0);

    // sprintf_s guarantees that the buffer will be null-terminated. So allocate one more byte for null character and then remove it.
    to.resize(currentLength + appendLength + 1);

    const auto len = vsprintf_s(to.data() + currentLength, appendLength + 1, format, args);
    THROW_HR_IF(E_FAIL, len < 0);

    to.resize(currentLength + appendLength);

    va_end(args);
}

template<typename T>
void TrimLeft(_In_ T& str, _In_ const typename T::value_type ch)
{
    const auto pos = str.find_first_not_of(ch);
    if (pos != T::npos)
    {
        str.erase(0, pos);
    }
    else
    {
        // find_first_not_of returns npos when:
        // 1. str is empty.
        // 2. str contains only ch. (For example : str = "AAA", ch = 'A')
        // So here we should clear the string.
        str.clear();
    }
}

template<typename T>
void TrimRight(_In_ T& str, _In_ const typename T::value_type ch)
{
    const auto pos = str.find_last_not_of(ch);
    if (pos != T::npos)
    {
        str.resize(pos + 1);
    }
    else
    {
        str.clear();
    }
}

#endif

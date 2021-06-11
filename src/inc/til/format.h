// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <stdint.h>
#include <string>

#ifdef UNIT_TESTING
#endif

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    namespace format
    {
        inline char* format_uint8(char* ptr, uint8_t number)
        {
            if (number >= 100)
            {
                *ptr++ = static_cast<char>((number / 100) + '0');
            }
            if (number >= 10)
            {
                *ptr++ = static_cast<char>(((number / 10) % 10) + '0');
            }

            *ptr++ = static_cast<char>((number % 10) + '0');
            return ptr;
        }

        std::string_view format(unsigned int maxLength, const char* pFormat, ...)
        {
            va_list args;
            va_start(args, pFormat);

            char* buf = new char[maxLength];
            char* pDst = buf;

            const char* pSrc = pFormat;
            while (*pSrc != '\0')
            {
                if (*pSrc == '{' && *(pSrc + 1) == '}')
                {
                    pDst = format_uint8(pDst, va_arg(args, uint8_t));
                    pSrc += 2;
                    continue;
                }

                *pDst++ = *pSrc++;
            }

            va_end(args);

            return { buf, static_cast<std::string::size_type>(pDst - buf) };
        }
    }
}

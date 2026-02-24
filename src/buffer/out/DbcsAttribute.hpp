/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- DbcsAttribute.hpp

Abstract:
- helper class for storing double byte character set information about a cell in the output buffer.

Author(s):
- Austin Diviness (AustDi) 26-Jan-2018

Revision History:
--*/

#pragma once

enum class DbcsAttribute : uint8_t
{
    Single,
    Leading,
    Trailing,
};

constexpr WORD GeneratePublicApiAttributeFormat(DbcsAttribute attribute) noexcept
{
    switch (attribute)
    {
    case DbcsAttribute::Leading:
        return COMMON_LVB_LEADING_BYTE;
    case DbcsAttribute::Trailing:
        return COMMON_LVB_TRAILING_BYTE;
    default:
        return 0;
    }
}

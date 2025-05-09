// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <icu.h>

namespace til::ICU // Terminal Implementation Library. Also: "Today I Learned"
{
    using unique_uregex = wistd::unique_ptr<URegularExpression, wil::function_deleter<decltype(&uregex_close), &uregex_close>>;

    _TIL_INLINEPREFIX unique_uregex CreateRegex(const std::wstring_view& pattern, uint32_t flags, UErrorCode* status) noexcept
    {
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
        const auto re = uregex_open(reinterpret_cast<const char16_t*>(pattern.data()), gsl::narrow_cast<int32_t>(pattern.size()), flags, nullptr, status);
        // ICU describes the time unit as being dependent on CPU performance and "typically [in] the order of milliseconds",
        // but this claim seems highly outdated already. On my CPU from 2021, a limit of 4096 equals roughly 600ms.
        uregex_setTimeLimit(re, 4096, status);
        uregex_setStackLimit(re, 4 * 1024 * 1024, status);
        return unique_uregex{ re };
    }
}
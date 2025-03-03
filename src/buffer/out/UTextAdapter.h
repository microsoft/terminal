// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <icu.h>

class TextBuffer;

namespace Microsoft::Console::ICU
{
    using unique_uregex = wistd::unique_ptr<URegularExpression, wil::function_deleter<decltype(&uregex_close), &uregex_close>>;
    using unique_utext = wil::unique_struct<UText, decltype(&utext_close), &utext_close>;

    unique_utext UTextFromTextBuffer(const TextBuffer& textBuffer, til::CoordType rowBeg, til::CoordType rowEnd) noexcept;
    unique_uregex CreateRegex(const std::wstring_view& pattern, uint32_t flags, UErrorCode* status) noexcept;
    til::point_span BufferRangeFromMatch(UText* ut, URegularExpression* re);
}

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <icu.h>

class TextBuffer;

namespace Microsoft::Console::ICU
{
    using unique_utext = wil::unique_struct<UText, decltype(&utext_close), &utext_close>;

    unique_utext UTextFromTextBuffer(const TextBuffer& textBuffer, til::CoordType rowBeg, til::CoordType rowEnd) noexcept;
    til::point_span BufferRangeFromMatch(UText* ut, URegularExpression* re);
}

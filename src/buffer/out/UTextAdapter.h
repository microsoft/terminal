// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// Can't forward declare the UErrorCode enum. Thanks C++.
#include <icu.h>

class TextBuffer;
struct UText;

UText* UTextFromTextBuffer(UText* ut, const TextBuffer& textBuffer, til::CoordType rowBeg, til::CoordType rowEnd, UErrorCode* status) noexcept;
til::point_span BufferRangeFromUText(UText* ut, int64_t nativeIndexBeg, int64_t nativeIndexEnd);

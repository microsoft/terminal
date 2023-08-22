// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// Can't forward declare the UErrorCode enum. Thanks C++.
#include <icu.h>

class TextBuffer;

using unique_URegularExpression = wistd::unique_ptr<URegularExpression, wil::function_deleter<decltype(&uregex_close), &uregex_close>>;

UText UTextFromTextBuffer(const TextBuffer& textBuffer, til::CoordType rowBeg, til::CoordType rowEnd, UErrorCode* status) noexcept;
URegularExpression* CreateURegularExpression(const std::wstring_view& pattern, uint32_t flags, UErrorCode* status) noexcept;
til::point_span BufferRangeFromMatch(UText* ut, URegularExpression* re);

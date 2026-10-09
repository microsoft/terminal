// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../inc/FontInfoBase.hpp"

const std::wstring& FontInfoBase::GetFaceName() const noexcept
{
    return _faceName;
}

unsigned char FontInfoBase::GetFamily() const noexcept
{
    return _family;
}

unsigned int FontInfoBase::GetWeight() const noexcept
{
    return _weight;
}

unsigned int FontInfoBase::GetCodePage() const noexcept
{
    return _codePage;
}

void FontInfoBase::SetFaceName(std::wstring faceName) noexcept
{
    _faceName = std::move(faceName);
}

void FontInfoBase::SetFamily(unsigned char family) noexcept
{
    _family = family;
}

void FontInfoBase::SetWeight(unsigned int weight) noexcept
{
    _weight = weight;
}

void FontInfoBase::SetCodePage(unsigned int codePage) noexcept
{
    _codePage = codePage;
}

// Populates a fixed-length **null-terminated** buffer with the name of this font, truncating it as necessary.
// Positions within the buffer that are not filled by the font name are zeroed.
void FontInfoBase::FillLegacyNameBuffer(wchar_t (&buffer)[LF_FACESIZE]) const noexcept
{
    const auto toCopy = std::min(std::size(buffer) - 1, _faceName.size());
    const auto last = std::copy_n(_faceName.data(), toCopy, &buffer[0]);
    *last = L'\0';
}

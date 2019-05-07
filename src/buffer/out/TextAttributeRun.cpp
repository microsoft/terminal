// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "TextAttributeRun.hpp"

TextAttributeRun::TextAttributeRun() noexcept :
    _cchLength(0)
{
    SetAttributes(TextAttribute(0));
}

TextAttributeRun::TextAttributeRun(const size_t cchLength, const TextAttribute attr) noexcept :
    _cchLength(cchLength)
{
    SetAttributes(attr);
}

size_t TextAttributeRun::GetLength() const noexcept
{
    return _cchLength;
}

void TextAttributeRun::SetLength(const size_t cchLength) noexcept
{
    _cchLength = cchLength;
}

void TextAttributeRun::IncrementLength() noexcept
{
    _cchLength++;
}

void TextAttributeRun::DecrementLength() noexcept
{
    _cchLength--;
}

const TextAttribute& TextAttributeRun::GetAttributes() const noexcept
{
    return _attributes;
}

void TextAttributeRun::SetAttributes(const TextAttribute textAttribute) noexcept
{
    _attributes = textAttribute;
}

// Routine Description:
// - Sets the attributes of this run to the given legacy attributes
// Arguments:
// - wNew - the new value for this run's attributes
// Return Value:
// <none>
void TextAttributeRun::SetAttributesFromLegacy(const WORD wNew) noexcept
{
    _attributes.SetFromLegacy(wNew);
}

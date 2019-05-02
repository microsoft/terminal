/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TextAttributeRun.hpp

Abstract:
- contains data structure for run-length-encoding of text attribute data

Author(s):
- Michael Niksa (miniksa) 10-Apr-2014
- Paul Campbell (paulcam) 10-Apr-2014

Revision History:
- From components of output.h/.c
  by Therese Stowell (ThereseS) 1990-1991
- Pulled into its own file from textBuffer.hpp/cpp (AustDi, 2017)
--*/

#pragma once

#include "TextAttribute.hpp"

class TextAttributeRun final
{
public:
    TextAttributeRun() noexcept;
    TextAttributeRun(const size_t cchLength, const TextAttribute attr) noexcept;

    size_t GetLength() const noexcept;
    void SetLength(const size_t cchLength) noexcept;
    void IncrementLength() noexcept;
    void DecrementLength() noexcept;

    const TextAttribute& GetAttributes() const noexcept;
    void SetAttributes(const TextAttribute textAttribute) noexcept;
    void SetAttributesFromLegacy(const WORD wNew) noexcept;

private:
    size_t _cchLength;
    TextAttribute _attributes;

#ifdef UNIT_TESTING
    friend class AttrRowTests;
#endif
};

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
    TextAttributeRun() = default;
    TextAttributeRun(const size_t cchLength, const TextAttribute attr) noexcept :
        _cchLength(gsl::narrow<unsigned int>(cchLength))
    {
        SetAttributes(attr);
    }

    size_t GetLength() const noexcept { return _cchLength; }
    void SetLength(const size_t cchLength) noexcept { _cchLength = gsl::narrow<unsigned int>(cchLength); }
    void IncrementLength() noexcept { _cchLength++; }
    void DecrementLength() noexcept { _cchLength--; }

    const TextAttribute& GetAttributes() const noexcept { return _attributes; }
    void SetAttributes(const TextAttribute textAttribute) noexcept { _attributes = textAttribute; }

private:
    unsigned int _cchLength{ 0 };
    TextAttribute _attributes{ 0 };

#ifdef UNIT_TESTING
    friend class AttrRowTests;
#endif
};

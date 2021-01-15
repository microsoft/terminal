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

class TextAttributeRun final : public std::pair<TextAttribute, unsigned int>
{
public:
    using mybase = std::pair<TextAttribute, unsigned int>;

    using mybase::mybase;

    TextAttributeRun(const size_t cchLength, const TextAttribute attr) :
        mybase(attr, gsl::narrow<unsigned int>(cchLength))
    {
    }

    size_t GetLength() const noexcept { return mybase::second; }
    void SetLength(const size_t cchLength) noexcept { mybase::second = gsl::narrow<unsigned int>(cchLength); }
    void IncrementLength() noexcept { mybase::second++; }
    void DecrementLength() noexcept { mybase::second--; }

    const TextAttribute& GetAttributes() const noexcept { return mybase::first; }
    void SetAttributes(const TextAttribute textAttribute) noexcept { mybase::first = textAttribute; }
};

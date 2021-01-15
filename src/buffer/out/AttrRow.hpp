/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- AttrRow.hpp

Abstract:
- contains data structure for the attributes of one row of screen buffer

Author(s):
- Michael Niksa (miniksa) 10-Apr-2014
- Paul Campbell (paulcam) 10-Apr-2014

Revision History:
- From components of output.h/.c
  by Therese Stowell (ThereseS) 1990-1991
- Pulled into its own file from textBuffer.hpp/cpp (AustDi, 2017)
--*/

#pragma once

#include "til/rle.h"

#include "TextAttributeRun.hpp"

class ATTR_ROW final : public til::rle<TextAttribute, UINT>
{
public:
    using mybase = til::rle<TextAttribute, UINT>;

    using const_iterator = mybase::const_iterator;
    using const_reverse_iterator = mybase::const_reverse_iterator;

    using mybase::mybase; // use base constructor

    void Reset(const TextAttribute attr);

    TextAttribute GetAttrByColumn(const size_t column) const;
    TextAttribute GetAttrByColumn(const size_t column,
                                  size_t* const pApplies) const;

    std::vector<uint16_t> GetHyperlinks();

    bool SetAttrToEnd(const UINT iStart, const TextAttribute attr);
    void ReplaceAttrs(const TextAttribute& toBeReplacedAttr, const TextAttribute& replaceWith) noexcept;

    void Resize(const size_t newWidth);

    [[nodiscard]] HRESULT InsertAttrRuns(const gsl::span<const TextAttributeRun> newAttrs,
                                         const size_t iStart,
                                         const size_t iEnd,
                                         const size_t cBufferWidth);

    using mybase::begin;
    using mybase::cbegin;
    using mybase::cend;
    using mybase::end;

    using mybase::operator==;
};

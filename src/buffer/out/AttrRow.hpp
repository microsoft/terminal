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

#include "boost/container/small_vector.hpp"
#include "TextAttributeRun.hpp"
#include "AttrRowIterator.hpp"

class ATTR_ROW final
{
public:
    using const_iterator = typename AttrRowIterator;

    ATTR_ROW(const UINT cchRowWidth, const TextAttribute attr)
    noexcept;

    void Reset(const TextAttribute attr);

    TextAttribute GetAttrByColumn(const unsigned int column) const;
    TextAttribute GetAttrByColumn(const unsigned int column,
                                  unsigned int* const pApplies) const;

    size_t GetNumberOfRuns() const noexcept;

    size_t FindAttrIndex(const unsigned int index,
                         unsigned int* const pApplies) const;

    std::unordered_set<uint16_t> GetHyperlinks();

    bool SetAttrToEnd(const UINT iStart, const TextAttribute attr);
    void ReplaceAttrs(const TextAttribute& toBeReplacedAttr, const TextAttribute& replaceWith) noexcept;

    void Resize(const unsigned int newWidth);

    [[nodiscard]] HRESULT InsertAttrRuns(const gsl::span<const TextAttributeRun> newAttrs,
                                         const unsigned int iStart,
                                         const unsigned int iEnd,
                                         const unsigned int cBufferWidth);

    static std::vector<TextAttributeRun> PackAttrs(const std::vector<TextAttribute>& attrs);

    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;

    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;

    friend bool operator==(const ATTR_ROW& a, const ATTR_ROW& b) noexcept;
    friend class AttrRowIterator;

private:
    boost::container::small_vector<TextAttributeRun, 1> _list;
    unsigned int _cchRowWidth;

#ifdef UNIT_TESTING
    friend class AttrRowTests;
#endif
};

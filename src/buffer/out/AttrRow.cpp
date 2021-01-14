// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "AttrRow.hpp"

// Routine Description:
// - Takes an existing row of attributes, and changes the length so that it fills the NewWidth.
//     If the new size is bigger, then the last attr is extended to fill the NewWidth.
//     If the new size is smaller, the runs are cut off to fit.
// Arguments:
// - oldWidth - The original width of the row.
// - newWidth - The new width of the row.
// Return Value:
// - <none>, throws exceptions on failures.
void ATTR_ROW::Resize(const size_t newWidth)
{
    mybase::resize(gsl::narrow<UINT>(newWidth));
}

void ATTR_ROW::Reset(const TextAttribute attr)
{
    mybase::fill(attr);
}

// Routine Description:
// - returns a copy of the TextAttribute at the specified column
// Arguments:
// - column - the column to get the attribute for
// Return Value:
// - the text attribute at column
// Note:
// - will throw on error
TextAttribute ATTR_ROW::GetAttrByColumn(const size_t column) const
{
    return mybase::at(gsl::narrow<UINT>(column));
}

// Routine Description:
// - returns a copy of the TextAttribute at the specified column
// Arguments:
// - column - the column to get the attribute for
// - pApplies - if given, fills how long this attribute will apply for
// Return Value:
// - the text attribute at column
// Note:
// - will throw on error
TextAttribute ATTR_ROW::GetAttrByColumn(const size_t column,
                                        size_t* const pApplies) const
{
    UINT applies = 0;
    const auto attr = mybase::at(gsl::narrow<UINT>(column), applies);
    *pApplies = applies;
    return attr;
}

// Routine Description:
// - Finds the hyperlink IDs present in this row and returns them
// Return value:
// - The hyperlink IDs present in this row
std::vector<uint16_t> ATTR_ROW::GetHyperlinks()
{
    std::vector<uint16_t> ids;
    for (const auto& run : *this)
    {
        if (run.IsHyperlink())
        {
            ids.emplace_back(run.GetHyperlinkId());
        }
    }
    return ids;
}

// Routine Description:
// - Sets the attributes (colors) of all character positions from the given position through the end of the row.
// Arguments:
// - iStart - Starting index position within the row
// - attr - Attribute (color) to fill remaining characters with
// Return Value:
// - <none>
bool ATTR_ROW::SetAttrToEnd(const UINT iStart, const TextAttribute attr)
{
    mybase::fill(attr, iStart);
    return true;
}

// Method Description:
// - Replaces all runs in the row with the given toBeReplacedAttr with the new
//      attribute replaceWith.
// Arguments:
// - toBeReplacedAttr - the attribute to replace in this row.
// - replaceWith - the new value for the matching runs' attributes.
// Return Value:
// - <none>
void ATTR_ROW::ReplaceAttrs(const TextAttribute& toBeReplacedAttr, const TextAttribute& replaceWith) noexcept
{
    mybase::replace(toBeReplacedAttr, replaceWith);
}

// Routine Description:
// - Takes a array of attribute runs, and inserts them into this row from startIndex to endIndex.
// - For example, if the current row was was [{4, BLUE}], the merge string
//   was [{ 2, RED }], with (StartIndex, EndIndex) = (1, 2),
//   then the row would modified to be = [{ 1, BLUE}, {2, RED}, {1, BLUE}].
// Arguments:
// - rgInsertAttrs - The array of attrRuns to merge into this row.
// - cInsertAttrs - The number of elements in rgInsertAttrs
// - iStart - The index in the row to place the array of runs.
// - iEnd - the final index of the merge runs
// - BufferWidth - the width of the row.
// Return Value:
// - STATUS_NO_MEMORY if there wasn't enough memory to insert the runs
//   otherwise STATUS_SUCCESS if we were successful.
[[nodiscard]] HRESULT ATTR_ROW::InsertAttrRuns(const gsl::span<const TextAttributeRun> newAttrs,
                                               const size_t iStart,
                                               const size_t /*iEnd*/,
                                               const size_t /*cBufferWidth*/)
try
{
    UINT pos = gsl::narrow<UINT>(iStart);
    for (auto& attrPair : newAttrs)
    {
        mybase::insert(attrPair.GetAttributes(), pos, gsl::narrow<UINT>(attrPair.GetLength()));
    }

    return S_OK;
}
CATCH_RETURN()

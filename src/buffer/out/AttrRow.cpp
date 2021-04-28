// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "AttrRow.hpp"

// Routine Description:
// - constructor
// Arguments:
// - cchRowWidth - the length of the default text attribute
// - attr - the default text attribute
// Return Value:
// - constructed object
ATTR_ROW::ATTR_ROW(const UINT cchRowWidth, const TextAttribute attr) :
    _data(cchRowWidth, attr) {}

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
    _data.resize(gsl::narrow<UINT>(newWidth));
}

void ATTR_ROW::Reset(const TextAttribute attr)
{
    _data.assign(attr);
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
    return _data.at(gsl::narrow<UINT>(column));
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
    const auto attr = _data.at(gsl::narrow<UINT>(column), applies);
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
    _data.assign(attr, iStart);
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
    _data.replace(toBeReplacedAttr, replaceWith);
}

// Routine Description:
// - Takes an attribute run, and merges it into this row from start (inclusive) to start+length (exclusive).
// - For example, if the current row was was [{4, BLUE}], the merge arguments were
//   { newAttr = RED, start = 1, length = 2 }, then the row would modified to be
//   [{ 1, BLUE}, {2, RED}, {1, BLUE}].
// Arguments:
// - newAttr: The attribute to merge into this row.
// - start: The index in the row to place the array of runs
// - length: The run length of this attribute, or:
//           How many times this attribute is repeated starting at "start"
// Return Value:
// - <none>
void ATTR_ROW::MergeAttrRun(const TextAttribute newAttr, const size_t start, const size_t length)
{
    _data.assign(newAttr, gsl::narrow<UINT>(start), gsl::narrow<UINT>(length));
}

ATTR_ROW::const_iterator ATTR_ROW::begin() const noexcept
{
    return _data.begin();
}

ATTR_ROW::const_iterator ATTR_ROW::end() const noexcept
{
    return _data.end();
}

ATTR_ROW::const_iterator ATTR_ROW::cbegin() const noexcept
{
    return _data.cbegin();
}

ATTR_ROW::const_iterator ATTR_ROW::cend() const noexcept
{
    return _data.cend();
}

bool operator==(const ATTR_ROW& a, const ATTR_ROW& b) noexcept
{
    return a._data == b._data;
}

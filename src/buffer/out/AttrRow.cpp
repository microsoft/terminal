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
ATTR_ROW::ATTR_ROW(const uint16_t width, const TextAttribute attr) :
    _data(width, attr) {}

// Routine Description:
// - Sets all properties of the ATTR_ROW to default values
// Arguments:
// - attr - The default text attributes to use on text in this row.
void ATTR_ROW::Reset(const TextAttribute attr)
{
    _data.replace(0, _data.size(), attr);
}

// Routine Description:
// - Takes an existing row of attributes, and changes the length so that it fills the NewWidth.
//     If the new size is bigger, then the last attr is extended to fill the NewWidth.
//     If the new size is smaller, the runs are cut off to fit.
// Arguments:
// - oldWidth - The original width of the row.
// - newWidth - The new width of the row.
// Return Value:
// - <none>, throws exceptions on failures.
void ATTR_ROW::Resize(const uint16_t newWidth)
{
    _data.resize_trailing_extent(newWidth);
}

// Routine Description:
// - returns a copy of the TextAttribute at the specified column
// Arguments:
// - column - the column to get the attribute for
// Return Value:
// - the text attribute at column
// Note:
// - will throw on error
TextAttribute ATTR_ROW::GetAttrByColumn(const uint16_t column) const
{
    return _data.at(column);
}

// Routine Description:
// - Finds the hyperlink IDs present in this row and returns them
// Return value:
// - The hyperlink IDs present in this row
std::vector<uint16_t> ATTR_ROW::GetHyperlinks() const
{
    std::vector<uint16_t> ids;
    for (const auto& run : _data.runs())
    {
        if (run.value.IsHyperlink())
        {
            ids.emplace_back(run.value.GetHyperlinkId());
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
bool ATTR_ROW::SetAttrToEnd(const uint16_t beginIndex, const TextAttribute attr)
{
    _data.replace(gsl::narrow<uint16_t>(beginIndex), _data.size(), attr);
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
void ATTR_ROW::ReplaceAttrs(const TextAttribute& toBeReplacedAttr, const TextAttribute& replaceWith)
{
    _data.replace_values(toBeReplacedAttr, replaceWith);
}

// Routine Description:
// - Takes an attribute, and merges it into this row from beginIndex (inclusive) to endIndex (exclusive).
// - For example, if the current row was was [{4, BLUE}], the merge arguments were
//   { beginIndex = 1, endIndex = 3, newAttr = RED }, then the row would modified to be
//   [{ 1, BLUE}, {2, RED}, {1, BLUE}].
// Arguments:
// - beginIndex, endIndex: The [beginIndex, endIndex) range that's to be replaced with newAttr.
// - newAttr: The attribute to merge into this row.
// Return Value:
// - <none>
void ATTR_ROW::Replace(const uint16_t beginIndex, const uint16_t endIndex, const TextAttribute& newAttr)
{
    _data.replace(beginIndex, endIndex, newAttr);
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

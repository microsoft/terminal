// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../inc/Cluster.hpp"
#include "../../inc/unicode.hpp"
#include "../types/inc/convert.hpp"

using namespace Microsoft::Console::Render;

// Routine Description:
// - Instantiates a new cluster structure
// Arguments:
// - text - This is a view of the text that forms this cluster (one or more wchar_t*s)
// - columns - This is the number of columns in the grid that the cluster should consume when drawn
Cluster::Cluster(const std::wstring_view text, const size_t columns) :
    _text(text),
    _columns(columns)
{
}

// Routine Description:
// - Provides the embedded text as a single character
// - This might replace the string with the replacement character if it doesn't fit as one wchar_t
// Arguments:
// - <none>
// Return Value:
// - The only wchar_t in the string or the Unicode replacement character as appropriate.
const wchar_t Cluster::GetTextAsSingle() const noexcept
{
    try
    {
        return Utf16ToUcs2(_text);
    }
    CATCH_LOG();
    return UNICODE_REPLACEMENT;
}

// Routine Description:
// - Provides the string of wchar_ts for this cluster.
// Arguments:
// - <none>
// Return Value:
// - String view of wchar_ts.
const std::wstring_view& Cluster::GetText() const noexcept
{
    return _text;
}

// Routine Description:
// - Gets the number of columns in the grid that this character should consume
//   visually when rendered onto a line.
// Arguments:
// - <none>
// Return Value:
// - Number of columns to use when drawing (not a pixel count).
const size_t Cluster::GetColumns() const noexcept
{
    return _columns;
}

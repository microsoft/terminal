// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "textBufferTextIterator.hpp"

#include "CharRow.hpp"
#include "Row.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Types;

// Routine Description:
// - Narrows the view of a cell iterator into a text only iterator.
// Arguments:
// - A cell iterator
TextBufferTextIterator::TextBufferTextIterator(const TextBufferCellIterator& cellIt) :
    TextBufferCellIterator(cellIt)
{
}

// Routine Description:
// - Returns the text information from the text buffer position addressed by this iterator.
// Return Value:
// - Read only UTF-16 text data
const std::wstring_view TextBufferTextIterator::operator*() const
{
    return _view.Chars();
}

// Routine Description:
// - Returns the text information from the text buffer position addressed by this iterator.
// Return Value:
// - Read only UTF-16 text data
const std::wstring_view* TextBufferTextIterator::operator->() const
{
    return &_view.Chars();
}

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "inputReadHandleData.h"

INPUT_READ_HANDLE_DATA::INPUT_READ_HANDLE_DATA() :
    _isInputPending{ false },
    _isMultilineInput{ false },
    _readCount{ 0 },
    _buffer{}
{
}

bool INPUT_READ_HANDLE_DATA::IsInputPending() const
{
    return _isInputPending;
}

bool INPUT_READ_HANDLE_DATA::IsMultilineInput() const
{
    FAIL_FAST_IF(!_isInputPending); // we shouldn't have multiline input without a pending input.
    return _isMultilineInput;
}

void INPUT_READ_HANDLE_DATA::SaveMultilinePendingInput(std::wstring_view pending)
{
    _isMultilineInput = true;
    SavePendingInput(pending);
}

void INPUT_READ_HANDLE_DATA::SavePendingInput(std::wstring_view pending)
{
    _isInputPending = true;
    UpdatePending(pending);
}

void INPUT_READ_HANDLE_DATA::UpdatePending(std::wstring_view pending)
{
    _buffer = pending;
}

void INPUT_READ_HANDLE_DATA::CompletePending()
{
    _isInputPending = false;
    _isMultilineInput = false;
    _buffer.clear();
}

std::wstring_view INPUT_READ_HANDLE_DATA::GetPendingInput() const
{
    return _buffer;
}

void INPUT_READ_HANDLE_DATA::IncrementReadCount()
{
    _readCount++;
}

void INPUT_READ_HANDLE_DATA::DecrementReadCount()
{
    const size_t prevCount = _readCount.fetch_sub(1);
    FAIL_FAST_IF(prevCount == 0); // we just underflowed, that's a programming error.
}

size_t INPUT_READ_HANDLE_DATA::GetReadCount()
{
    return _readCount;
}

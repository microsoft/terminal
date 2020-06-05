/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- InputReadHandleData.h

Abstract:
- This file defines counters and state information related to reading input from the internal buffers
  when called from a particular input handle that has been given to a client application.
  It's used to know where the next bit of continuation should be if the same input handle doesn't have a big
  enough buffer and must split data over multiple returns.

Author:
- Michael Niksa (miniksa) 12-Oct-2016

Revision History:
- Adapted from original items in handle.h
--*/

#pragma once

class INPUT_READ_HANDLE_DATA
{
public:
    INPUT_READ_HANDLE_DATA();

    ~INPUT_READ_HANDLE_DATA() = default;
    INPUT_READ_HANDLE_DATA(const INPUT_READ_HANDLE_DATA&) = delete;
    INPUT_READ_HANDLE_DATA(INPUT_READ_HANDLE_DATA&&) = delete;
    INPUT_READ_HANDLE_DATA& operator=(const INPUT_READ_HANDLE_DATA&) & = delete;
    INPUT_READ_HANDLE_DATA& operator=(INPUT_READ_HANDLE_DATA&&) & = delete;

    void IncrementReadCount();
    void DecrementReadCount();
    size_t GetReadCount();

    bool IsInputPending() const;
    bool IsMultilineInput() const;

    void SavePendingInput(std::wstring_view pending);
    void SaveMultilinePendingInput(std::wstring_view pending);
    void UpdatePending(std::wstring_view pending);
    void CompletePending();

    std::wstring_view GetPendingInput() const;

private:
    bool _isInputPending;
    bool _isMultilineInput;

    std::wstring _buffer;

    std::atomic<size_t> _readCount;
};

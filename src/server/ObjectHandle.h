/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ObjectHandle.h

Abstract:
- This file defines a handle associated with a console input or output buffer object.
- This is used to expose a handle to a client application via the API.

Author:
- Michael Niksa (miniksa) 12-Oct-2016

Revision History:
- Adapted from original items in handle.h
--*/

#pragma once

class INPUT_READ_HANDLE_DATA;

class InputBuffer;

class SCREEN_INFORMATION;

#include "WaitQueue.h"

class ConsoleHandleData final
{
public:
    ConsoleHandleData(const ACCESS_MASK amAccess,
                      const ULONG ulShareAccess);

    void Initialize(const ULONG ulHandleType,
                    PVOID const pvClientPointer);

    ~ConsoleHandleData();
    ConsoleHandleData(const ConsoleHandleData&) = delete;
    ConsoleHandleData(ConsoleHandleData&&) = delete;
    ConsoleHandleData& operator=(const ConsoleHandleData&) & = delete;
    ConsoleHandleData& operator=(ConsoleHandleData&&) & = delete;

    [[nodiscard]] HRESULT GetInputBuffer(const ACCESS_MASK amRequested,
                                         _Outptr_ InputBuffer** const ppInputBuffer) const;
    [[nodiscard]] HRESULT GetScreenBuffer(const ACCESS_MASK amRequested,
                                          _Outptr_ SCREEN_INFORMATION** const ppScreenInfo) const;

    [[nodiscard]] HRESULT GetWaitQueue(_Outptr_ ConsoleWaitQueue** const ppWaitQueue) const;

    INPUT_READ_HANDLE_DATA* GetClientInput() const;

    bool IsReadAllowed() const;
    bool IsReadShared() const;
    bool IsWriteAllowed() const;
    bool IsWriteShared() const;

    // TODO: MSFT 9355178 Temporary public access to types... http://osgvsowi/9355178
    bool IsInputHandle() const
    {
        return _IsInput();
    }

    enum HandleType
    {
        NotReady = 0x0,
        Input = 0x1,
        Output = 0x2
    };

private:
    bool _IsInput() const;
    bool _IsOutput() const;

    [[nodiscard]] HRESULT _CloseInputHandle();
    [[nodiscard]] HRESULT _CloseOutputHandle();

    ACCESS_MASK const _amAccess;
    ULONG const _ulShareAccess;
    ULONG _ulHandleType;
    PVOID _pvClientPointer; // This will be a pointer to a SCREEN_INFORMATION or INPUT_INFORMATION object.
    std::unique_ptr<INPUT_READ_HANDLE_DATA> _pClientInput;
};

DEFINE_ENUM_FLAG_OPERATORS(ConsoleHandleData::HandleType);

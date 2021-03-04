/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- DeviceComm.h

Abstract:
- This module assists in communicating via IOCTL messages to and from an endpoint

Author:
- Dustin Howett (DuHowett) 10-Apr-2020

Revision History:
--*/

#pragma once

#include "../host/conapi.h"

class IDeviceComm
{
public:
    virtual ~IDeviceComm() = default;

    [[nodiscard]] virtual HRESULT SetServerInformation(_In_ CD_IO_SERVER_INFORMATION* const pServerInfo) const = 0;
    [[nodiscard]] virtual HRESULT ReadIo(_In_opt_ PCONSOLE_API_MSG const pReplyMsg,
                                         _Out_ CONSOLE_API_MSG* const pMessage) const = 0;
    [[nodiscard]] virtual HRESULT CompleteIo(_In_ CD_IO_COMPLETE* const pCompletion) const = 0;

    [[nodiscard]] virtual HRESULT ReadInput(_In_ CD_IO_OPERATION* const pIoOperation) const = 0;
    [[nodiscard]] virtual HRESULT WriteOutput(_In_ CD_IO_OPERATION* const pIoOperation) const = 0;

    [[nodiscard]] virtual HRESULT AllowUIAccess() const = 0;

    [[nodiscard]] virtual ULONG_PTR PutHandle(const void*) = 0;
    [[nodiscard]] virtual void* GetHandle(ULONG_PTR) const = 0;

    [[nodiscard]] virtual HRESULT GetServerHandle(_Out_ HANDLE* pHandle) const = 0;
};

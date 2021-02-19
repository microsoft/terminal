/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ConDrvDeviceComm.h

Abstract:
- This module assists in communicating via IOCTL messages to and from a Device server handle.

Author:
- Michael Niksa (MiNiksa) 14-Sept-2016

Revision History:
2020-04 split into an interface IDeviceComm and a concrete impl for ConDrv
--*/

#pragma once

#include "DeviceComm.h"

#include <wil\resource.h>

class ConDrvDeviceComm : public IDeviceComm
{
public:
    ConDrvDeviceComm(_In_ HANDLE Server);
    ~ConDrvDeviceComm();

    [[nodiscard]] HRESULT SetServerInformation(_In_ CD_IO_SERVER_INFORMATION* const pServerInfo) const override;
    [[nodiscard]] HRESULT ReadIo(_In_opt_ PCONSOLE_API_MSG const pReplyMsg,
                                 _Out_ CONSOLE_API_MSG* const pMessage) const override;
    [[nodiscard]] HRESULT CompleteIo(_In_ CD_IO_COMPLETE* const pCompletion) const override;

    [[nodiscard]] HRESULT ReadInput(_In_ CD_IO_OPERATION* const pIoOperation) const override;
    [[nodiscard]] HRESULT WriteOutput(_In_ CD_IO_OPERATION* const pIoOperation) const override;

    [[nodiscard]] HRESULT AllowUIAccess() const override;

    [[nodiscard]] ULONG_PTR PutHandle(const void*) override;
    [[nodiscard]] void* GetHandle(ULONG_PTR) const override;

    [[nodiscard]] HRESULT GetServerHandle(_Out_ HANDLE* pHandle) const override;

private:
    [[nodiscard]] HRESULT _CallIoctl(_In_ DWORD dwIoControlCode,
                                     _In_reads_bytes_opt_(cbInBufferSize) PVOID pInBuffer,
                                     _In_ DWORD cbInBufferSize,
                                     _Out_writes_bytes_opt_(cbOutBufferSize) PVOID pOutBuffer,
                                     _In_ DWORD cbOutBufferSize) const;

    wil::unique_handle _Server;
};

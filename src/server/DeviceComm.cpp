// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "DeviceComm.h"

DeviceComm::DeviceComm(_In_ HANDLE Server) :
    _Server(Server)
{
    THROW_HR_IF(E_HANDLE, Server == INVALID_HANDLE_VALUE);
}

DeviceComm::~DeviceComm()
{
}

// Routine Description:
// - Needs to be called once per server session and typically as the absolute first operation.
// - This sets up the driver with the input event that it will need to coordinate with when client
//   applications attempt to read data and need to be blocked. (It will be the signal to unblock those clients.)
// Arguments:
// - pServerInfo - Structure containing information required to initialize driver state for this console connection.
// Return Value:
// - HRESULT S_OK or suitable error.
[[nodiscard]] HRESULT DeviceComm::SetServerInformation(_In_ CD_IO_SERVER_INFORMATION* const pServerInfo) const
{
    return _CallIoctl(IOCTL_CONDRV_SET_SERVER_INFORMATION,
                      pServerInfo,
                      sizeof(*pServerInfo),
                      nullptr,
                      0);
}

// Routine Description:
// - Retrieves a packet message from the driver representing the next action/activity that should be performed.
// Arguments:
// - pCompletion - Optional completion structure from the previous activity (can be used in lieu of calling CompleteIo seperately.)
// - pMessage - A structure to hold the message data retrieved from the driver.
// Return Value:
// - HRESULT S_OK or suitable error.
[[nodiscard]] HRESULT DeviceComm::ReadIo(_In_opt_ PCONSOLE_API_MSG const pReplyMsg,
                                         _Out_ CONSOLE_API_MSG* const pMessage) const
{
    HRESULT hr = _CallIoctl(IOCTL_CONDRV_READ_IO,
                            pReplyMsg == nullptr ? nullptr : &pReplyMsg->Complete,
                            pReplyMsg == nullptr ? 0 : sizeof(pReplyMsg->Complete),
                            &pMessage->Descriptor,
                            sizeof(CONSOLE_API_MSG) - FIELD_OFFSET(CONSOLE_API_MSG, Descriptor));

    if (hr == HRESULT_FROM_WIN32(ERROR_IO_PENDING))
    {
        WaitForSingleObjectEx(_Server.get(), 0, FALSE);
        hr = S_OK; // TODO: MSFT: 9115192 - ??? This isn't really relevant anymore with a switch from NtDeviceIoControlFile to DeviceIoControl...
    }

    return hr;
}

// Routine Description:
// - Marks an action/activity as completed to the driver so control/responses can be returned to the client application.
// Arguments:
// - pCompletion - Completion structure from the previous activity (can be used in lieu of calling CompleteIo seperately.)
// Return Value:
// - HRESULT S_OK or suitable error.
[[nodiscard]] HRESULT DeviceComm::CompleteIo(_In_ CD_IO_COMPLETE* const pCompletion) const
{
    return _CallIoctl(IOCTL_CONDRV_COMPLETE_IO,
                      pCompletion,
                      sizeof(*pCompletion),
                      nullptr,
                      0);
}

// Routine Description:
// - Used to retrieve any buffered input data related to an action/activity message.
// Arguments:
// - pIoOperation - Structure containing the identifier matching the action/activity message and containing a suitable buffer space
//                  to hold retrieved buffered input data from the client application.
// Return Value:
// - HRESULT S_OK or suitable error.
[[nodiscard]] HRESULT DeviceComm::ReadInput(_In_ CD_IO_OPERATION* const pIoOperation) const
{
    return _CallIoctl(IOCTL_CONDRV_READ_INPUT,
                      pIoOperation,
                      sizeof(*pIoOperation),
                      nullptr,
                      0);
}

// Routine Description:
// - Used to return any buffered output data related to an action/activity message.
// Arguments:
// - pIoOperation - Structure containing the identifier matching the action/activity message and containing a suitable buffer space
//                  to hold buffered output data to be sent to the client application.
// Return Value:
// - HRESULT S_OK or suitable error.
[[nodiscard]] HRESULT DeviceComm::WriteOutput(_In_ CD_IO_OPERATION* const pIoOperation) const
{
    return _CallIoctl(IOCTL_CONDRV_WRITE_OUTPUT,
                      pIoOperation,
                      sizeof(*pIoOperation),
                      nullptr,
                      0);
}

// Routine Description:
// - To be called when the console instantiates UI to permit low-level UIAccess patterns to be used for retrieval of
//   accessibility data from the console session.
// Arguments:
// - <none>
// Return Value:
// - HRESULT S_OK or suitable error.
[[nodiscard]] HRESULT DeviceComm::AllowUIAccess() const
{
    return _CallIoctl(IOCTL_CONDRV_ALLOW_VIA_UIACCESS,
                      nullptr,
                      0,
                      nullptr,
                      0);
}

// Routine Description:
// - For internal use. This function will send the appropriate control code verb and buffers to the driver and return a result.
// - Usage of the optional buffers depends on which verb is sent and is specific to the particular driver and its protocol.
// Arguments:
// - dwIoControlCode - The action code to send to the driver
// - pInBuffer - An optional buffer to send as input with the verb. Usage depends on the control code.
// - cbInBufferSize - The length in bytes of the optional input buffer.
// - pOutBuffer - An optional buffer to send as output with the verb. Usage depends on the control code.
// - cbOutBufferSize - The length in bytes of the optional output buffer.
// Return Value:
// - HRESULT S_OK or suitable error.
[[nodiscard]] HRESULT DeviceComm::_CallIoctl(_In_ DWORD dwIoControlCode,
                                             _In_reads_bytes_opt_(cbInBufferSize) PVOID pInBuffer,
                                             _In_ DWORD cbInBufferSize,
                                             _Out_writes_bytes_opt_(cbOutBufferSize) PVOID pOutBuffer,
                                             _In_ DWORD cbOutBufferSize) const
{
    // See: https://msdn.microsoft.com/en-us/library/windows/desktop/aa363216(v=vs.85).aspx
    // Written is unused but cannot be nullptr because we aren't using overlapped.
    DWORD cbWritten = 0;
    RETURN_IF_WIN32_BOOL_FALSE(DeviceIoControl(_Server.get(),
                                               dwIoControlCode,
                                               pInBuffer,
                                               cbInBufferSize,
                                               pOutBuffer,
                                               cbOutBufferSize,
                                               &cbWritten,
                                               nullptr));

    return S_OK;
}

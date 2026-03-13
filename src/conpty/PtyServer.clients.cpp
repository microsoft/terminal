#include "pch.h"
#include "PtyServer.h"

#include <algorithm>

// Reads [part of] the input payload of the given message from the driver.
// Analogous to the OG ReadMessageInput() in csrutil.cpp.
//
// For CONSOLE_IO_CONNECT, offset is 0 and the payload is a CONSOLE_SERVER_MSG.
// For CONSOLE_IO_USER_DEFINED, offset would typically be past the message header.
__declspec(noinline) void PtyServer::readInput(const CD_IO_DESCRIPTOR& desc, ULONG offset, void* buffer, ULONG size)
{
    CD_IO_OPERATION op{};
    op.Identifier = desc.Identifier;
    op.Buffer.Offset = offset;
    op.Buffer.Data = buffer;
    op.Buffer.Size = size;
    THROW_IF_NTSTATUS_FAILED(ioctl(IOCTL_CONDRV_READ_INPUT, &op, sizeof(op), nullptr, 0));
}

// Completes a message with the given completion descriptor.
// Analogous to the OG ConsoleComplete() in csrutil.cpp.
//
// This sends the reply out-of-band (via IOCTL_CONDRV_COMPLETE_IO) rather than
// piggybacking on the next IOCTL_CONDRV_READ_IO call. Used when the reply
// carries write data (e.g. CD_CONNECTION_INFORMATION for CONNECT).
void PtyServer::completeIo(CD_IO_COMPLETE& completion)
{
    THROW_IF_NTSTATUS_FAILED(ioctl(IOCTL_CONDRV_COMPLETE_IO, &completion, sizeof(completion), nullptr, 0));
}

PtyClient* PtyServer::findClient(ULONG_PTR handle)
{
    auto ptr = reinterpret_cast<PtyClient*>(handle);
    for (auto& c : m_clients)
    {
        if (c.get() == ptr)
        {
            return ptr;
        }
    }
    return nullptr;
}

// Handles CONSOLE_IO_CONNECT messages.
//
// Protocol (from the OG ConsoleHandleConnectionRequest in srvinit.cpp):
//  1. Read the CONSOLE_SERVER_MSG from the client's input payload.
//  2. Validate string lengths and null termination.
//  3. Allocate per-process tracking data.
//  4. Mark the first connection as the root process.
//  5. Allocate IO handles for input and output.
//  6. Reply with CD_CONNECTION_INFORMATION via IOCTL_CONDRV_COMPLETE_IO.
//
// Returns STATUS_SUCCESS if the reply was sent via completeIo.
// Returns a failure NTSTATUS if the caller should reply inline with that status.
void PtyServer::handleConnect(CONSOLE_API_MSG& msg)
{
    // 1. Read the CONSOLE_SERVER_MSG payload from the client.
    CONSOLE_SERVER_MSG data{};
    readInput(msg.Descriptor, 0, &data, sizeof(data));

    // 2. Validate that strings are within the buffers and null-terminated.
    if ((data.ApplicationNameLength > (sizeof(data.ApplicationName) - sizeof(WCHAR))) ||
        (data.TitleLength > (sizeof(data.Title) - sizeof(WCHAR))) ||
        (data.CurrentDirectoryLength > (sizeof(data.CurrentDirectory) - sizeof(WCHAR))) ||
        (data.ApplicationName[data.ApplicationNameLength / sizeof(WCHAR)] != UNICODE_NULL) ||
        (data.Title[data.TitleLength / sizeof(WCHAR)] != UNICODE_NULL) ||
        (data.CurrentDirectory[data.CurrentDirectoryLength / sizeof(WCHAR)] != UNICODE_NULL))
    {
        THROW_NTSTATUS(STATUS_INVALID_BUFFER_SIZE);
    }

    // 3. Allocate per-process tracking data.
    const auto processId = static_cast<DWORD>(msg.Descriptor.Process);

    auto client = std::make_unique<PtyClient>();
    client->processId = processId;
    client->processGroupId = data.ProcessGroupId;

    // 4. The first connection is the root process (console owner).
    client->rootProcess = !m_initialized;

    // 5. Allocate opaque handle IDs for input and output.
    //    The driver echoes these back in Descriptor.Object for future IO messages.
    client->inputHandle = m_nextHandleId++;
    client->outputHandle = m_nextHandleId++;

    if (!m_initialized)
    {
        m_initialized = true;
    }

    auto clientPtr = client.get();
    m_clients.push_back(std::move(client));

    // 6. Build the reply with connection information.
    //    The driver stores these opaque values and echoes them back in future messages:
    //    - Process  → Descriptor.Process  (for DISCONNECT, etc.)
    //    - Input    → Descriptor.Object   (for RAW_READ, etc.)
    //    - Output   → Descriptor.Object   (for RAW_WRITE, etc.)
    CD_CONNECTION_INFORMATION connInfo{};
    connInfo.Process = reinterpret_cast<ULONG_PTR>(clientPtr);
    connInfo.Input = clientPtr->inputHandle;
    connInfo.Output = clientPtr->outputHandle;

    CD_IO_COMPLETE completion{};
    completion.Identifier = msg.Descriptor.Identifier;
    completion.IoStatus.Status = STATUS_SUCCESS;
    completion.IoStatus.Information = sizeof(CD_CONNECTION_INFORMATION);
    completion.Write.Data = &connInfo;
    completion.Write.Size = sizeof(CD_CONNECTION_INFORMATION);

    completeIo(completion);

    // TODO: Failed to deliver the reply — clean up the client we just added.
    //std::erase_if(m_clients, [clientPtr](const auto& c) { return c.get() == clientPtr; });
}

// Handles CONSOLE_IO_DISCONNECT messages.
//
// Protocol (from the OG ConsoleClientDisconnectRoutine / RemoveConsole in srvinit.cpp):
//  1. Look up the process from Descriptor.Process (the opaque value we set in CONNECT).
//  2. Free per-process data (handles, command history, etc.).
//
// The caller always replies with STATUS_SUCCESS inline.
void PtyServer::handleDisconnect(CONSOLE_API_MSG& msg)
{
    auto client = findClient(msg.Descriptor.Process);
    if (!client)
    {
        return;
    }

    std::erase_if(m_clients, [client](const auto& c) { return c.get() == client; });
}

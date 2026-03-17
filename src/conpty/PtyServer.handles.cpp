#include "pch.h"
#include "PtyServer.h"

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
NTSTATUS PtyServer::handleConnect()
{
    // 1. Read the CONSOLE_SERVER_MSG payload from the client.
    CONSOLE_SERVER_MSG data{};
    readInput(0, &data, sizeof(data));

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
    const auto processId = static_cast<DWORD>(m_req.Descriptor.Process);

    auto client = std::make_unique<PtyClient>();
    client->processId = processId;
    client->processGroupId = data.ProcessGroupId;

    // 4. The first connection is the root process (console owner).
    client->rootProcess = !m_initialized;

    // 5. Allocate IO handles for input and output.
    client->inputHandle = allocateHandle(CONSOLE_INPUT_HANDLE, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE);
    client->outputHandle = allocateHandle(CONSOLE_OUTPUT_HANDLE, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE);

    if (!m_initialized)
    {
        m_initialized = true;

        // Capture the initial title from the client's startup info.
        m_title.assign(data.Title, data.TitleLength / sizeof(WCHAR));
        m_originalTitle = m_title;

        // Capture initial buffer/window sizes if the client provided them.
        if (data.ScreenBufferSize.X > 0 && data.ScreenBufferSize.Y > 0)
        {
            m_bufferSize = data.ScreenBufferSize;
        }
        if (data.WindowSize.X > 0 && data.WindowSize.Y > 0)
        {
            m_viewSize = data.WindowSize;
        }
    }

    auto clientPtr = client.get();
    m_clients.push_back(std::move(client));

    // 6. Build the reply with connection information.
    CD_CONNECTION_INFORMATION connInfo{};
    connInfo.Process = reinterpret_cast<ULONG_PTR>(clientPtr);
    connInfo.Input = clientPtr->inputHandle;
    connInfo.Output = clientPtr->outputHandle;

    CD_IO_COMPLETE completion{};
    completion.Identifier = m_req.Descriptor.Identifier;
    completion.IoStatus.Status = STATUS_SUCCESS;
    completion.IoStatus.Information = sizeof(CD_CONNECTION_INFORMATION);
    completion.Write.Data = &connInfo;
    completion.Write.Size = sizeof(CD_CONNECTION_INFORMATION);

    completeIo(completion);
    return STATUS_NO_RESPONSE;
}

// Handles CONSOLE_IO_DISCONNECT messages.
//
// Protocol (from the OG ConsoleClientDisconnectRoutine / RemoveConsole in srvinit.cpp):
//  1. Look up the process from Descriptor.Process (the opaque value we set in CONNECT).
//  2. Free per-process data (handles, command history, etc.).
//
// The caller always replies with STATUS_SUCCESS inline.
NTSTATUS PtyServer::handleDisconnect()
{
    auto client = findClient(m_req.Descriptor.Process);
    if (!client)
    {
        return STATUS_SUCCESS;
    }

    // Cancel any pending IOs from this client.
    cancelPendingIOs(m_req.Descriptor.Process);

    // Free the client's IO handles, mirroring OG FreeProcessData which calls
    // ConsoleCloseHandle on InputHandle and OutputHandle.
    if (client->inputHandle)
    {
        freeHandle(client->inputHandle);
    }
    if (client->outputHandle)
    {
        freeHandle(client->outputHandle);
    }

    std::erase_if(m_clients, [client](const auto& c) { return c.get() == client; });
    return STATUS_SUCCESS;
}

// Handle management.
//
// Analogous to OG AllocateIoHandle (handle.cpp). The OG creates a CONSOLE_HANDLE_DATA
// with share/access tracking and a pointer to the underlying console object.
// We create a lightweight PtyHandle and return its pointer cast to ULONG_PTR.
ULONG_PTR PtyServer::allocateHandle(ULONG handleType, ACCESS_MASK access, ULONG shareMode)
{
    auto h = std::make_unique<PtyHandle>();
    h->handleType = handleType;
    h->access = access;
    h->shareMode = shareMode;
    auto ptr = reinterpret_cast<ULONG_PTR>(h.get());
    m_handles.push_back(std::move(h));
    return ptr;
}

// Analogous to OG ConsoleCloseHandle → FreeConsoleHandle (handle.cpp).
void PtyServer::freeHandle(ULONG_PTR handle)
{
    auto ptr = reinterpret_cast<PtyHandle*>(handle);
    std::erase_if(m_handles, [ptr](const auto& h) { return h.get() == ptr; });
}

// Handles CONSOLE_IO_CREATE_OBJECT messages.
//
// Protocol (from OG ConsoleCreateObject in srvinit.cpp):
//  1. Read CD_CREATE_OBJECT_INFORMATION from the message (already in msg.CreateObject).
//  2. Resolve CD_IO_OBJECT_TYPE_GENERIC based on DesiredAccess.
//  3. Allocate a handle of the appropriate type.
//  4. Reply via completeIo with the handle value in IoStatus.Information.
NTSTATUS PtyServer::handleCreateObject()
{
    auto& info = m_req.CreateObject;

    // Resolve generic object type based on desired access, matching OG behavior.
    if (info.ObjectType == CD_IO_OBJECT_TYPE_GENERIC)
    {
        if ((info.DesiredAccess & (GENERIC_READ | GENERIC_WRITE)) == GENERIC_READ)
        {
            info.ObjectType = CD_IO_OBJECT_TYPE_CURRENT_INPUT;
        }
        else if ((info.DesiredAccess & (GENERIC_READ | GENERIC_WRITE)) == GENERIC_WRITE)
        {
            info.ObjectType = CD_IO_OBJECT_TYPE_CURRENT_OUTPUT;
        }
    }

    ULONG_PTR handle = 0;

    switch (info.ObjectType)
    {
    case CD_IO_OBJECT_TYPE_CURRENT_INPUT:
        handle = allocateHandle(CONSOLE_INPUT_HANDLE, info.DesiredAccess, info.ShareMode);
        break;

    case CD_IO_OBJECT_TYPE_CURRENT_OUTPUT:
        handle = allocateHandle(CONSOLE_OUTPUT_HANDLE, info.DesiredAccess, info.ShareMode);
        break;

    case CD_IO_OBJECT_TYPE_NEW_OUTPUT:
        // In the OG, this creates a new screen buffer via ConsoleCreateScreenBuffer.
        // For now, we allocate a handle that tracks as an output handle.
        handle = allocateHandle(CONSOLE_OUTPUT_HANDLE, info.DesiredAccess, info.ShareMode);
        break;

    default:
        THROW_NTSTATUS(STATUS_INVALID_PARAMETER);
    }

    // Reply with the handle value in IoStatus.Information.
    // The driver stores this and echoes it back in Descriptor.Object for future IO.
    CD_IO_COMPLETE completion{};
    completion.Identifier = m_req.Descriptor.Identifier;
    completion.IoStatus.Status = STATUS_SUCCESS;
    completion.IoStatus.Information = handle;

    completeIo(completion);
    return STATUS_NO_RESPONSE;
}

// Handles CONSOLE_IO_CLOSE_OBJECT messages.
//
// Protocol (from OG SrvCloseHandle in stream.cpp):
//  1. Descriptor.Object contains the opaque handle value.
//  2. Close/free the handle.
//
// The caller replies with STATUS_SUCCESS inline.
NTSTATUS PtyServer::handleCloseObject()
{
    freeHandle(m_req.Descriptor.Object);
    return STATUS_SUCCESS;
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

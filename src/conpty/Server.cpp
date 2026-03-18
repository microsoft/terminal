#include "pch.h"
#include "Server.h"

#include <cassert>
#include <wil/nt_result_macros.h>

#define ProcThreadAttributeConsoleReference 10

#define PROC_THREAD_ATTRIBUTE_CONSOLE_REFERENCE \
    ProcThreadAttributeValue(10, FALSE, TRUE, FALSE)

#pragma warning(disable : 4100 4189)

HRESULT WINAPI PtyCreateServer(REFIID riid, void** server)
try
{
    if (server == nullptr)
    {
        return E_POINTER;
    }

    *server = nullptr;

    if (riid == __uuidof(IPtyServer))
    {
        *server = static_cast<IPtyServer*>(new Server());
        return S_OK;
    }

    return E_NOINTERFACE;
}
CATCH_RETURN()

Server::Server()
{
    m_server = createHandle(nullptr, L"\\Device\\ConDrv\\Server", false, false);
    m_inputAvailableEvent.create(wil::EventOptions::ManualReset);

    CD_IO_SERVER_INFORMATION info{
        .InputAvailableEvent = m_inputAvailableEvent.get(),
    };
    THROW_IF_FAILED(ioctl(IOCTL_CONDRV_SET_SERVER_INFORMATION, &info, sizeof(CD_IO_SERVER_INFORMATION), nullptr, 0));
}

#pragma region IUnknown

HRESULT Server::QueryInterface(const IID& riid, void** ppvObject)
{
    if (ppvObject == nullptr)
    {
        return E_POINTER;
    }

    if (riid == __uuidof(IPtyServer) || riid == __uuidof(IUnknown))
    {
        *ppvObject = static_cast<IPtyServer*>(this);
        AddRef();
        return S_OK;
    }

    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG Server::AddRef()
{
    return m_refCount.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG Server::Release()
{
    const auto count = m_refCount.fetch_sub(1, std::memory_order_relaxed) - 1;
    if (count == 0)
    {
        delete this;
    }
    return count;
}

HRESULT Server::SetHost(IPtyHost* host)
{
    m_host = host;
    return S_OK;
}

#pragma endregion

#pragma region IPtyServer

HRESULT Server::WriteUTF8(PTY_UTF8_STRING input)
try
{
    m_input.write({ input.data, input.length });
    m_inputAvailableEvent.SetEvent();
    drainPendingInputReads();
    return S_OK;
}
CATCH_RETURN()

HRESULT Server::WriteUTF16(PTY_UTF16_STRING input)
try
{
    // TODO
    return S_OK;
}
CATCH_RETURN()

HRESULT Server::Run()
{
    CD_IO_COMPLETE res{};
    NTSTATUS status = STATUS_NO_RESPONSE;

    while (true)
    {
        {
            void* in = nullptr;
            DWORD inLen = 0;

            if (status != STATUS_NO_RESPONSE)
            {
                res.Identifier = m_req.Descriptor.Identifier;
                res.IoStatus.Status = status;
                res.Write.Data = const_cast<uint8_t*>(m_resData.data());
                res.Write.Size = static_cast<ULONG>(m_resData.size());
                in = &res;
                inLen = sizeof(res);
            }

            status = ioctl(IOCTL_CONDRV_READ_IO, in, inLen, &m_req, sizeof(m_req));

            m_resData = {};
            m_resBuffer.clear();
        }
        if (!NT_SUCCESS(status))
        {
            if (status == STATUS_PIPE_DISCONNECTED)
            {
                return S_OK;
            }
            return HRESULT_FROM_NT(status);
        }

        try
        {
            switch (m_req.Descriptor.Function)
            {
            case CONSOLE_IO_CONNECT:
                status = handleConnect();
                break;
            case CONSOLE_IO_DISCONNECT:
                status = handleDisconnect();
                break;
            case CONSOLE_IO_CREATE_OBJECT:
                status = handleCreateObject();
                break;
            case CONSOLE_IO_CLOSE_OBJECT:
                status = handleCloseObject();
                break;
            case CONSOLE_IO_RAW_WRITE:
                status = handleRawWrite();
                break;
            case CONSOLE_IO_RAW_READ:
                status = handleRawRead();
                break;
            case CONSOLE_IO_USER_DEFINED:
                status = handleUserDefined();
                break;
            case CONSOLE_IO_RAW_FLUSH:
                status = handleRawFlush();
                break;
            default:
                status = STATUS_UNSUCCESSFUL;
                break;
            }
        }
        catch (...)
        {
            status = wil::StatusFromCaughtException();
        }
    }
}

HRESULT Server::CreateProcessW(
    LPCWSTR lpApplicationName,
    LPWSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    LPVOID lpEnvironment,
    LPCWSTR lpCurrentDirectory,
    LPPROCESS_INFORMATION lpProcessInformation)
try
{
    const auto proc = GetCurrentProcess();
    uint64_t attrListBuffer[16];
    STARTUPINFOEX si{};

    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    si.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(&attrListBuffer[0]);

    auto listSize = sizeof(attrListBuffer);
    THROW_IF_WIN32_BOOL_FALSE(InitializeProcThreadAttributeList(si.lpAttributeList, 2, 0, &listSize));
    const auto cleanup = wil::scope_exit([&] {
        DeleteProcThreadAttributeList(si.lpAttributeList);
    });

    std::array<unique_nthandle, 4> handles{
        createHandle(m_server.get(), L"\\Reference", false, true),
        createHandle(m_server.get(), L"\\Input", true, true),
        createHandle(m_server.get(), L"\\Output", true, true),
        nullptr,
    };
    THROW_IF_WIN32_BOOL_FALSE(DuplicateHandle(proc, handles[2].get(), proc, handles[3].addressof(), 0, TRUE, DUPLICATE_SAME_ACCESS));

    THROW_IF_WIN32_BOOL_FALSE(UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_CONSOLE_REFERENCE, handles[0].addressof(), sizeof(HANDLE), nullptr, nullptr));

    // bInheritHandles=TRUE is required in order to use STARTF_USESTDHANDLES.
    // We can fake bInheritHandles=FALSE anyway, by using PROC_THREAD_ATTRIBUTE_HANDLE_LIST.
    if (!bInheritHandles)
    {
        // NOTE: UpdateProcThreadAttribute doesn't copy the handle values!
        // The given lpValue pointers have to be valid until the call to CreateProcessW!
        THROW_IF_WIN32_BOOL_FALSE(UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles[1].addressof(), 3 * sizeof(HANDLE), nullptr, nullptr));
    }

    si.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = handles[1].get();
    si.StartupInfo.hStdOutput = handles[2].get();
    si.StartupInfo.hStdError = handles[3].get();

    return ::CreateProcessW(
        lpApplicationName,
        lpCommandLine,
        lpProcessAttributes,
        lpThreadAttributes,
        TRUE,
        dwCreationFlags | EXTENDED_STARTUPINFO_PRESENT,
        lpEnvironment,
        lpCurrentDirectory,
        &si.StartupInfo,
        lpProcessInformation);
}
CATCH_RETURN()

#pragma endregion

unique_nthandle Server::createHandle(HANDLE parent, const wchar_t* typeName, bool inherit, bool synchronous)
{
    UNICODE_STRING name;
    RtlInitUnicodeString(&name, typeName);

    ULONG attrFlags = OBJ_CASE_INSENSITIVE;
    WI_SetFlagIf(attrFlags, OBJ_INHERIT, inherit);

    OBJECT_ATTRIBUTES attr;
    InitializeObjectAttributes(&attr, &name, attrFlags, parent, nullptr);

    ULONG options = 0;
    WI_SetFlagIf(options, FILE_SYNCHRONOUS_IO_NONALERT, synchronous);

    HANDLE handle;
    IO_STATUS_BLOCK ioStatus;
    THROW_IF_NTSTATUS_FAILED(NtCreateFile(
        /* FileHandle        */ &handle,
        /* DesiredAccess     */ FILE_GENERIC_READ | FILE_GENERIC_WRITE,
        /* ObjectAttributes  */ &attr,
        /* IoStatusBlock     */ &ioStatus,
        /* AllocationSize    */ nullptr,
        /* FileAttributes    */ 0,
        /* ShareAccess       */ FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        /* CreateDisposition */ FILE_CREATE,
        /* CreateOptions     */ options,
        /* EaBuffer          */ nullptr,
        /* EaLength          */ 0));
    return unique_nthandle{ handle };
}

NTSTATUS Server::ioctl(DWORD code, void* in, DWORD inLen, void* out, DWORD outLen) const
{
    assert((in == nullptr) == (inLen == 0));
    assert((out == nullptr) == (outLen == 0));

    IO_STATUS_BLOCK iosb;
    auto status = NtDeviceIoControlFile(m_server.get(), nullptr, nullptr, nullptr, &iosb, code, in, inLen, out, outLen);

    if (status == STATUS_PENDING)
    {
        // Operation must complete before iosb is destroyed
        status = NtWaitForSingleObject(m_server.get(), FALSE, nullptr);
        if (NT_SUCCESS(status))
        {
            status = iosb.Status;
        }
    }

    return status;
}

// Reads [part of] the input payload of the current message from the driver.
// Analogous to the OG ReadMessageInput() in csrutil.cpp.
//
// For CONSOLE_IO_CONNECT, offset is 0 and the payload is a CONSOLE_SERVER_MSG.
// For CONSOLE_IO_USER_DEFINED, offset would typically be past the message header.
void Server::readInput(ULONG offset, void* buffer, ULONG size)
{
    CD_IO_OPERATION op{};
    op.Identifier = m_req.Descriptor.Identifier;
    op.Buffer.Offset = offset;
    op.Buffer.Data = buffer;
    op.Buffer.Size = size;
    THROW_IF_NTSTATUS_FAILED(ioctl(IOCTL_CONDRV_READ_INPUT, &op, sizeof(op), nullptr, 0));
}

// Reads the trailing input payload (after the API descriptor struct) for
// USER_DEFINED messages. Analogous to OG GetInputBuffer() in csrutil.cpp.
std::vector<uint8_t> Server::readTrailingInput()
{
    const auto readOffset = m_req.msgHeader.ApiDescriptorSize + sizeof(CONSOLE_MSG_HEADER);
    if (readOffset > m_req.Descriptor.InputSize)
    {
        THROW_NTSTATUS(STATUS_UNSUCCESSFUL);
    }
    const auto size = m_req.Descriptor.InputSize - readOffset;
    std::vector<uint8_t> buf(size);
    if (size > 0)
    {
        readInput(static_cast<ULONG>(readOffset), buf.data(), static_cast<ULONG>(size));
    }
    return buf;
}

// Writes data back to the client's output buffer for the current message.
// Analogous to the IOCTL_CONDRV_WRITE_OUTPUT call in the OG ReleaseMessageBuffers() (csrutil.cpp).
//
// The driver matches the Identifier to the pending IO and copies data into
// the client's buffer at the specified offset.
void Server::writeOutput(ULONG offset, const void* buffer, ULONG size)
{
    CD_IO_OPERATION op{};
    op.Identifier = m_req.Descriptor.Identifier;
    op.Buffer.Offset = offset;
    op.Buffer.Data = const_cast<void*>(buffer);
    op.Buffer.Size = size;
    THROW_IF_NTSTATUS_FAILED(ioctl(IOCTL_CONDRV_WRITE_OUTPUT, &op, sizeof(op), nullptr, 0));
}

// Completes a message with the given completion descriptor.
// Analogous to the OG ConsoleComplete() in csrutil.cpp.
//
// This sends the reply out-of-band (via IOCTL_CONDRV_COMPLETE_IO) rather than
// piggybacking on the next IOCTL_CONDRV_READ_IO call. Used when the reply
// carries write data (e.g. CD_CONNECTION_INFORMATION for CONNECT).
void Server::completeIo(CD_IO_COMPLETE& completion)
{
    THROW_IF_NTSTATUS_FAILED(ioctl(IOCTL_CONDRV_COMPLETE_IO, &completion, sizeof(completion), nullptr, 0));
}

// Validates a handle against expected type and access mask.
// Analogous to OG DereferenceIoHandle() in handle.cpp.
// Returns nullptr if not found, wrong type, or insufficient access.
Handle* Server::findHandle(ULONG_PTR obj, ULONG type, ACCESS_MASK access)
{
    auto ptr = reinterpret_cast<Handle*>(obj);
    for (auto& h : m_handles)
    {
        if (h.get() == ptr && (h->handleType & type) && (h->access & access) == access)
        {
            return ptr;
        }
    }
    return nullptr;
}

// Validates an output handle and temporarily activates the associated screen
// buffer on the host if it differs from the currently active one.
// All IPtyHost calls that operate on screen buffer content (GetScreenBufferInfo,
// SetScreenBufferInfo, ReadBuffer, WriteUTF8/16) operate on the active buffer,
// so we ensure the right one is activated before each API call.
//
// Returns nullptr if the handle is invalid. The caller must check for nullptr
// and return STATUS_INVALID_HANDLE.
Handle* Server::activateOutputBuffer(ACCESS_MASK requiredAccess)
{
    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, requiredAccess);
    if (!h)
        return nullptr;

    // If the handle's buffer differs from the active one, ask the host to switch.
    if (h->screenBuffer != m_activeScreenBuffer && m_host)
    {
        m_host->ActivateBuffer(h->screenBuffer);
        m_activeScreenBuffer = h->screenBuffer;
    }

    return h;
}

// VT output helpers.
// These accumulate VT sequences into m_vtBuf. Call vtFlush() to send them.

void Server::vtFlush()
{
    if (!m_vtBuf.empty() && m_host)
    {
        m_host->WriteUTF8({ m_vtBuf.data(), m_vtBuf.size() });
        m_vtBuf.clear();
    }
}

void Server::vtAppend(std::string_view sv)
{
    m_vtBuf.append(sv);
}

void Server::vtAppendFmt(const char* fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    const auto n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n > 0)
        m_vtBuf.append(buf, static_cast<size_t>(n));
}

void Server::vtAppendUTF16(std::wstring_view str)
{
    if (str.empty())
        return;
    const auto len = static_cast<int>(str.size());
    const auto utf8Len = WideCharToMultiByte(CP_UTF8, 0, str.data(), len, nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0)
        return;
    const auto offset = m_vtBuf.size();
    m_vtBuf.resize(offset + utf8Len);
    WideCharToMultiByte(CP_UTF8, 0, str.data(), len, m_vtBuf.data() + offset, utf8Len, nullptr, nullptr);
}

void Server::vtAppendTitle(std::wstring_view title)
{
    vtAppend("\x1b]0;");

    // Strip C0/C1 control characters to prevent OSC injection.
    std::wstring safe;
    safe.reserve(title.size());
    for (const auto ch : title)
    {
        if (ch >= 0x20 && ch != 0x7F)
            safe += ch;
    }
    vtAppendUTF16(safe);

    vtAppend("\x1b\\");
}

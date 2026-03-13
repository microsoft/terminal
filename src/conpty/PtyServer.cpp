#include "pch.h"
#include "PtyServer.h"

#include <cassert>

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
        *server = static_cast<IPtyServer*>(new PtyServer());
        return S_OK;
    }

    return E_NOINTERFACE;
}
CATCH_RETURN()

PtyServer::PtyServer()
{
    m_server = createHandle(nullptr, L"\\Device\\ConDrv\\Server", false, false);
    m_inputAvailableEvent.create(wil::EventOptions::ManualReset);

    CD_IO_SERVER_INFORMATION info{
        .InputAvailableEvent = m_inputAvailableEvent.get(),
    };
    THROW_IF_FAILED(ioctl(IOCTL_CONDRV_SET_SERVER_INFORMATION, &info, sizeof(CD_IO_SERVER_INFORMATION), nullptr, 0));
}

#pragma region IUnknown

HRESULT PtyServer::QueryInterface(const IID& riid, void** ppvObject)
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

ULONG PtyServer::AddRef()
{
    return m_refCount.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG PtyServer::Release()
{
    const auto count = m_refCount.fetch_sub(1, std::memory_order_relaxed) - 1;
    if (count == 0)
    {
        delete this;
    }
    return count;
}

#pragma endregion

#pragma region IPtyServer

HRESULT PtyServer::Run()
{
    CONSOLE_API_MSG req{};
    CD_IO_COMPLETE res{};
    bool hasRes = false;

    while (true)
    {
        NTSTATUS status;
        {
            void* in = nullptr;
            DWORD inLen = 0;

            if (hasRes)
            {
                res.Identifier = req.Descriptor.Identifier;
                in = &res;
                inLen = sizeof(res);
            }

            status = ioctl(IOCTL_CONDRV_READ_IO, in, inLen, &req, sizeof(req));

            if (hasRes)
            {
                memset(&res, 0, sizeof(res));
                hasRes = false;
            }
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
            switch (req.Descriptor.Function)
            {
            case CONSOLE_IO_CONNECT:
            {
                printf("Received connect request from process %llu\n", req.Descriptor.Process);
                handleConnect(req);
                break;
            }
            case CONSOLE_IO_DISCONNECT:
                printf("Received disconnect request from process %llu\n", req.Descriptor.Process);
                handleDisconnect(req);
                res.IoStatus.Status = STATUS_SUCCESS;
                hasRes = true;
                break;
            case CONSOLE_IO_CREATE_OBJECT:
                printf("Received create object request for object %llu from process %llu\n", req.Descriptor.Object, req.Descriptor.Process);
                handleCreateObject(req);
                break;
            case CONSOLE_IO_CLOSE_OBJECT:
                printf("Received close object request for object %llu from process %llu\n", req.Descriptor.Object, req.Descriptor.Process);
                handleCloseObject(req);
                res.IoStatus.Status = STATUS_SUCCESS;
                hasRes = true;
                break;
            case CONSOLE_IO_RAW_WRITE:
                printf("Received raw write request of %lu bytes from process %llu\n", req.Descriptor.InputSize, req.Descriptor.Process);
                if (handleRawWrite(req))
                {
                    res.IoStatus.Status = STATUS_SUCCESS;
                    hasRes = true;
                }
                break;
            case CONSOLE_IO_RAW_READ:
                printf("Received raw read request of %lu bytes from process %llu\n", req.Descriptor.OutputSize, req.Descriptor.Process);
                if (handleRawRead(req))
                {
                    res.IoStatus.Status = STATUS_SUCCESS;
                    hasRes = true;
                }
                break;
            case CONSOLE_IO_USER_DEFINED:
                printf("Received user defined IO request: %lu\n", req.Descriptor.InputSize);
                res.IoStatus.Status = STATUS_NOT_IMPLEMENTED;
                hasRes = true;
                break;
            case CONSOLE_IO_RAW_FLUSH:
                printf("Received raw flush request from process %llu\n", req.Descriptor.Process);
                res.IoStatus.Status = STATUS_NOT_IMPLEMENTED;
                hasRes = true;
                break;
            default:
                res.IoStatus.Status = STATUS_UNSUCCESSFUL;
                hasRes = true;
                break;
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            res.IoStatus.Status = status;
            hasRes = true;
        }
    }
}

HRESULT PtyServer::CreateProcessW(
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

unique_nthandle PtyServer::createHandle(HANDLE parent, const wchar_t* typeName, bool inherit, bool synchronous)
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

NTSTATUS PtyServer::ioctl(DWORD code, void* in, DWORD inLen, void* out, DWORD outLen) const
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

#pragma endregion

#define WIN32_LEAN_AND_MEAN
#define UMDF_USING_NTSTATUS
#define NOMINMAX
#include <ntstatus.h>
#include <Windows.h>
#include <winioctl.h>
#include <winternl.h>

// NOTE: These headers depend on Windows/winternl being included first.
#include <array>
#include <condrv.h>
#include <conmsgl1.h>
#include <conmsgl2.h>
#include <conmsgl3.h>

#include <atomic>
#include <wil/win32_helpers.h>

#include "conpty.h"

#define ProcThreadAttributeConsoleReference 10

#define PROC_THREAD_ATTRIBUTE_CONSOLE_REFERENCE \
    ProcThreadAttributeValue(10, FALSE, TRUE, FALSE)

#pragma warning(disable : 4100 4189)

using unique_nthandle = wil::unique_any_handle_null<decltype(&::NtClose), ::NtClose>;

struct CONSOLE_API_MSG
{
    CD_IO_DESCRIPTOR Descriptor;
    union
    {
        struct
        {
            CD_CREATE_OBJECT_INFORMATION CreateObject;
            CONSOLE_CREATESCREENBUFFER_MSG CreateScreenBuffer;
        };
        struct
        {
            CONSOLE_MSG_HEADER msgHeader;
            union
            {
                CONSOLE_MSG_BODY_L1 consoleMsgL1;
                CONSOLE_MSG_BODY_L2 consoleMsgL2;
                CONSOLE_MSG_BODY_L3 consoleMsgL3;
            } u;
        };
    };
};

struct PtyServer : IPtyServer
{
    PtyServer()
    {
        m_server = createHandle(nullptr, L"\\Device\\ConDrv\\Server", false, false);
        m_inputAvailableEvent.create(wil::EventOptions::ManualReset);

        CD_IO_SERVER_INFORMATION info{
            .InputAvailableEvent = m_inputAvailableEvent.get(),
        };
        THROW_IF_FAILED(ioctl(IOCTL_CONDRV_SET_SERVER_INFORMATION, &info, sizeof(CD_IO_SERVER_INFORMATION), nullptr, 0));
    }

    virtual ~PtyServer() = default;

#pragma region IUnknown

    HRESULT QueryInterface(const IID& riid, void** ppvObject) override
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

    ULONG AddRef() override
    {
        return m_refCount.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    ULONG Release() override
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

    HRESULT Run() override
    {
        CONSOLE_API_MSG req{};
        CD_IO_COMPLETE resBuf{};
        CD_IO_COMPLETE* res = nullptr;

        while (true)
        {
            auto hr = ioctl(IOCTL_CONDRV_READ_IO, res, res ? sizeof(res) : 0, &req, sizeof(CONSOLE_API_MSG));

            if (res)
            {
                memset(res, 0, sizeof(resBuf));
                res = nullptr;
            }

            if (FAILED_NTSTATUS_LOG(hr))
            {
                if (hr == STATUS_PIPE_DISCONNECTED)
                {
                    hr = S_OK;
                }
                return hr;
            }

            switch (req.Descriptor.Function)
            {
            case CONSOLE_IO_CONNECT:
                printf("Received connect request from process %llu\n", req.Descriptor.Process);
                break;
            case CONSOLE_IO_DISCONNECT:
                printf("Received disconnect request from process %llu\n", req.Descriptor.Process);
                break;
            case CONSOLE_IO_CREATE_OBJECT:
                printf("Received create object request for object %llu from process %llu\n", req.Descriptor.Object, req.Descriptor.Process);
                break;
            case CONSOLE_IO_CLOSE_OBJECT:
                printf("Received close object request for object %llu from process %llu\n", req.Descriptor.Object, req.Descriptor.Process);
                break;
            case CONSOLE_IO_RAW_WRITE:
                printf("Received raw write request of %lu bytes from process %llu\n", req.Descriptor.InputSize, req.Descriptor.Process);
                break;
            case CONSOLE_IO_RAW_READ:
                printf("Received raw read request of %lu bytes from process %llu\n", req.Descriptor.OutputSize, req.Descriptor.Process);
                break;
            case CONSOLE_IO_USER_DEFINED:
                printf("Received user defined IO request: %lu\n", req.Descriptor.InputSize);
                break;
            case CONSOLE_IO_RAW_FLUSH:
                printf("Received raw flush request from process %llu\n", req.Descriptor.Process);
                break;
            default:
                resBuf.IoStatus.Status = STATUS_UNSUCCESSFUL;
                res = &resBuf;
                break;
            }
        }
    }

    HRESULT CreateProcessW(
        /* [in] */ LPCWSTR lpApplicationName,
        /* [out][in] */ LPWSTR lpCommandLine,
        /* [in] */ LPSECURITY_ATTRIBUTES lpProcessAttributes,
        /* [in] */ LPSECURITY_ATTRIBUTES lpThreadAttributes,
        /* [in] */ BOOL bInheritHandles,
        /* [in] */ DWORD dwCreationFlags,
        /* [in] */ LPVOID lpEnvironment,
        /* [in] */ LPCWSTR lpCurrentDirectory,
        /* [out] */ LPPROCESS_INFORMATION lpProcessInformation) override
    try
    {
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
        handles[3] = dup(handles[2].get());

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

    void test()
    {
    }

private:
    static unique_nthandle createHandle(HANDLE parent, const wchar_t* typeName, bool inherit, bool synchronous)
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

    static unique_nthandle dup(HANDLE i)
    {
        HANDLE o;
        const auto proc = GetCurrentProcess();
        THROW_IF_WIN32_BOOL_FALSE(DuplicateHandle(proc, i, proc, &o, 0, TRUE, DUPLICATE_SAME_ACCESS));
        return unique_nthandle{ o };
    }

    NTSTATUS ioctl(DWORD code, void* in, DWORD inLen, void* out, DWORD outLen) const
    {
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

        if (NT_SUCCESS(status) && outLen != static_cast<DWORD>(iosb.Information))
        {
            // Short read? Error.
            status = STATUS_UNSUCCESSFUL;
        }

        return status;
    }
    std::atomic<ULONG> m_refCount{ 1 };
    unique_nthandle m_server;
    wil::unique_event m_inputAvailableEvent;
};

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

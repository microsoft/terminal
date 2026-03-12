#include <Windows.h>
#include <winternl.h>

// NOTE: These headers depend on Windows/winternl being included first.
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
        m_server = createHandle(nullptr, L"\\Device\\ConDrv\\Server", true, false);
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
        CONSOLE_API_MSG req;
        CD_IO_COMPLETE res;

        while (true)
        {
            const auto hr = ioctl(IOCTL_CONDRV_READ_IO, &res, sizeof(res), &req, sizeof(CONSOLE_API_MSG));
            if (FAILED(hr))
            {
                if (hr == HRESULT_FROM_WIN32(ERROR_IO_PENDING))
                {
                    WaitForSingleObjectEx(m_server.get(), 0, FALSE);
                }
                else if (hr == HRESULT_FROM_WIN32(ERROR_PIPE_NOT_CONNECTED))
                {
                    return S_OK;
                }
                else
                {
                    return hr;
                }
            }
        }
    }

    HRESULT FillStartupInfoEx(LPSTARTUPINFOEXW si, HANDLE* reference) override
    try
    {
        fillStartupInfoExImpl(si, reference);
        return S_OK;
    }
    CATCH_RETURN()

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
        HANDLE reference = nullptr;
        uint64_t attrListBuffer[8];
        STARTUPINFOEX si{};

        si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
        si.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(&attrListBuffer[0]);

        auto listSize = sizeof(attrListBuffer);
        THROW_IF_WIN32_BOOL_FALSE(InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &listSize));

        const auto cleanup = wil::scope_exit([&] {
            DeleteProcThreadAttributeList(si.lpAttributeList);

            if (reference)
            {
                NtClose(reference);
            }

            const auto handles = &si.StartupInfo.hStdInput;
            for (int i = 0; i < 3; i++)
            {
                if (handles[i])
                {
                    NtClose(handles[i]);
                }
            }
        });

        fillStartupInfoExImpl(&si, &reference);

        return ::CreateProcessW(
            lpApplicationName,
            lpCommandLine,
            lpProcessAttributes,
            lpThreadAttributes,
            bInheritHandles,
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
        const auto proc = GetCurrentProcess();
        HANDLE o;
        THROW_IF_WIN32_BOOL_FALSE(DuplicateHandle(proc, i, proc, &o, 0, TRUE, DUPLICATE_SAME_ACCESS));
        return unique_nthandle{ o };
    }

    HRESULT ioctl(DWORD code, void* in, DWORD inLen, void* out, DWORD outCap) const
    {
        DWORD outLen = 0;
        return DeviceIoControl(m_server.get(), code, in, inLen, out, outCap, &outLen, nullptr);
    }

    void fillStartupInfoExImpl(LPSTARTUPINFOEXW si, HANDLE* reference) const
    {
        auto ref = createHandle(m_server.get(), L"\\Reference", false, true);
        auto inp = createHandle(m_server.get(), L"\\Input", true, true);
        auto out = createHandle(m_server.get(), L"\\Output", true, true);
        auto err = dup(si->StartupInfo.hStdOutput);

        THROW_IF_WIN32_BOOL_FALSE(UpdateProcThreadAttribute(si->lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_CONSOLE_REFERENCE, ref.addressof(), sizeof(HANDLE), nullptr, nullptr));

        si->StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
        si->StartupInfo.hStdInput = inp.release();
        si->StartupInfo.hStdOutput = out.release();
        si->StartupInfo.hStdError = err.release();
        *reference = ref.release();
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

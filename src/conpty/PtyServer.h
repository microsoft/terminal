#pragma once

#include <conpty.h>

#include <memory>
#include <vector>

using unique_nthandle = wil::unique_any_handle_null<decltype(&::NtClose), ::NtClose>;

// Mirrors the payload of IOCTL_CONDRV_READ_IO.
// Unlike the OG CONSOLE_API_MSG (which has Complete/State/IoStatus before Descriptor),
// this struct starts at Descriptor because that's where the driver output begins.
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

// Per-client tracking data, analogous to the OG CONSOLE_PROCESS_HANDLE.
struct PtyClient
{
    DWORD processId = 0;
    ULONG processGroupId = 0;
    bool rootProcess = false;
    ULONG_PTR inputHandle = 0;
    ULONG_PTR outputHandle = 0;
};

struct PtyServer : IPtyServer
{
    PtyServer();

    virtual ~PtyServer() = default;

#pragma region IUnknown

    HRESULT QueryInterface(const IID& riid, void** ppvObject) override;
    ULONG AddRef() override;
    ULONG Release() override;

#pragma endregion

#pragma region IPtyServer

    HRESULT Run() override;
    HRESULT CreateProcessW(
        LPCWSTR lpApplicationName,
        LPWSTR lpCommandLine,
        LPSECURITY_ATTRIBUTES lpProcessAttributes,
        LPSECURITY_ATTRIBUTES lpThreadAttributes,
        BOOL bInheritHandles,
        DWORD dwCreationFlags,
        LPVOID lpEnvironment,
        LPCWSTR lpCurrentDirectory,
        LPPROCESS_INFORMATION lpProcessInformation) override;

#pragma endregion

private:
    static unique_nthandle createHandle(HANDLE parent, const wchar_t* typeName, bool inherit, bool synchronous);
    NTSTATUS ioctl(DWORD code, void* in, DWORD inLen, void* out, DWORD outLen) const;

    // ConDrv communication helpers.
    void readInput(const CD_IO_DESCRIPTOR& desc, ULONG offset, void* buffer, ULONG size);
    void completeIo(CD_IO_COMPLETE& completion);

    // Message handlers (implemented in PtyServer.clients.cpp).
    void handleConnect(CONSOLE_API_MSG& msg);
    void handleDisconnect(CONSOLE_API_MSG& msg);

    // Client lookup by opaque handle value (the raw PtyClient pointer cast to ULONG_PTR).
    PtyClient* findClient(ULONG_PTR handle);

    std::atomic<ULONG> m_refCount{ 1 };
    unique_nthandle m_server;
    wil::unique_event m_inputAvailableEvent;

    bool m_initialized = false;
    ULONG_PTR m_nextHandleId = 1;
    std::vector<std::unique_ptr<PtyClient>> m_clients;
};

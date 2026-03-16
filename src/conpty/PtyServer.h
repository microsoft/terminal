#pragma once

#include <conpty.h>

#include <deque>
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

// Handle type flags, from the OG server.h.
// These are internal to the console server, not part of the condrv protocol.
#define CONSOLE_INPUT_HANDLE           0x00000001
#define CONSOLE_OUTPUT_HANDLE          0x00000002

// Handle tracking data, analogous to the OG CONSOLE_HANDLE_DATA.
// In the OG, handles are raw pointers to CONSOLE_HANDLE_DATA which contain
// share mode/access tracking and a pointer to the underlying object
// (INPUT_INFORMATION or SCREEN_INFORMATION). We simplify this for now.
struct PtyHandle
{
    ULONG handleType = 0; // CONSOLE_INPUT_HANDLE or CONSOLE_OUTPUT_HANDLE
    ACCESS_MASK access = 0;
    ULONG shareMode = 0;
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

// A pending IO request that couldn't be completed immediately.
// For writes: the output is paused (e.g. the user hit Pause).
// For reads: the input queue is empty; we'll complete it when data arrives.
struct PendingIO
{
    LUID identifier{};       // ConDrv message identifier, needed for completeIo/writeOutput.
    ULONG_PTR process = 0;   // Descriptor.Process, for cleanup on disconnect.
    ULONG_PTR object = 0;    // Descriptor.Object, the handle this IO targets.
    ULONG function = 0;      // CONSOLE_IO_RAW_READ or CONSOLE_IO_RAW_WRITE.
    ULONG outputSize = 0;    // For reads: max bytes the client accepts.
    std::vector<uint8_t> inputData; // For writes: the data the client sent.
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
    void writeOutput(const CD_IO_DESCRIPTOR& desc, ULONG offset, const void* buffer, ULONG size);
    void completeIo(CD_IO_COMPLETE& completion);

    // Message handlers (implemented in PtyServer.clients.cpp).
    // Handlers returning bool: true = reply pending (don't reply inline), false = reply inline.
    void handleConnect(CONSOLE_API_MSG& msg);
    void handleDisconnect(CONSOLE_API_MSG& msg);
    void handleCreateObject(CONSOLE_API_MSG& msg);
    void handleCloseObject(CONSOLE_API_MSG& msg);
    bool handleRawWrite(CONSOLE_API_MSG& msg);
    bool handleRawRead(CONSOLE_API_MSG& msg);
    void handleUserDefined(CONSOLE_API_MSG& msg);
    void handleRawFlush(CONSOLE_API_MSG& msg);

    // Complete pending IOs (called when state changes make progress possible).
    void completePendingRead(const void* data, ULONG size);
    void completePendingWrites();
    void cancelPendingIOs(ULONG_PTR process);

    // Client lookup by opaque handle value (the raw PtyClient pointer cast to ULONG_PTR).
    PtyClient* findClient(ULONG_PTR handle);

    // Handle management.
    ULONG_PTR allocateHandle(ULONG handleType, ACCESS_MASK access, ULONG shareMode);
    void freeHandle(ULONG_PTR handle);

    std::atomic<ULONG> m_refCount{ 1 };
    unique_nthandle m_server;
    wil::unique_event m_inputAvailableEvent;

    bool m_initialized = false;
    bool m_outputPaused = false;
    std::vector<std::unique_ptr<PtyClient>> m_clients;
    std::vector<std::unique_ptr<PtyHandle>> m_handles;
    std::deque<PendingIO> m_pendingReads;
    std::deque<PendingIO> m_pendingWrites;
};

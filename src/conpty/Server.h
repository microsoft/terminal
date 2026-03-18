#pragma once

#include <conpty.h>
#include "InputBuffer.h"

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

struct Server;

// Handle tracking data, analogous to the OG CONSOLE_HANDLE_DATA.
// In the OG, handles are raw pointers to CONSOLE_HANDLE_DATA which contain
// share mode/access tracking and a pointer to the underlying object
// (INPUT_INFORMATION or SCREEN_INFORMATION). We simplify this for now.
struct Handle
{
    ULONG handleType = 0; // CONSOLE_INPUT_HANDLE or CONSOLE_OUTPUT_HANDLE
    ACCESS_MASK access = 0;
    ULONG shareMode = 0;
};

// Per-client tracking data, analogous to the OG CONSOLE_PROCESS_HANDLE.
struct Client
{
    DWORD processId = 0;
    ULONG processGroupId = 0;
    bool rootProcess = false;
    ULONG_PTR inputHandle = 0;
    ULONG_PTR outputHandle = 0;
};

// A pending IO request that couldn't be completed immediately.
//
// Analogous to the OG CONSOLE_WAIT_BLOCK. When new input arrives (or output
// is unpaused), the server walks its pending-IO queues and calls `retry()`.
// If retry() returns true, the IO was satisfied and the block is removed.
// If false, it stays queued for a future attempt.
//
// retry() is responsible for calling writeOutput() and completeIo() itself.
struct PendingIO
{
    LUID identifier{};       // ConDrv message identifier, for completeIo/writeOutput.
    ULONG_PTR process = 0;   // Descriptor.Process, for cleanup on disconnect.

    // Retry callback. Called when conditions change (new input, output unpaused).
    // Returns true if the IO was completed, false if it should stay queued.
    // Signature: bool retry(Server& server)
    std::function<bool(Server&)> retry;
};

struct Server : IPtyServer
{
    Server();

    virtual ~Server() = default;

#pragma region IUnknown

    HRESULT QueryInterface(const IID& riid, void** ppvObject) override;
    ULONG AddRef() override;
    ULONG Release() override;

#pragma endregion

#pragma region IPtyServer

    HRESULT SetHost(IPtyHost* host) override;

    HRESULT WriteUTF8(PTY_UTF8_STRING input) override;
    HRESULT WriteUTF16(PTY_UTF16_STRING input) override;

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

    // Positive NTSTATUS sentinel: "no piggyback reply for this iteration."
    // NTSTATUS is a signed LONG, so status <= 0 catches both success (0) and
    // errors (negative), while this positive value skips the piggyback path.
    static constexpr NTSTATUS STATUS_NO_RESPONSE = 1;

    static unique_nthandle createHandle(HANDLE parent, const wchar_t* typeName, bool inherit, bool synchronous);
    NTSTATUS ioctl(DWORD code, void* in, DWORD inLen, void* out, DWORD outLen) const;

    // ConDrv communication helpers.
    // These operate on m_req (the current message being processed).
    void readInput(ULONG offset, void* buffer, ULONG size);
    std::vector<uint8_t> readTrailingInput();
    void writeOutput(ULONG offset, const void* buffer, ULONG size);
    void completeIo(CD_IO_COMPLETE& completion);

    // Handle validation. Returns nullptr if the handle doesn't exist,
    // doesn't match the expected type, or lacks the required access.
    Handle* findHandle(ULONG_PTR obj, ULONG type, ACCESS_MASK access);

    // Message handlers.
    // All handlers read from m_req and return NTSTATUS:
    //  - STATUS_SUCCESS / error → piggyback reply on next READ_IO
    //  - STATUS_NO_RESPONSE     → handler already replied (completeIo) or deferred
    NTSTATUS handleConnect();
    NTSTATUS handleDisconnect();
    NTSTATUS handleCreateObject();
    NTSTATUS handleCloseObject();
    NTSTATUS handleRawWrite();
    NTSTATUS handleRawRead();
    NTSTATUS handleUserDefined();
    NTSTATUS handleRawFlush();

    NTSTATUS handleUserDeprecatedApi();

    NTSTATUS handleUserL1GetConsoleCP();
    NTSTATUS handleUserL1GetConsoleMode();
    NTSTATUS handleUserL1SetConsoleMode();
    NTSTATUS handleUserL1GetNumberOfConsoleInputEvents();
    NTSTATUS handleUserL1GetConsoleInput();
    NTSTATUS handleUserL1ReadConsole();
    NTSTATUS handleUserL1WriteConsole();
    NTSTATUS handleUserL1GetConsoleLangId();

    NTSTATUS handleUserL2FillConsoleOutput();
    NTSTATUS handleUserL2GenerateConsoleCtrlEvent();
    NTSTATUS handleUserL2SetConsoleActiveScreenBuffer();
    NTSTATUS handleUserL2FlushConsoleInputBuffer();
    NTSTATUS handleUserL2SetConsoleCP();
    NTSTATUS handleUserL2GetConsoleCursorInfo();
    NTSTATUS handleUserL2SetConsoleCursorInfo();
    NTSTATUS handleUserL2GetConsoleScreenBufferInfo();
    NTSTATUS handleUserL2SetConsoleScreenBufferInfo();
    NTSTATUS handleUserL2SetConsoleScreenBufferSize();
    NTSTATUS handleUserL2SetConsoleCursorPosition();
    NTSTATUS handleUserL2GetLargestConsoleWindowSize();
    NTSTATUS handleUserL2ScrollConsoleScreenBuffer();
    NTSTATUS handleUserL2SetConsoleTextAttribute();
    NTSTATUS handleUserL2SetConsoleWindowInfo();
    NTSTATUS handleUserL2ReadConsoleOutputString();
    NTSTATUS handleUserL2WriteConsoleInput();
    NTSTATUS handleUserL2WriteConsoleOutput();
    NTSTATUS handleUserL2WriteConsoleOutputString();
    NTSTATUS handleUserL2ReadConsoleOutput();
    NTSTATUS handleUserL2GetConsoleTitle();
    NTSTATUS handleUserL2SetConsoleTitle();

    NTSTATUS handleUserL3GetConsoleMouseInfo();
    NTSTATUS handleUserL3GetConsoleFontSize();
    NTSTATUS handleUserL3GetConsoleCurrentFont();
    NTSTATUS handleUserL3SetConsoleDisplayMode();
    NTSTATUS handleUserL3GetConsoleDisplayMode();
    NTSTATUS handleUserL3AddConsoleAlias();
    NTSTATUS handleUserL3GetConsoleAlias();
    NTSTATUS handleUserL3GetConsoleAliasesLength();
    NTSTATUS handleUserL3GetConsoleAliasExesLength();
    NTSTATUS handleUserL3GetConsoleAliases();
    NTSTATUS handleUserL3GetConsoleAliasExes();
    NTSTATUS handleUserL3ExpungeConsoleCommandHistory();
    NTSTATUS handleUserL3SetConsoleNumberOfCommands();
    NTSTATUS handleUserL3GetConsoleCommandHistoryLength();
    NTSTATUS handleUserL3GetConsoleCommandHistory();
    NTSTATUS handleUserL3GetConsoleWindow();
    NTSTATUS handleUserL3GetConsoleSelectionInfo();
    NTSTATUS handleUserL3GetConsoleProcessList();
    NTSTATUS handleUserL3GetConsoleHistory();
    NTSTATUS handleUserL3SetConsoleHistory();
    NTSTATUS handleUserL3SetConsoleCurrentFont();

    // Complete pending IOs (called when state changes make progress possible).
    void drainPendingInputReads();
    void drainPendingOutputWrites();
    void cancelPendingIOs(ULONG_PTR process);

    // Helpers to pend a read or write IO. Both return STATUS_NO_RESPONSE.
    NTSTATUS pendRead(std::function<bool(Server&)> retry);
    NTSTATUS pendWrite(std::function<bool(Server&)> retry);

    // Helper to complete a single pending IO with status + information.
    void completePendingIo(const LUID& identifier, NTSTATUS status, ULONG_PTR information = 0);

    // Client lookup by opaque handle value (the raw Client pointer cast to ULONG_PTR).
    Client* findClient(ULONG_PTR handle);

    // Handle management.
    ULONG_PTR allocateHandle(ULONG handleType, ACCESS_MASK access, ULONG shareMode);
    void freeHandle(ULONG_PTR handle);

    // VT output helpers.
    void vtFlush();
    void vtAppend(std::string_view sv);
    void vtAppendFmt(_Printf_format_string_ const char* fmt, ...);
    void vtAppendUTF16(std::wstring_view str);
    void vtAppendCUP(SHORT row, SHORT col);
    void vtAppendSGR(WORD attr);
    void vtAppendTitle(std::wstring_view title);

    std::atomic<ULONG> m_refCount{ 1 };
    unique_nthandle m_server;
    wil::com_ptr<IPtyHost> m_host;
    wil::unique_event m_inputAvailableEvent;

    // Per-message state. Set by Run() before dispatching, read by handlers.
    CONSOLE_API_MSG m_req{};
    // Piggyback response. m_resData points to either m_req.u (zero-copy, set by
    // handleUserDefined for most APIs) or m_resBuffer.data() (for bulk responses).
    std::span<const uint8_t> m_resData;
    std::vector<uint8_t> m_resBuffer;

    bool m_initialized = false;
    bool m_outputPaused = false;
    std::vector<std::unique_ptr<Client>> m_clients;
    std::vector<std::unique_ptr<Handle>> m_handles;
    std::deque<PendingIO> m_pendingReads;
    std::deque<PendingIO> m_pendingWrites;

    // Console state — code pages.
    UINT m_inputCP = CP_UTF8;
    UINT m_outputCP = CP_UTF8;

    // Console state — mode flags (global; per-handle tracking deferred).
    DWORD m_inputMode = ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT;
    DWORD m_outputMode = ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT;

    // Console state — screen buffer.
    COORD m_bufferSize{ 120, 30 };
    COORD m_viewSize{ 120, 30 };
    COORD m_cursorPosition{ 0, 0 };
    WORD m_attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    WORD m_popupAttributes = 0;
    COLORREF m_colorTable[16]{};
    DWORD m_cursorSize = 25;
    bool m_cursorVisible = true;

    // Console state — title.
    std::wstring m_title;
    std::wstring m_originalTitle;

    // VT output accumulation buffer.
    std::string m_vtBuf;

    // Input buffer with integrated VT parser.
    InputBuffer m_input;
};

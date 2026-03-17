#include "pch.h"
#include "PtyServer.h"

// L3: GetConsoleMouseInfo (GetNumberOfConsoleMouseButtons)
// OG: SrvGetConsoleMouseInfo in getset.cpp — no handle validation.
// Returns a->NumButtons.
NTSTATUS PtyServer::handleUserL3GetConsoleMouseInfo()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleMouseInfo;

    // TODO: Return actual mouse button count.
    a.NumButtons = GetSystemMetrics(SM_CMOUSEBUTTONS);
    return STATUS_SUCCESS;
}

// L3: GetConsoleFontSize
// OG: SrvGetConsoleFontSize in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_READ)
// Reads a->FontIndex. Returns a->FontSize.
NTSTATUS PtyServer::handleUserL3GetConsoleFontSize()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleFontSize;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_READ);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Return font size for a->FontIndex from screen buffer.
    (void)a.FontIndex;
    a.FontSize = { 8, 16 };
    return STATUS_SUCCESS;
}

// L3: GetConsoleCurrentFont (GetCurrentConsoleFontEx)
// OG: SrvGetConsoleCurrentFont in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_READ)
// Reads a->MaximumWindow.
// Returns a->FontIndex, a->FontSize, a->FontFamily, a->FontWeight, a->FaceName.
NTSTATUS PtyServer::handleUserL3GetConsoleCurrentFont()
{
    auto& a = m_req.u.consoleMsgL3.GetCurrentConsoleFont;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_READ);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Return actual font info from screen buffer state.
    a.FontIndex = 0;
    a.FontSize = { 8, 16 };
    a.FontFamily = FF_MODERN | FIXED_PITCH;
    a.FontWeight = FW_NORMAL;
    wcscpy_s(a.FaceName, L"Consolas");
    return STATUS_SUCCESS;
}

// L3: SetConsoleDisplayMode
// OG: SrvSetConsoleDisplayMode in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
// Reads a->dwFlags (CONSOLE_FULLSCREEN_MODE / CONSOLE_WINDOWED_MODE).
// Returns a->ScreenBufferDimensions.
NTSTATUS PtyServer::handleUserL3SetConsoleDisplayMode()
{
    auto& a = m_req.u.consoleMsgL3.SetConsoleDisplayMode;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Switch display mode. Return resulting buffer dimensions.
    (void)a.dwFlags;
    a.ScreenBufferDimensions = { 120, 30 };
    return STATUS_SUCCESS;
}

// L3: GetConsoleDisplayMode
// OG: SrvGetConsoleDisplayMode in getset.cpp — no handle validation.
// Returns a->ModeFlags.
NTSTATUS PtyServer::handleUserL3GetConsoleDisplayMode()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleDisplayMode;

    // TODO: Return actual display mode.
    a.ModeFlags = 0; // windowed
    return STATUS_SUCCESS;
}

// L3: AddConsoleAlias
// OG: SrvAddConsoleAlias in cmdline.cpp — no handle validation.
// Reads a->SourceLength, a->TargetLength, a->ExeLength, a->Unicode.
// Trailing input contains three packed strings: source, target, exe name.
NTSTATUS PtyServer::handleUserL3AddConsoleAlias()
{
    auto& a = m_req.u.consoleMsgL3.AddConsoleAliasW;

    if (a.SourceLength > USHRT_MAX || a.TargetLength > USHRT_MAX || a.ExeLength > USHRT_MAX)
        return STATUS_INVALID_PARAMETER;

    auto payload = readTrailingInput();

    // Validate payload contains all three strings.
    const auto totalLen = static_cast<size_t>(a.SourceLength) + a.TargetLength + a.ExeLength;
    if (payload.size() < totalLen)
        return STATUS_INVALID_PARAMETER;

    // TODO: Parse source, target, exe from payload and store alias.
    (void)a.Unicode;
    return STATUS_SUCCESS;
}

// L3: GetConsoleAlias
// OG: SrvGetConsoleAlias in cmdline.cpp — no handle validation.
// Reads a->SourceLength, a->ExeLength, a->Unicode.
// Trailing input = source + exe strings.
// Writes target string via writeOutput(). Returns a->TargetLength.
NTSTATUS PtyServer::handleUserL3GetConsoleAlias()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleAliasW;

    if (a.SourceLength > USHRT_MAX || a.ExeLength > USHRT_MAX)
        return STATUS_INVALID_PARAMETER;

    auto payload = readTrailingInput();

    const auto totalLen = static_cast<size_t>(a.SourceLength) + a.ExeLength;
    if (payload.size() < totalLen)
        return STATUS_INVALID_PARAMETER;

    // TODO: Look up alias for source+exe, write target via writeOutput().
    a.TargetLength = 0;
    return STATUS_SUCCESS;
}

// L3: GetConsoleAliasesLength
// OG: SrvGetConsoleAliasesLength in cmdline.cpp — no handle validation.
// Reads a->Unicode. Trailing input = exe name string.
// Returns a->AliasesLength (bytes needed for all aliases of that exe).
NTSTATUS PtyServer::handleUserL3GetConsoleAliasesLength()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleAliasesLengthW;

    auto payload = readTrailingInput();

    // TODO: Calculate total alias buffer size for exe in payload.
    (void)a.Unicode;
    a.AliasesLength = 0;
    return STATUS_SUCCESS;
}

// L3: GetConsoleAliasExesLength
// OG: SrvGetConsoleAliasExesLength in cmdline.cpp — no handle validation.
// Reads a->Unicode.
// Returns a->AliasExesLength.
NTSTATUS PtyServer::handleUserL3GetConsoleAliasExesLength()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleAliasExesLengthW;

    // TODO: Sum lengths of all exe names that have aliases.
    (void)a.Unicode;
    a.AliasExesLength = 0;
    return STATUS_SUCCESS;
}

// L3: GetConsoleAliases
// OG: SrvGetConsoleAliases in cmdline.cpp — no handle validation.
// Reads a->Unicode. Trailing input = exe name.
// Writes all aliases for that exe via writeOutput(). Returns a->AliasesBufferLength.
NTSTATUS PtyServer::handleUserL3GetConsoleAliases()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleAliasesW;

    auto payload = readTrailingInput();

    // TODO: Write "source=target\0..." pairs via writeOutput().
    (void)a.Unicode;
    a.AliasesBufferLength = 0;
    return STATUS_SUCCESS;
}

// L3: GetConsoleAliasExes
// OG: SrvGetConsoleAliasExes in cmdline.cpp — no handle validation.
// Reads a->Unicode.
// Writes exe name list via writeOutput(). Returns a->AliasExesBufferLength.
NTSTATUS PtyServer::handleUserL3GetConsoleAliasExes()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleAliasExesW;

    // TODO: Write null-separated exe names via writeOutput().
    (void)a.Unicode;
    a.AliasExesBufferLength = 0;
    return STATUS_SUCCESS;
}

// L3: ExpungeConsoleCommandHistory
// OG: SrvExpungeConsoleCommandHistory in cmdline.cpp — no handle validation.
// Reads a->Unicode. Trailing input = exe name.
NTSTATUS PtyServer::handleUserL3ExpungeConsoleCommandHistory()
{
    auto& a = m_req.u.consoleMsgL3.ExpungeConsoleCommandHistoryW;

    auto payload = readTrailingInput();

    // TODO: Clear command history for exe in payload.
    (void)a.Unicode;
    return STATUS_SUCCESS;
}

// L3: SetConsoleNumberOfCommands
// OG: SrvSetConsoleNumberOfCommands in cmdline.cpp — no handle validation.
// Reads a->NumCommands, a->Unicode. Trailing input = exe name.
NTSTATUS PtyServer::handleUserL3SetConsoleNumberOfCommands()
{
    auto& a = m_req.u.consoleMsgL3.SetConsoleNumberOfCommandsW;

    auto payload = readTrailingInput();

    // TODO: Resize command history for exe in payload to a->NumCommands.
    (void)a.NumCommands;
    (void)a.Unicode;
    return STATUS_SUCCESS;
}

// L3: GetConsoleCommandHistoryLength
// OG: SrvGetConsoleCommandHistoryLength in cmdline.cpp — no handle validation.
// Reads a->Unicode. Trailing input = exe name.
// Returns a->CommandHistoryLength.
NTSTATUS PtyServer::handleUserL3GetConsoleCommandHistoryLength()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleCommandHistoryLengthW;

    auto payload = readTrailingInput();

    // TODO: Calculate total command history buffer size for exe.
    (void)a.Unicode;
    a.CommandHistoryLength = 0;
    return STATUS_SUCCESS;
}

// L3: GetConsoleCommandHistory
// OG: SrvGetConsoleCommandHistory in cmdline.cpp — no handle validation.
// Reads a->CommandBufferLength, a->Unicode. Trailing input = exe name.
// Writes command strings via writeOutput(). Returns a->CommandBufferLength (actual).
NTSTATUS PtyServer::handleUserL3GetConsoleCommandHistory()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleCommandHistoryW;

    auto payload = readTrailingInput();

    // TODO: Write null-separated command history via writeOutput().
    (void)a.Unicode;
    a.CommandBufferLength = 0;
    return STATUS_SUCCESS;
}

// L3: GetConsoleWindow
// OG: SrvGetConsoleWindow in getset.cpp — no handle validation.
// Returns a->hwnd.
NTSTATUS PtyServer::handleUserL3GetConsoleWindow()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleWindow;

    // TODO: Return actual console window handle.
    a.hwnd = nullptr;
    return STATUS_SUCCESS;
}

// L3: GetConsoleSelectionInfo
// OG: SrvGetConsoleSelectionInfo in getset.cpp — no handle validation.
// Returns a->SelectionInfo (dwFlags, dwSelectionAnchor, srSelection).
NTSTATUS PtyServer::handleUserL3GetConsoleSelectionInfo()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleSelectionInfo;

    // TODO: Return actual selection state.
    a.SelectionInfo = {};
    return STATUS_SUCCESS;
}

// L3: GetConsoleProcessList
// OG: SrvGetConsoleProcessList in getset.cpp — no handle validation.
// Reads a->dwProcessCount (buffer capacity in DWORDs).
// Writes DWORD array of PIDs via writeOutput().
// Returns a->dwProcessCount (actual count).
NTSTATUS PtyServer::handleUserL3GetConsoleProcessList()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleProcessList;

    const auto capacity = a.dwProcessCount;

    // Build PID list from connected clients.
    std::vector<DWORD> pids;
    pids.reserve(m_clients.size());
    for (auto& c : m_clients)
    {
        pids.push_back(c->processId);
    }

    a.dwProcessCount = static_cast<DWORD>(pids.size());

    // Only write the PIDs if the client's buffer is large enough.
    if (capacity >= pids.size() && !pids.empty())
    {
        const auto writeOffset = m_req.msgHeader.ApiDescriptorSize;
        writeOutput(writeOffset, pids.data(), static_cast<ULONG>(pids.size() * sizeof(DWORD)));
    }

    return STATUS_SUCCESS;
}

// L3: GetConsoleHistoryInfo
// OG: SrvGetConsoleHistoryInfo in getset.cpp — no handle validation.
// Returns a->HistoryBufferSize, a->NumberOfHistoryBuffers, a->dwFlags.
NTSTATUS PtyServer::handleUserL3GetConsoleHistory()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleHistory;

    // TODO: Return actual history settings.
    a.HistoryBufferSize = 50;
    a.NumberOfHistoryBuffers = 4;
    a.dwFlags = 0;
    return STATUS_SUCCESS;
}

// L3: SetConsoleHistoryInfo
// OG: SrvSetConsoleHistoryInfo in getset.cpp — no handle validation.
// Reads a->HistoryBufferSize, a->NumberOfHistoryBuffers, a->dwFlags.
NTSTATUS PtyServer::handleUserL3SetConsoleHistory()
{
    auto& a = m_req.u.consoleMsgL3.SetConsoleHistory;

    // TODO: Apply history settings from a->*.
    (void)a.HistoryBufferSize;
    (void)a.NumberOfHistoryBuffers;
    (void)a.dwFlags;
    return STATUS_SUCCESS;
}

// L3: SetCurrentConsoleFontEx
// OG: SrvSetConsoleCurrentFont in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
// Reads a->FaceName, a->FontFamily, a->FontWeight, a->FontSize.
NTSTATUS PtyServer::handleUserL3SetConsoleCurrentFont()
{
    auto& a = m_req.u.consoleMsgL3.SetCurrentConsoleFont;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Apply font settings from a->FaceName, a->FontFamily, a->FontWeight, a->FontSize.
    (void)a;
    return STATUS_SUCCESS;
}

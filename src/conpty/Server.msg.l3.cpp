#include "pch.h"
#include "Server.h"

#define HOST_CALL(expr) \
    do { \
        const auto _hr = (expr); \
        if (FAILED(_hr)) return STATUS_UNSUCCESSFUL; \
    } while (0)

// Helper: extract a wide string from a byte payload at the given offset.
static std::wstring extractWideString(const std::vector<uint8_t>& payload, size_t offset, USHORT byteLen)
{
    if (offset + byteLen > payload.size() || byteLen % sizeof(WCHAR) != 0)
        return {};
    return { reinterpret_cast<const wchar_t*>(payload.data() + offset), byteLen / sizeof(WCHAR) };
}

// ============================================================================
// AliasStore implementation
// ============================================================================

void AliasStore::add(std::wstring_view exe, std::wstring_view source, std::wstring_view target)
{
    auto& map = exes[std::wstring(exe)];
    if (target.empty())
        map.erase(std::wstring(source)); // Empty target = remove alias.
    else
        map[std::wstring(source)] = std::wstring(target);

    // Clean up empty exe entries.
    if (map.empty())
        exes.erase(std::wstring(exe));
}

void AliasStore::remove(std::wstring_view exe, std::wstring_view source)
{
    auto it = exes.find(std::wstring(exe));
    if (it != exes.end())
    {
        it->second.erase(std::wstring(source));
        if (it->second.empty())
            exes.erase(it);
    }
}

const std::wstring* AliasStore::find(std::wstring_view exe, std::wstring_view source) const
{
    auto exeIt = exes.find(std::wstring(exe));
    if (exeIt == exes.end())
        return nullptr;
    auto srcIt = exeIt->second.find(std::wstring(source));
    if (srcIt == exeIt->second.end())
        return nullptr;
    return &srcIt->second;
}

void AliasStore::expunge(std::wstring_view exe)
{
    exes.erase(std::wstring(exe));
}

// ============================================================================
// CommandHistoryStore implementation
// ============================================================================

void CommandHistory::add(std::wstring_view cmd)
{
    if (!allowDuplicates)
    {
        // Remove any existing duplicate before adding.
        std::erase_if(commands, [&](const auto& c) { return c == cmd; });
    }
    commands.emplace_back(cmd);
    if (commands.size() > maxCommands)
        commands.erase(commands.begin());
}

void CommandHistory::clear()
{
    commands.clear();
}

CommandHistory& CommandHistoryStore::getOrCreate(std::wstring_view exe)
{
    auto& h = exes[std::wstring(exe)];
    if (h.maxCommands == 0)
    {
        h.maxCommands = defaultBufferSize;
        h.allowDuplicates = !(flags & 1); // HISTORY_NO_DUP_FLAG = 1
    }
    return h;
}

void CommandHistoryStore::expunge(std::wstring_view exe)
{
    auto it = exes.find(std::wstring(exe));
    if (it != exes.end())
        it->second.clear();
}

// ============================================================================
// L3 handler implementations
// ============================================================================

NTSTATUS Server::handleUserL3GetConsoleMouseInfo()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleMouseInfo;
    a.NumButtons = GetSystemMetrics(SM_CMOUSEBUTTONS);
    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3GetConsoleFontSize()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleFontSize;

    auto* h = activateOutputBuffer(GENERIC_READ);
    if (!h)
        return STATUS_INVALID_HANDLE;

    PTY_SCREEN_BUFFER_INFO info{};
    HOST_CALL(m_host->GetScreenBufferInfo(&info));

    // The OG only supports font index 0 in modern builds.
    (void)a.FontIndex;
    a.FontSize = info.FontSize;
    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3GetConsoleCurrentFont()
{
    auto& a = m_req.u.consoleMsgL3.GetCurrentConsoleFont;

    auto* h = activateOutputBuffer(GENERIC_READ);
    if (!h)
        return STATUS_INVALID_HANDLE;

    PTY_SCREEN_BUFFER_INFO info{};
    HOST_CALL(m_host->GetScreenBufferInfo(&info));

    a.FontIndex = info.FontIndex;
    a.FontSize = info.FontSize;
    a.FontFamily = info.FontFamily;
    a.FontWeight = info.FontWeight;
    memcpy(a.FaceName, info.FaceName, sizeof(a.FaceName));
    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3SetConsoleDisplayMode()
{
    auto& a = m_req.u.consoleMsgL3.SetConsoleDisplayMode;

    auto* h = activateOutputBuffer(GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    // Fullscreen is not supported. Return current buffer dimensions.
    PTY_SCREEN_BUFFER_INFO info{};
    HOST_CALL(m_host->GetScreenBufferInfo(&info));
    a.ScreenBufferDimensions = info.Size;
    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3GetConsoleDisplayMode()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleDisplayMode;
    a.ModeFlags = 0; // Always windowed.
    return STATUS_SUCCESS;
}

// ============================================================================
// Alias handlers
// ============================================================================

NTSTATUS Server::handleUserL3AddConsoleAlias()
{
    auto& a = m_req.u.consoleMsgL3.AddConsoleAliasW;

    if (a.SourceLength > USHRT_MAX || a.TargetLength > USHRT_MAX || a.ExeLength > USHRT_MAX)
        return STATUS_INVALID_PARAMETER;

    // TODO: Handle !a.Unicode (ANSI) by converting to Unicode first.
    auto payload = readTrailingInput();
    const auto totalLen = static_cast<size_t>(a.SourceLength) + a.TargetLength + a.ExeLength;
    if (payload.size() < totalLen)
        return STATUS_INVALID_PARAMETER;

    auto source = extractWideString(payload, 0, a.SourceLength);
    auto target = extractWideString(payload, a.SourceLength, a.TargetLength);
    auto exe = extractWideString(payload, a.SourceLength + a.TargetLength, a.ExeLength);

    m_aliases.add(exe, source, target);
    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3GetConsoleAlias()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleAliasW;

    // TODO: Handle !a.Unicode (ANSI).
    auto payload = readTrailingInput();
    const auto totalLen = static_cast<size_t>(a.SourceLength) + a.ExeLength;
    if (payload.size() < totalLen)
        return STATUS_INVALID_PARAMETER;

    auto source = extractWideString(payload, 0, a.SourceLength);
    auto exe = extractWideString(payload, a.SourceLength, a.ExeLength);

    const auto* target = m_aliases.find(exe, source);
    if (!target)
    {
        a.TargetLength = 0;
        return STATUS_SUCCESS;
    }

    const auto bytes = static_cast<USHORT>(target->size() * sizeof(WCHAR));
    const auto outputOffset = m_req.msgHeader.ApiDescriptorSize;
    const auto outputCapacity = (m_req.Descriptor.OutputSize > outputOffset) ? (m_req.Descriptor.OutputSize - outputOffset) : 0u;
    const auto toWrite = std::min(static_cast<ULONG>(bytes), outputCapacity);
    if (toWrite > 0)
        writeOutput(outputOffset, target->data(), toWrite);

    a.TargetLength = bytes;
    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3GetConsoleAliasesLength()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleAliasesLengthW;

    // TODO: Handle !a.Unicode (ANSI).
    auto payload = readTrailingInput();
    auto exe = extractWideString(payload, 0, static_cast<USHORT>(payload.size()));

    ULONG totalBytes = 0;
    auto exeIt = m_aliases.exes.find(exe);
    if (exeIt != m_aliases.exes.end())
    {
        for (const auto& [src, tgt] : exeIt->second)
        {
            // Format: "source=target\0" (in WCHARs)
            totalBytes += static_cast<ULONG>((src.size() + 1 + tgt.size() + 1) * sizeof(WCHAR));
        }
    }

    a.AliasesLength = totalBytes;
    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3GetConsoleAliasExesLength()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleAliasExesLengthW;

    // TODO: Handle !a.Unicode (ANSI).
    ULONG totalBytes = 0;
    for (const auto& [exe, _] : m_aliases.exes)
    {
        totalBytes += static_cast<ULONG>((exe.size() + 1) * sizeof(WCHAR)); // "exe\0"
    }

    a.AliasExesLength = totalBytes;
    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3GetConsoleAliases()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleAliasesW;

    // TODO: Handle !a.Unicode (ANSI).
    auto payload = readTrailingInput();
    auto exe = extractWideString(payload, 0, static_cast<USHORT>(payload.size()));

    const auto outputOffset = m_req.msgHeader.ApiDescriptorSize;
    const auto outputCapacity = (m_req.Descriptor.OutputSize > outputOffset) ? (m_req.Descriptor.OutputSize - outputOffset) : 0u;

    std::wstring buf;
    auto exeIt = m_aliases.exes.find(exe);
    if (exeIt != m_aliases.exes.end())
    {
        for (const auto& [src, tgt] : exeIt->second)
        {
            buf += src;
            buf += L'=';
            buf += tgt;
            buf += L'\0';
        }
    }

    const auto bytes = static_cast<ULONG>(buf.size() * sizeof(WCHAR));
    const auto toWrite = std::min(bytes, outputCapacity);
    if (toWrite > 0)
        writeOutput(outputOffset, buf.data(), toWrite);

    a.AliasesBufferLength = bytes;
    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3GetConsoleAliasExes()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleAliasExesW;

    // TODO: Handle !a.Unicode (ANSI).
    const auto outputOffset = m_req.msgHeader.ApiDescriptorSize;
    const auto outputCapacity = (m_req.Descriptor.OutputSize > outputOffset) ? (m_req.Descriptor.OutputSize - outputOffset) : 0u;

    std::wstring buf;
    for (const auto& [exe, _] : m_aliases.exes)
    {
        buf += exe;
        buf += L'\0';
    }

    const auto bytes = static_cast<ULONG>(buf.size() * sizeof(WCHAR));
    const auto toWrite = std::min(bytes, outputCapacity);
    if (toWrite > 0)
        writeOutput(outputOffset, buf.data(), toWrite);

    a.AliasExesBufferLength = bytes;
    return STATUS_SUCCESS;
}

// ============================================================================
// Command history handlers
// ============================================================================

NTSTATUS Server::handleUserL3ExpungeConsoleCommandHistory()
{
    // TODO: Handle !a.Unicode (ANSI).
    auto payload = readTrailingInput();
    auto exe = extractWideString(payload, 0, static_cast<USHORT>(payload.size()));
    m_history.expunge(exe);
    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3SetConsoleNumberOfCommands()
{
    auto& a = m_req.u.consoleMsgL3.SetConsoleNumberOfCommandsW;

    // TODO: Handle !a.Unicode (ANSI).
    auto payload = readTrailingInput();
    auto exe = extractWideString(payload, 0, static_cast<USHORT>(payload.size()));
    auto& hist = m_history.getOrCreate(exe);
    hist.maxCommands = a.NumCommands;
    // Trim if current history exceeds new limit.
    while (hist.commands.size() > hist.maxCommands)
        hist.commands.erase(hist.commands.begin());
    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3GetConsoleCommandHistoryLength()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleCommandHistoryLengthW;

    // TODO: Handle !a.Unicode (ANSI).
    auto payload = readTrailingInput();
    auto exe = extractWideString(payload, 0, static_cast<USHORT>(payload.size()));

    ULONG totalBytes = 0;
    auto it = m_history.exes.find(exe);
    if (it != m_history.exes.end())
    {
        for (const auto& cmd : it->second.commands)
            totalBytes += static_cast<ULONG>((cmd.size() + 1) * sizeof(WCHAR)); // "cmd\0"
    }

    a.CommandHistoryLength = totalBytes;
    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3GetConsoleCommandHistory()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleCommandHistoryW;

    // TODO: Handle !a.Unicode (ANSI).
    auto payload = readTrailingInput();
    auto exe = extractWideString(payload, 0, static_cast<USHORT>(payload.size()));

    const auto outputOffset = m_req.msgHeader.ApiDescriptorSize;
    const auto outputCapacity = (m_req.Descriptor.OutputSize > outputOffset) ? (m_req.Descriptor.OutputSize - outputOffset) : 0u;

    std::wstring buf;
    auto it = m_history.exes.find(exe);
    if (it != m_history.exes.end())
    {
        for (const auto& cmd : it->second.commands)
        {
            buf += cmd;
            buf += L'\0';
        }
    }

    const auto bytes = static_cast<ULONG>(buf.size() * sizeof(WCHAR));
    const auto toWrite = std::min(bytes, outputCapacity);
    if (toWrite > 0)
        writeOutput(outputOffset, buf.data(), toWrite);

    a.CommandBufferLength = bytes;
    return STATUS_SUCCESS;
}

// ============================================================================
// Window, selection, process, history settings, font
// ============================================================================

NTSTATUS Server::handleUserL3GetConsoleWindow()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleWindow;

    HWND hwnd = nullptr;
    if (m_host)
        m_host->GetConsoleWindow(&hwnd);
    a.hwnd = hwnd;
    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3GetConsoleSelectionInfo()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleSelectionInfo;
    // Selection is not supported in PTY mode.
    a.SelectionInfo = {};
    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3GetConsoleProcessList()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleProcessList;

    const auto capacity = a.dwProcessCount;

    std::vector<DWORD> pids;
    pids.reserve(m_clients.size());
    for (auto& c : m_clients)
        pids.push_back(c->processId);

    a.dwProcessCount = static_cast<DWORD>(pids.size());

    if (capacity >= pids.size() && !pids.empty())
    {
        const auto writeOffset = m_req.msgHeader.ApiDescriptorSize;
        writeOutput(writeOffset, pids.data(), static_cast<ULONG>(pids.size() * sizeof(DWORD)));
    }

    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3GetConsoleHistory()
{
    auto& a = m_req.u.consoleMsgL3.GetConsoleHistory;
    a.HistoryBufferSize = m_history.defaultBufferSize;
    a.NumberOfHistoryBuffers = m_history.numberOfBuffers;
    a.dwFlags = m_history.flags;
    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3SetConsoleHistory()
{
    auto& a = m_req.u.consoleMsgL3.SetConsoleHistory;
    m_history.defaultBufferSize = a.HistoryBufferSize;
    m_history.numberOfBuffers = a.NumberOfHistoryBuffers;
    m_history.flags = a.dwFlags;
    return STATUS_SUCCESS;
}

NTSTATUS Server::handleUserL3SetConsoleCurrentFont()
{
    auto& a = m_req.u.consoleMsgL3.SetCurrentConsoleFont;

    auto* h = activateOutputBuffer(GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    // TODO: Font changes via SetScreenBufferInfo once we add font fields to the change struct.
    (void)a;
    return STATUS_SUCCESS;
}

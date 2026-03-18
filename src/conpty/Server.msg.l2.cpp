#include "pch.h"
#include "Server.h"

// Helper: call an IPtyHost method that returns HRESULT, and convert failure
// to an NTSTATUS return from the calling handler.
#define HOST_CALL(expr) \
    do { \
        const auto _hr = (expr); \
        if (FAILED(_hr)) return STATUS_UNSUCCESSFUL; \
    } while (0)

// L2: FillConsoleOutput
// OG: SrvFillConsoleOutput in directio.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
// Reads a->WriteCoord, a->ElementType, a->Element, a->Length.
// Returns a->Length (actual count filled).
NTSTATUS Server::handleUserL2FillConsoleOutput()
{
    auto& a = m_req.u.consoleMsgL2.FillConsoleOutput;

    auto* h = activateOutputBuffer(GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    // TODO: Read existing cells via ReadBuffer, modify per ElementType, WriteUTF16 back.
    //       This requires the host to support cell-level read/write.
    a.Length = 0;
    return STATUS_SUCCESS;
}

// L2: GenerateConsoleCtrlEvent
// OG: SrvGenerateConsoleCtrlEvent in getset.cpp — no handle validation.
// Reads a->CtrlEvent, a->ProcessGroupId.
NTSTATUS Server::handleUserL2GenerateConsoleCtrlEvent()
{
    auto& a = m_req.u.consoleMsgL2.GenerateConsoleCtrlEvent;

    // TODO: Dispatch ctrl event (CTRL_C_EVENT / CTRL_BREAK_EVENT)
    //       to processes in a->ProcessGroupId.
    (void)a.CtrlEvent;
    (void)a.ProcessGroupId;
    return STATUS_SUCCESS;
}

// L2: SetConsoleActiveScreenBuffer
// OG: SrvSetConsoleActiveScreenBuffer in getset.cpp
// DereferenceIoHandle(obj, OUTPUT|GRAPHICS_OUTPUT, GENERIC_WRITE)
NTSTATUS Server::handleUserL2SetConsoleActiveScreenBuffer()
{
    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    if (h->screenBuffer != m_activeScreenBuffer && m_host)
    {
        const auto hr = m_host->ActivateBuffer(h->screenBuffer);
        if (FAILED(hr))
            return STATUS_UNSUCCESSFUL;
        m_activeScreenBuffer = h->screenBuffer;
    }

    return STATUS_SUCCESS;
}

// L2: FlushConsoleInputBuffer
// OG: SrvFlushConsoleInputBuffer in getset.cpp
// DereferenceIoHandle(obj, INPUT, GENERIC_WRITE)
NTSTATUS Server::handleUserL2FlushConsoleInputBuffer()
{
    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_INPUT_HANDLE, GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    m_input.flush();
    m_inputAvailableEvent.ResetEvent();
    return STATUS_SUCCESS;
}

// L2: SetConsoleCP / SetConsoleOutputCP
// OG: SrvSetConsoleCP in getset.cpp — no handle validation.
// Reads a->CodePage, a->Output.
NTSTATUS Server::handleUserL2SetConsoleCP()
{
    auto& a = m_req.u.consoleMsgL2.SetConsoleCP;

    if (!IsValidCodePage(a.CodePage))
        return STATUS_INVALID_PARAMETER;

    if (a.Output)
        m_outputCP = a.CodePage;
    else
        m_inputCP = a.CodePage;

    return STATUS_SUCCESS;
}

// L2: GetConsoleCursorInfo
// OG: SrvGetConsoleCursorInfo in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_READ)
// Returns a->CursorSize, a->Visible.
NTSTATUS Server::handleUserL2GetConsoleCursorInfo()
{
    auto& a = m_req.u.consoleMsgL2.GetConsoleCursorInfo;

    auto* h = activateOutputBuffer(GENERIC_READ);
    if (!h)
        return STATUS_INVALID_HANDLE;

    PTY_SCREEN_BUFFER_INFO info{};
    HOST_CALL(m_host->GetScreenBufferInfo(&info));

    a.CursorSize = info.CursorSize;
    a.Visible = info.CursorVisible;
    return STATUS_SUCCESS;
}

// L2: SetConsoleCursorInfo
// OG: SrvSetConsoleCursorInfo in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
// Reads a->CursorSize, a->Visible.
NTSTATUS Server::handleUserL2SetConsoleCursorInfo()
{
    auto& a = m_req.u.consoleMsgL2.SetConsoleCursorInfo;

    auto* h = activateOutputBuffer(GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    if (a.CursorSize < 1 || a.CursorSize > 100)
        return STATUS_INVALID_PARAMETER;

    ULONG cursorSize = a.CursorSize;
    BOOLEAN cursorVisible = a.Visible;
    PTY_SCREEN_BUFFER_INFO_CHANGE change{};
    change.CursorSize = &cursorSize;
    change.CursorVisible = &cursorVisible;
    HOST_CALL(m_host->SetScreenBufferInfo(&change));

    return STATUS_SUCCESS;
}

// L2: GetConsoleScreenBufferInfo (GetConsoleScreenBufferInfoEx)
// OG: SrvGetConsoleScreenBufferInfo in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_READ)
NTSTATUS Server::handleUserL2GetConsoleScreenBufferInfo()
{
    auto& a = m_req.u.consoleMsgL2.GetConsoleScreenBufferInfo;

    auto* h = activateOutputBuffer(GENERIC_READ);
    if (!h)
        return STATUS_INVALID_HANDLE;

    PTY_SCREEN_BUFFER_INFO info{};
    HOST_CALL(m_host->GetScreenBufferInfo(&info));

    a.Size = info.Size;
    a.CursorPosition = info.CursorPosition;
    a.ScrollPosition = { info.Window.Left, info.Window.Top };
    a.Attributes = info.Attributes;
    a.CurrentWindowSize = {
        static_cast<SHORT>(info.Window.Right - info.Window.Left + 1),
        static_cast<SHORT>(info.Window.Bottom - info.Window.Top + 1),
    };
    a.MaximumWindowSize = info.MaximumWindowSize;
    a.PopupAttributes = info.PopupAttributes;
    a.FullscreenSupported = info.FullscreenSupported;
    memcpy(a.ColorTable, info.ColorTable, sizeof(a.ColorTable));
    return STATUS_SUCCESS;
}

// L2: SetConsoleScreenBufferInfoEx
// OG: SrvSetConsoleScreenBufferInfo in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
NTSTATUS Server::handleUserL2SetConsoleScreenBufferInfo()
{
    auto& a = m_req.u.consoleMsgL2.SetConsoleScreenBufferInfo;

    auto* h = activateOutputBuffer(GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    WORD attributes = a.Attributes;
    WORD popupAttributes = a.PopupAttributes;
    SMALL_RECT window = { 0, 0, static_cast<SHORT>(a.CurrentWindowSize.X - 1), static_cast<SHORT>(a.CurrentWindowSize.Y - 1) };

    PTY_SCREEN_BUFFER_INFO_CHANGE change{};
    if (a.Size.X > 0 && a.Size.Y > 0)
        change.Size = &a.Size;
    change.Attributes = &attributes;
    change.PopupAttributes = &popupAttributes;
    change.Window = &window;
    change.ColorTable = a.ColorTable;

    HOST_CALL(m_host->SetScreenBufferInfo(&change));
    return STATUS_SUCCESS;
}

// L2: SetConsoleScreenBufferSize
// OG: SrvSetConsoleScreenBufferSize in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
NTSTATUS Server::handleUserL2SetConsoleScreenBufferSize()
{
    auto& a = m_req.u.consoleMsgL2.SetConsoleScreenBufferSize;

    auto* h = activateOutputBuffer(GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    if (a.Size.X <= 0 || a.Size.Y <= 0)
        return STATUS_INVALID_PARAMETER;

    PTY_SCREEN_BUFFER_INFO_CHANGE change{};
    change.Size = &a.Size;
    HOST_CALL(m_host->SetScreenBufferInfo(&change));
    return STATUS_SUCCESS;
}

// L2: SetConsoleCursorPosition
// OG: SrvSetConsoleCursorPosition in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
NTSTATUS Server::handleUserL2SetConsoleCursorPosition()
{
    auto& a = m_req.u.consoleMsgL2.SetConsoleCursorPosition;

    auto* h = activateOutputBuffer(GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    PTY_SCREEN_BUFFER_INFO_CHANGE change{};
    change.CursorPosition = &a.CursorPosition;
    HOST_CALL(m_host->SetScreenBufferInfo(&change));
    return STATUS_SUCCESS;
}

// L2: GetLargestConsoleWindowSize
// OG: SrvGetLargestConsoleWindowSize in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE) — yes, WRITE for a getter.
NTSTATUS Server::handleUserL2GetLargestConsoleWindowSize()
{
    auto& a = m_req.u.consoleMsgL2.GetLargestConsoleWindowSize;

    auto* h = activateOutputBuffer(GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    PTY_SCREEN_BUFFER_INFO info{};
    HOST_CALL(m_host->GetScreenBufferInfo(&info));

    a.Size = info.MaximumWindowSize;
    return STATUS_SUCCESS;
}

// L2: ScrollConsoleScreenBuffer
// OG: SrvScrollConsoleScreenBuffer in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
NTSTATUS Server::handleUserL2ScrollConsoleScreenBuffer()
{
    auto& a = m_req.u.consoleMsgL2.ScrollConsoleScreenBuffer;

    auto* h = activateOutputBuffer(GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    // TODO: Read source rectangle via ReadBuffer, write to destination,
    //       fill vacated area with a->Fill. Needs host ReadBuffer + WriteUTF16 support.
    (void)a;
    return STATUS_SUCCESS;
}

// L2: SetConsoleTextAttribute
// OG: SrvSetConsoleTextAttribute in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
NTSTATUS Server::handleUserL2SetConsoleTextAttribute()
{
    auto& a = m_req.u.consoleMsgL2.SetConsoleTextAttribute;

    auto* h = activateOutputBuffer(GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    PTY_SCREEN_BUFFER_INFO_CHANGE change{};
    change.Attributes = &a.Attributes;
    HOST_CALL(m_host->SetScreenBufferInfo(&change));
    return STATUS_SUCCESS;
}

// L2: SetConsoleWindowInfo
// OG: SrvSetConsoleWindowInfo in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
NTSTATUS Server::handleUserL2SetConsoleWindowInfo()
{
    auto& a = m_req.u.consoleMsgL2.SetConsoleWindowInfo;

    auto* h = activateOutputBuffer(GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    SMALL_RECT window = a.Window;
    if (!a.Absolute)
    {
        // Relative mode: get current window position first.
        PTY_SCREEN_BUFFER_INFO info{};
        HOST_CALL(m_host->GetScreenBufferInfo(&info));
        window.Left += info.Window.Left;
        window.Top += info.Window.Top;
        window.Right += info.Window.Right;
        window.Bottom += info.Window.Bottom;
    }

    PTY_SCREEN_BUFFER_INFO_CHANGE change{};
    change.Window = &window;
    HOST_CALL(m_host->SetScreenBufferInfo(&change));
    return STATUS_SUCCESS;
}

// L2: ReadConsoleOutputString (ReadConsoleOutputCharacter / ReadConsoleOutputAttribute)
// OG: SrvReadConsoleOutputString in directio.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_READ)
NTSTATUS Server::handleUserL2ReadConsoleOutputString()
{
    auto& a = m_req.u.consoleMsgL2.ReadConsoleOutputString;

    auto* h = activateOutputBuffer(GENERIC_READ);
    if (!h)
        return STATUS_INVALID_HANDLE;

    const auto outputOffset = m_req.msgHeader.ApiDescriptorSize;
    const auto maxOutputBytes = (m_req.Descriptor.OutputSize > outputOffset) ? (m_req.Descriptor.OutputSize - outputOffset) : 0u;

    // Compute how many cells we can read based on the output capacity and string type.
    ULONG maxCells = 0;
    switch (a.StringType)
    {
    case CONSOLE_ASCII:
        maxCells = maxOutputBytes; // 1 byte per cell
        break;
    case CONSOLE_REAL_UNICODE:
        maxCells = maxOutputBytes / sizeof(WCHAR); // 2 bytes per cell
        break;
    case CONSOLE_ATTRIBUTE:
        maxCells = maxOutputBytes / sizeof(WORD); // 2 bytes per cell
        break;
    default:
        return STATUS_INVALID_PARAMETER;
    }

    if (maxCells == 0)
    {
        a.NumRecords = 0;
        return STATUS_SUCCESS;
    }

    // Read cells from the host.
    std::vector<PTY_CHAR_INFO> cells(maxCells);
    const auto hr = m_host->ReadBuffer(a.ReadCoord, maxCells, cells.data());
    if (FAILED(hr))
    {
        a.NumRecords = 0;
        return STATUS_SUCCESS;
    }

    // Convert to the requested string type.
    if (a.StringType == CONSOLE_REAL_UNICODE)
    {
        std::vector<WCHAR> chars(maxCells);
        for (ULONG i = 0; i < maxCells; i++)
            chars[i] = cells[i].Char;
        writeOutput(outputOffset, chars.data(), maxCells * sizeof(WCHAR));
    }
    else if (a.StringType == CONSOLE_ASCII)
    {
        // Convert each Unicode char to the output code page.
        std::vector<WCHAR> wchars(maxCells);
        for (ULONG i = 0; i < maxCells; i++)
            wchars[i] = cells[i].Char;
        const auto ansiLen = WideCharToMultiByte(m_outputCP, 0, wchars.data(), maxCells, nullptr, 0, nullptr, nullptr);
        if (ansiLen > 0)
        {
            std::vector<char> ansi(ansiLen);
            WideCharToMultiByte(m_outputCP, 0, wchars.data(), maxCells, ansi.data(), ansiLen, nullptr, nullptr);
            const auto toWrite = std::min(static_cast<ULONG>(ansiLen), maxOutputBytes);
            writeOutput(outputOffset, ansi.data(), toWrite);
            // For ASCII, the cell count may differ from byte count due to DBCS.
            // We return the number of cells consumed (= maxCells).
        }
    }
    else if (a.StringType == CONSOLE_ATTRIBUTE)
    {
        std::vector<WORD> attrs(maxCells);
        for (ULONG i = 0; i < maxCells; i++)
            attrs[i] = cells[i].Attributes;
        writeOutput(outputOffset, attrs.data(), maxCells * sizeof(WORD));
    }

    a.NumRecords = maxCells;
    return STATUS_SUCCESS;
}

// L2: WriteConsoleInput
// OG: SrvWriteConsoleInput in directio.cpp
// DereferenceIoHandle(obj, INPUT, GENERIC_WRITE)
NTSTATUS Server::handleUserL2WriteConsoleInput()
{
    auto& a = m_req.u.consoleMsgL2.WriteConsoleInput;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_INPUT_HANDLE, GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    auto payload = readTrailingInput();
    auto numRecords = static_cast<ULONG>(payload.size() / sizeof(INPUT_RECORD));

    // TODO: Handle Unicode vs ANSI conversion if !a->Unicode.
    const auto* records = reinterpret_cast<const INPUT_RECORD*>(payload.data());
    for (ULONG i = 0; i < numRecords; i++)
    {
        const auto& rec = records[i];
        if (rec.EventType == KEY_EVENT)
        {
            const auto& ke = rec.Event.KeyEvent;
            char buf[128];
            const auto n = snprintf(buf, sizeof(buf), "\x1b[%u;%u;%u;%u;%lu;%u_",
                ke.wVirtualKeyCode,
                ke.wVirtualScanCode,
                static_cast<unsigned>(ke.uChar.UnicodeChar),
                ke.bKeyDown ? 1u : 0u,
                ke.dwControlKeyState,
                ke.wRepeatCount);
            if (n > 0)
                m_input.write({ buf, static_cast<size_t>(n) });
        }
        // TODO: Handle MOUSE_EVENT, WINDOW_BUFFER_SIZE_EVENT, etc.
    }

    if (numRecords > 0 && m_input.hasData())
    {
        m_inputAvailableEvent.SetEvent();
        drainPendingInputReads();
    }

    a.NumRecords = numRecords;
    return STATUS_SUCCESS;
}

// L2: WriteConsoleOutput
// OG: SrvWriteConsoleOutput in directio.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
NTSTATUS Server::handleUserL2WriteConsoleOutput()
{
    auto& a = m_req.u.consoleMsgL2.WriteConsoleOutput;

    auto* h = activateOutputBuffer(GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    auto payload = readTrailingInput();

    // TODO: Write CHAR_INFO grid from payload into screen buffer at a->CharRegion.
    //       This requires a WriteBuffer-like host callback or per-row WriteUTF16.
    (void)payload;
    a.CharRegion = {};
    return STATUS_SUCCESS;
}

// L2: WriteConsoleOutputString (WriteConsoleOutputCharacter / WriteConsoleOutputAttribute)
// OG: SrvWriteConsoleOutputString in directio.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
NTSTATUS Server::handleUserL2WriteConsoleOutputString()
{
    auto& a = m_req.u.consoleMsgL2.WriteConsoleOutputString;

    auto* h = activateOutputBuffer(GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    auto payload = readTrailingInput();

    // TODO: Write chars/attrs from payload to screen buffer at a->WriteCoord.
    //       Requires per-cell write support on the host.
    (void)payload;
    a.NumRecords = 0;
    return STATUS_SUCCESS;
}

// L2: ReadConsoleOutput
// OG: SrvReadConsoleOutput in directio.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_READ)
NTSTATUS Server::handleUserL2ReadConsoleOutput()
{
    auto& a = m_req.u.consoleMsgL2.ReadConsoleOutput;

    auto* h = activateOutputBuffer(GENERIC_READ);
    if (!h)
        return STATUS_INVALID_HANDLE;

    const auto outputOffset = m_req.msgHeader.ApiDescriptorSize;
    const auto maxOutputBytes = (m_req.Descriptor.OutputSize > outputOffset) ? (m_req.Descriptor.OutputSize - outputOffset) : 0u;

    const SHORT width = a.CharRegion.Right - a.CharRegion.Left + 1;
    const SHORT height = a.CharRegion.Bottom - a.CharRegion.Top + 1;

    if (width <= 0 || height <= 0)
    {
        a.CharRegion = {};
        return STATUS_SUCCESS;
    }

    const auto totalCells = static_cast<ULONG>(width) * height;
    const auto maxCells = maxOutputBytes / sizeof(CHAR_INFO);

    if (maxCells == 0)
    {
        a.CharRegion = {};
        return STATUS_SUCCESS;
    }

    // Read row by row from the host.
    std::vector<CHAR_INFO> result(totalCells);
    for (SHORT row = 0; row < height; row++)
    {
        COORD readPos = { a.CharRegion.Left, static_cast<SHORT>(a.CharRegion.Top + row) };
        std::vector<PTY_CHAR_INFO> cells(width);
        const auto hr = m_host->ReadBuffer(readPos, width, cells.data());
        if (FAILED(hr))
            break;

        for (SHORT col = 0; col < width; col++)
        {
            auto& ci = result[row * width + col];
            ci.Char.UnicodeChar = cells[col].Char;
            ci.Attributes = cells[col].Attributes;
        }
    }

    const auto cellsToWrite = std::min(totalCells, static_cast<ULONG>(maxCells));
    if (cellsToWrite > 0)
        writeOutput(outputOffset, result.data(), cellsToWrite * sizeof(CHAR_INFO));

    a.CharRegion = {
        a.CharRegion.Left,
        a.CharRegion.Top,
        static_cast<SHORT>(a.CharRegion.Left + width - 1),
        static_cast<SHORT>(a.CharRegion.Top + height - 1),
    };
    return STATUS_SUCCESS;
}

// L2: GetConsoleTitle
// OG: SrvGetConsoleTitle in cmdline.cpp — no handle validation.
NTSTATUS Server::handleUserL2GetConsoleTitle()
{
    auto& a = m_req.u.consoleMsgL2.GetConsoleTitle;

    const auto& title = a.Original ? m_originalTitle : m_title;
    const auto outputOffset = m_req.msgHeader.ApiDescriptorSize;
    const auto outputCapacity = (m_req.Descriptor.OutputSize > outputOffset) ? (m_req.Descriptor.OutputSize - outputOffset) : 0u;

    if (a.Unicode)
    {
        const auto bytes = static_cast<ULONG>(title.size() * sizeof(WCHAR));
        const auto bytesToWrite = std::min(bytes, outputCapacity);
        if (bytesToWrite > 0)
            writeOutput(outputOffset, title.data(), bytesToWrite);
        a.TitleLength = bytes;
    }
    else
    {
        const auto ansiLen = WideCharToMultiByte(m_outputCP, 0, title.data(), static_cast<int>(title.size()), nullptr, 0, nullptr, nullptr);
        const auto bytesToWrite = std::min(static_cast<ULONG>(std::max(ansiLen, 0)), outputCapacity);
        if (bytesToWrite > 0)
        {
            std::string ansi(ansiLen, '\0');
            WideCharToMultiByte(m_outputCP, 0, title.data(), static_cast<int>(title.size()), ansi.data(), ansiLen, nullptr, nullptr);
            writeOutput(outputOffset, ansi.data(), bytesToWrite);
        }
        a.TitleLength = static_cast<ULONG>(std::max(ansiLen, 0));
    }

    return STATUS_SUCCESS;
}

// L2: SetConsoleTitle
// OG: SrvSetConsoleTitle in cmdline.cpp — no handle validation.
NTSTATUS Server::handleUserL2SetConsoleTitle()
{
    auto& a = m_req.u.consoleMsgL2.SetConsoleTitle;

    auto payload = readTrailingInput();

    if (a.Unicode)
    {
        m_title.assign(reinterpret_cast<const wchar_t*>(payload.data()), payload.size() / sizeof(wchar_t));
    }
    else
    {
        const auto len = static_cast<int>(payload.size());
        const auto wideLen = MultiByteToWideChar(m_outputCP, 0, reinterpret_cast<const char*>(payload.data()), len, nullptr, 0);
        if (wideLen > 0)
        {
            m_title.resize(wideLen);
            MultiByteToWideChar(m_outputCP, 0, reinterpret_cast<const char*>(payload.data()), len, m_title.data(), wideLen);
        }
        else
        {
            m_title.clear();
        }
    }

    // Emit the title to the host as an OSC sequence.
    vtAppendTitle(m_title);
    vtFlush();
    return STATUS_SUCCESS;
}

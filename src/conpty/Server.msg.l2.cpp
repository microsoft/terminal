#include "pch.h"
#include "Server.h"

// L2: FillConsoleOutput
// OG: SrvFillConsoleOutput in directio.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
// Reads a->WriteCoord, a->ElementType, a->Element, a->Length.
// Returns a->Length (actual count filled).
NTSTATUS Server::handleUserL2FillConsoleOutput()
{
    auto& a = m_req.u.consoleMsgL2.FillConsoleOutput;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Fill screen buffer at a->WriteCoord with a->Element
    //       for a->Length cells. a->ElementType selects char vs attribute.
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
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Switch active screen buffer to the handle's buffer.
    return STATUS_SUCCESS;
}

// L2: FlushConsoleInputBuffer
// OG: SrvFlushConsoleInputBuffer in getset.cpp
// DereferenceIoHandle(obj, INPUT, GENERIC_WRITE)
NTSTATUS Server::handleUserL2FlushConsoleInputBuffer()
{
    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_INPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Clear input queue and reset input-available event.
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

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_READ);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    a.CursorSize = m_cursorSize;
    a.Visible = m_cursorVisible;
    return STATUS_SUCCESS;
}

// L2: SetConsoleCursorInfo
// OG: SrvSetConsoleCursorInfo in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
// Reads a->CursorSize, a->Visible.
NTSTATUS Server::handleUserL2SetConsoleCursorInfo()
{
    auto& a = m_req.u.consoleMsgL2.SetConsoleCursorInfo;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    if (a.CursorSize < 1 || a.CursorSize > 100)
        return STATUS_INVALID_PARAMETER;

    m_cursorSize = a.CursorSize;

    // Emit DECTCEM when visibility changes.
    if (m_cursorVisible != static_cast<bool>(a.Visible))
    {
        m_cursorVisible = static_cast<bool>(a.Visible);
        vtAppendFmt("\x1b[?25%c", m_cursorVisible ? 'h' : 'l');
        vtFlush();
    }

    return STATUS_SUCCESS;
}

// L2: GetConsoleScreenBufferInfo (GetConsoleScreenBufferInfoEx)
// OG: SrvGetConsoleScreenBufferInfo in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_READ)
// Returns Size, CursorPosition, ScrollPosition, Attributes, CurrentWindowSize,
//         MaximumWindowSize, PopupAttributes, FullscreenSupported, ColorTable[16].
NTSTATUS Server::handleUserL2GetConsoleScreenBufferInfo()
{
    auto& a = m_req.u.consoleMsgL2.GetConsoleScreenBufferInfo;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_READ);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Fill from actual screen buffer state.
    a.Size = m_bufferSize;
    a.CursorPosition = m_cursorPosition;
    a.ScrollPosition = { 0, 0 };
    a.Attributes = m_attributes;
    a.CurrentWindowSize = m_viewSize;
    a.MaximumWindowSize = m_viewSize;
    a.PopupAttributes = m_popupAttributes;
    a.FullscreenSupported = FALSE;
    memcpy(a.ColorTable, m_colorTable, sizeof(m_colorTable));
    return STATUS_SUCCESS;
}

// L2: SetConsoleScreenBufferInfoEx
// OG: SrvSetConsoleScreenBufferInfo in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
// Reads Size, CurrentWindowSize, Attributes, PopupAttributes, ColorTable, etc.
NTSTATUS Server::handleUserL2SetConsoleScreenBufferInfo()
{
    auto& a = m_req.u.consoleMsgL2.SetConsoleScreenBufferInfo;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Apply screen buffer settings from a->*.
    if (a.Size.X > 0 && a.Size.Y > 0)
        m_bufferSize = a.Size;
    if (a.CurrentWindowSize.X > 0 && a.CurrentWindowSize.Y > 0)
        m_viewSize = a.CurrentWindowSize;

    m_attributes = a.Attributes;
    m_popupAttributes = a.PopupAttributes;
    memcpy(m_colorTable, a.ColorTable, sizeof(m_colorTable));

    vtAppendSGR(m_attributes);
    vtFlush();
    return STATUS_SUCCESS;
}

// L2: SetConsoleScreenBufferSize
// OG: SrvSetConsoleScreenBufferSize in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
// Reads a->Size.
NTSTATUS Server::handleUserL2SetConsoleScreenBufferSize()
{
    auto& a = m_req.u.consoleMsgL2.SetConsoleScreenBufferSize;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Resize screen buffer to a->Size.
    if (a.Size.X <= 0 || a.Size.Y <= 0)
        return STATUS_INVALID_PARAMETER;

    m_bufferSize = a.Size;
    return STATUS_SUCCESS;
}

// L2: SetConsoleCursorPosition
// OG: SrvSetConsoleCursorPosition in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
// Reads a->CursorPosition.
NTSTATUS Server::handleUserL2SetConsoleCursorPosition()
{
    auto& a = m_req.u.consoleMsgL2.SetConsoleCursorPosition;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Move cursor to a->CursorPosition, scrolling if needed.
    if (a.CursorPosition.X < 0 || a.CursorPosition.X >= m_bufferSize.X ||
        a.CursorPosition.Y < 0 || a.CursorPosition.Y >= m_bufferSize.Y)
        return STATUS_INVALID_PARAMETER;

    m_cursorPosition = a.CursorPosition;
    vtAppendCUP(m_cursorPosition.Y, m_cursorPosition.X);
    vtFlush();
    return STATUS_SUCCESS;
}

// L2: GetLargestConsoleWindowSize
// OG: SrvGetLargestConsoleWindowSize in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE) — yes, WRITE for a getter.
// Returns a->Size.
NTSTATUS Server::handleUserL2GetLargestConsoleWindowSize()
{
    auto& a = m_req.u.consoleMsgL2.GetLargestConsoleWindowSize;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Return actual largest window size.
    a.Size = m_viewSize;
    return STATUS_SUCCESS;
}

// L2: ScrollConsoleScreenBuffer
// OG: SrvScrollConsoleScreenBuffer in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
// Reads ScrollRectangle, ClipRectangle, Clip, DestinationOrigin, Fill, Unicode.
NTSTATUS Server::handleUserL2ScrollConsoleScreenBuffer()
{
    auto& a = m_req.u.consoleMsgL2.ScrollConsoleScreenBuffer;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Scroll region from a->ScrollRectangle to a->DestinationOrigin,
    //       clipping by a->ClipRectangle if a->Clip, filling with a->Fill.
    (void)a;
    return STATUS_SUCCESS;
}

// L2: SetConsoleTextAttribute
// OG: SrvSetConsoleTextAttribute in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
// Reads a->Attributes.
NTSTATUS Server::handleUserL2SetConsoleTextAttribute()
{
    auto& a = m_req.u.consoleMsgL2.SetConsoleTextAttribute;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Set current text attributes on screen buffer.
    m_attributes = a.Attributes;
    vtAppendSGR(m_attributes);
    vtFlush();
    return STATUS_SUCCESS;
}

// L2: SetConsoleWindowInfo
// OG: SrvSetConsoleWindowInfo in getset.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
// Reads a->Absolute, a->Window.
NTSTATUS Server::handleUserL2SetConsoleWindowInfo()
{
    auto& a = m_req.u.consoleMsgL2.SetConsoleWindowInfo;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Set window to a->Window (absolute or relative based on a->Absolute).
    auto window = a.Window;
    if (!a.Absolute)
    {
        // In the OG, relative mode adds offsets to each edge of the current window.
        // Our viewport always starts at (0,0), so current window is {0, 0, viewSize-1}.
        window.Right += m_viewSize.X - 1;
        window.Bottom += m_viewSize.Y - 1;
    }

    const SHORT newWidth = window.Right - window.Left + 1;
    const SHORT newHeight = window.Bottom - window.Top + 1;
    if (newWidth > 0 && newHeight > 0)
    {
        m_viewSize = { newWidth, newHeight };
    }

    return STATUS_SUCCESS;
}

// L2: ReadConsoleOutputString (ReadConsoleOutputCharacter / ReadConsoleOutputAttribute)
// OG: SrvReadConsoleOutputString in directio.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_READ)
// Reads a->ReadCoord, a->StringType (CONSOLE_ASCII / CONSOLE_REAL_UNICODE / CONSOLE_ATTRIBUTE).
// Writes char/attr data via writeOutput(). Returns a->NumRecords.
NTSTATUS Server::handleUserL2ReadConsoleOutputString()
{
    auto& a = m_req.u.consoleMsgL2.ReadConsoleOutputString;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_READ);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Read chars/attrs from screen buffer at a->ReadCoord.
    //       Write result via writeOutput() at offset a->ApiDescriptorSize.
    a.NumRecords = 0;
    return STATUS_SUCCESS;
}

// L2: WriteConsoleInput
// OG: SrvWriteConsoleInput in directio.cpp
// DereferenceIoHandle(obj, INPUT, GENERIC_WRITE)
// Reads a->Unicode, a->Append. Trailing input = INPUT_RECORD array.
// Returns a->NumRecords (count actually written).
NTSTATUS Server::handleUserL2WriteConsoleInput()
{
    auto& a = m_req.u.consoleMsgL2.WriteConsoleInput;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_INPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    auto payload = readTrailingInput();
    auto numRecords = static_cast<ULONG>(payload.size() / sizeof(INPUT_RECORD));

    // TODO: Handle Unicode vs ANSI conversion if !a->Unicode.
    // For now, inject the INPUT_RECORDs by serializing them as W32IM VT sequences
    // into the input buffer, so they come back out correctly on dequeue.
    // This is a simplification — the OG directly inserts into the input queue.
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
// Reads a->Unicode, a->CharRegion. Trailing input = CHAR_INFO array.
// Returns a->CharRegion (actual region written).
NTSTATUS Server::handleUserL2WriteConsoleOutput()
{
    auto& a = m_req.u.consoleMsgL2.WriteConsoleOutput;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    auto payload = readTrailingInput();

    // TODO: Write CHAR_INFO grid from payload into screen buffer at a->CharRegion.
    //       Handle Unicode vs ANSI. Update a->CharRegion to actual written region.
    (void)payload;
    a.CharRegion = {};
    return STATUS_SUCCESS;
}

// L2: WriteConsoleOutputString (WriteConsoleOutputCharacter / WriteConsoleOutputAttribute)
// OG: SrvWriteConsoleOutputString in directio.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
// Reads a->WriteCoord, a->StringType. Trailing input = char/attr data.
// Returns a->NumRecords.
NTSTATUS Server::handleUserL2WriteConsoleOutputString()
{
    auto& a = m_req.u.consoleMsgL2.WriteConsoleOutputString;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    auto payload = readTrailingInput();

    // TODO: Write chars/attrs from payload to screen buffer at a->WriteCoord.
    (void)payload;
    a.NumRecords = 0;
    return STATUS_SUCCESS;
}

// L2: ReadConsoleOutput
// OG: SrvReadConsoleOutput in directio.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_READ)
// Reads a->Unicode, a->CharRegion.
// Writes CHAR_INFO array via writeOutput(). Returns a->CharRegion.
NTSTATUS Server::handleUserL2ReadConsoleOutput()
{
    auto& a = m_req.u.consoleMsgL2.ReadConsoleOutput;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_READ);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Read CHAR_INFO grid from screen buffer at a->CharRegion.
    //       Write result via writeOutput(). Update a->CharRegion to actual read region.
    a.CharRegion = {};
    return STATUS_SUCCESS;
}

// L2: GetConsoleTitle
// OG: SrvGetConsoleTitle in cmdline.cpp — no handle validation.
// Reads a->Unicode, a->Original.
// Writes title string via writeOutput(). Returns a->TitleLength.
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
        {
            writeOutput(outputOffset, title.data(), bytesToWrite);
        }
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
// Reads a->Unicode. Trailing input = title string.
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

    vtAppendTitle(m_title);
    vtFlush();
    return STATUS_SUCCESS;
}

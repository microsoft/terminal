#include "pch.h"

#include "Server.h"

// L1: GetConsoleCP / GetConsoleOutputCP
// OG: SrvGetConsoleCP in getset.cpp — no handle validation.
// Reads a->Output to decide input vs output CP, returns a->CodePage.
NTSTATUS Server::handleUserL1GetConsoleCP()
{
    auto& a = m_req.u.consoleMsgL1.GetConsoleCP;
    a.CodePage = a.Output ? m_outputCP : m_inputCP;
    return STATUS_SUCCESS;
}

// L1: GetConsoleMode
// OG: SrvGetConsoleMode in getset.cpp
// DereferenceIoHandle(obj, INPUT|OUTPUT, GENERIC_READ)
// Returns a->Mode with the handle's mode flags.
NTSTATUS Server::handleUserL1GetConsoleMode()
{
    auto& a = m_req.u.consoleMsgL1.GetConsoleMode;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_INPUT_HANDLE | CONSOLE_OUTPUT_HANDLE, GENERIC_READ);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    a.Mode = (h->handleType & CONSOLE_INPUT_HANDLE) ? m_inputMode : m_outputMode;
    return STATUS_SUCCESS;
}

// L1: SetConsoleMode
// OG: SrvSetConsoleMode in getset.cpp
// DereferenceIoHandle(obj, INPUT|OUTPUT, GENERIC_WRITE)
// Reads a->Mode, validates flags, applies to handle's buffer.
NTSTATUS Server::handleUserL1SetConsoleMode()
{
    auto& a = m_req.u.consoleMsgL1.SetConsoleMode;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_INPUT_HANDLE | CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    if (h->handleType & CONSOLE_INPUT_HANDLE)
    {
        m_inputMode = a.Mode;
    }
    else
    {
        const auto oldMode = m_outputMode;
        m_outputMode = a.Mode;

        // Emit DECAWM when the wrap-at-EOL flag changes.
        if ((oldMode ^ m_outputMode) & ENABLE_WRAP_AT_EOL_OUTPUT)
        {
            vtAppendFmt("\x1b[?7%c", (m_outputMode & ENABLE_WRAP_AT_EOL_OUTPUT) ? 'h' : 'l');
            vtFlush();
        }
    }

    return STATUS_SUCCESS;
}

// L1: GetNumberOfConsoleInputEvents
// OG: SrvGetConsoleNumberOfInputEvents in getset.cpp
// DereferenceIoHandle(obj, INPUT, GENERIC_READ)
// Returns a->ReadyEvents.
NTSTATUS Server::handleUserL1GetNumberOfConsoleInputEvents()
{
    auto& a = m_req.u.consoleMsgL1.GetNumberOfConsoleInputEvents;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_INPUT_HANDLE, GENERIC_READ);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    a.ReadyEvents = static_cast<ULONG>(m_input.pendingEventCount());
    return STATUS_SUCCESS;
}

// L1: GetConsoleInput (ReadConsoleInput / PeekConsoleInput)
// OG: SrvGetConsoleInput in directio.cpp
// DereferenceIoHandle(obj, INPUT, GENERIC_READ)
// Reads a->Flags (CONSOLE_READ_NOREMOVE = peek), a->Unicode.
// Writes INPUT_RECORD array to output buffer, returns a->NumRecords.
// Can block (ReplyPending) if no data and CONSOLE_READ_NOWAIT not set.
NTSTATUS Server::handleUserL1GetConsoleInput()
{
    auto& a = m_req.u.consoleMsgL1.GetConsoleInput;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_INPUT_HANDLE, GENERIC_READ);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    const bool peek = (a.Flags & CONSOLE_READ_NOREMOVE) != 0;
    const bool nowait = (a.Flags & CONSOLE_READ_NOWAIT) != 0;
    const auto outputOffset = m_req.msgHeader.ApiDescriptorSize;
    const auto maxOutputBytes = (m_req.Descriptor.OutputSize > outputOffset) ? (m_req.Descriptor.OutputSize - outputOffset) : 0u;
    const auto maxRecords = maxOutputBytes / sizeof(INPUT_RECORD);

    // Try to satisfy immediately.
    if (maxRecords > 0 && m_input.hasData())
    {
        std::vector<INPUT_RECORD> records(maxRecords);
        const auto n = m_input.readInputRecords(records.data(), maxRecords, peek);
        if (n > 0)
        {
            writeOutput(outputOffset, records.data(), static_cast<ULONG>(n * sizeof(INPUT_RECORD)));
            a.NumRecords = static_cast<ULONG>(n);

            if (!peek && !m_input.hasData())
                m_inputAvailableEvent.ResetEvent();

            return STATUS_SUCCESS;
        }
    }

    // No data. If NOWAIT or peek, return immediately with 0 records.
    if (nowait || peek)
    {
        a.NumRecords = 0;
        return STATUS_SUCCESS;
    }

    // Block: defer until input arrives.
    // Capture the ID and parameters for the retry lambda.
    const auto id = m_req.Descriptor.Identifier;
    return pendRead([id, outputOffset, maxRecords](Server& self) -> bool {
        if (!self.m_input.hasData())
            return false;

        std::vector<INPUT_RECORD> records(maxRecords);
        const auto n = self.m_input.readInputRecords(records.data(), maxRecords, false);
        if (n == 0)
            return false;

        // Write the records to the client's output buffer.
        CD_IO_OPERATION op{};
        op.Identifier = id;
        op.Buffer.Offset = outputOffset;
        op.Buffer.Data = records.data();
        op.Buffer.Size = static_cast<ULONG>(n * sizeof(INPUT_RECORD));
        THROW_IF_NTSTATUS_FAILED(self.ioctl(IOCTL_CONDRV_WRITE_OUTPUT, &op, sizeof(op), nullptr, 0));

        // Build the reply. We need to write back the API descriptor with NumRecords,
        // then complete the IO.
        CONSOLE_GETCONSOLEINPUT_MSG reply{};
        reply.NumRecords = static_cast<ULONG>(n);

        CD_IO_COMPLETE completion{};
        completion.Identifier = id;
        completion.IoStatus.Status = STATUS_SUCCESS;
        completion.Write.Data = &reply;
        completion.Write.Size = sizeof(reply);
        self.completeIo(completion);
        return true;
    });
}

// Helper: Try to read raw text from m_input, convert encoding, and write to
// the client's output buffer at `outputOffset`. Returns bytes written to client,
// or 0 if no data is available.
static ULONG tryReadRawTextToClient(
    Server& self,
    const LUID& identifier,
    ULONG outputOffset,
    ULONG maxOutputBytes,
    bool unicode,
    UINT inputCP)
{
    if (maxOutputBytes == 0 || !self.m_input.hasData())
        return 0;

    // Read up to maxOutputBytes of raw UTF-8 from the input buffer.
    std::vector<char> raw(maxOutputBytes);
    const auto n = self.m_input.readRawText(raw.data(), maxOutputBytes);
    if (n == 0)
        return 0;

    ULONG bytesWritten = 0;

    if (unicode)
    {
        // UTF-8 → UTF-16.
        const auto wideLen = MultiByteToWideChar(CP_UTF8, 0, raw.data(), static_cast<int>(n), nullptr, 0);
        if (wideLen > 0)
        {
            const auto maxChars = maxOutputBytes / sizeof(WCHAR);
            std::vector<wchar_t> wide(wideLen);
            MultiByteToWideChar(CP_UTF8, 0, raw.data(), static_cast<int>(n), wide.data(), wideLen);
            const auto outChars = std::min(static_cast<size_t>(wideLen), maxChars);
            bytesWritten = static_cast<ULONG>(outChars * sizeof(WCHAR));

            CD_IO_OPERATION op{};
            op.Identifier = identifier;
            op.Buffer.Offset = outputOffset;
            op.Buffer.Data = wide.data();
            op.Buffer.Size = bytesWritten;
            THROW_IF_NTSTATUS_FAILED(self.ioctl(IOCTL_CONDRV_WRITE_OUTPUT, &op, sizeof(op), nullptr, 0));
        }
    }
    else
    {
        // UTF-8 → UTF-16 → output code page.
        const auto wideLen = MultiByteToWideChar(CP_UTF8, 0, raw.data(), static_cast<int>(n), nullptr, 0);
        if (wideLen > 0)
        {
            std::vector<wchar_t> wide(wideLen);
            MultiByteToWideChar(CP_UTF8, 0, raw.data(), static_cast<int>(n), wide.data(), wideLen);
            const auto ansiLen = WideCharToMultiByte(inputCP, 0, wide.data(), wideLen, nullptr, 0, nullptr, nullptr);
            if (ansiLen > 0)
            {
                const auto outBytes = std::min(static_cast<size_t>(ansiLen), static_cast<size_t>(maxOutputBytes));
                std::vector<char> ansi(ansiLen);
                WideCharToMultiByte(inputCP, 0, wide.data(), wideLen, ansi.data(), ansiLen, nullptr, nullptr);
                bytesWritten = static_cast<ULONG>(outBytes);

                CD_IO_OPERATION op{};
                op.Identifier = identifier;
                op.Buffer.Offset = outputOffset;
                op.Buffer.Data = ansi.data();
                op.Buffer.Size = bytesWritten;
                THROW_IF_NTSTATUS_FAILED(self.ioctl(IOCTL_CONDRV_WRITE_OUTPUT, &op, sizeof(op), nullptr, 0));
            }
        }
    }

    return bytesWritten;
}

// L1: ReadConsole
// OG: SrvReadConsole in stream.cpp
// DereferenceIoHandle(obj, INPUT, GENERIC_READ)
// Reads a->Unicode, a->ProcessControlZ, a->ExeNameLength, a->InitialNumBytes, a->CtrlWakeupMask.
// Writes text to output buffer (GetAugmentedOutputBuffer, factor=2 for ANSI->Unicode).
// Returns a->NumBytes, a->ControlKeyState.
// Can block (ReplyPending) waiting for user input.
NTSTATUS Server::handleUserL1ReadConsole()
{
    auto& a = m_req.u.consoleMsgL1.ReadConsole;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_INPUT_HANDLE, GENERIC_READ);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Implement cooked read (line editing) when ENABLE_LINE_INPUT is set.
    // For now, return raw text from the input buffer.
    const auto outputOffset = m_req.msgHeader.ApiDescriptorSize;
    const auto maxOutputBytes = (m_req.Descriptor.OutputSize > outputOffset) ? (m_req.Descriptor.OutputSize - outputOffset) : 0u;
    const bool unicode = a.Unicode != 0;
    const auto inputCP = m_inputCP;

    // Try to satisfy immediately.
    const auto bytesWritten = tryReadRawTextToClient(
        *this, m_req.Descriptor.Identifier, outputOffset, maxOutputBytes, unicode, inputCP);

    if (bytesWritten > 0)
    {
        a.NumBytes = bytesWritten;
        a.ControlKeyState = 0;

        if (!m_input.hasData())
            m_inputAvailableEvent.ResetEvent();

        return STATUS_SUCCESS;
    }

    // No data — block until input arrives.
    const auto id = m_req.Descriptor.Identifier;
    return pendRead([id, outputOffset, maxOutputBytes, unicode, inputCP](Server& self) -> bool {
        const auto bytesWritten = tryReadRawTextToClient(
            self, id, outputOffset, maxOutputBytes, unicode, inputCP);

        if (bytesWritten == 0)
            return false; // Still no data.

        // Build reply with NumBytes.
        CONSOLE_READCONSOLE_MSG reply{};
        reply.NumBytes = bytesWritten;
        reply.ControlKeyState = 0;
        reply.Unicode = unicode ? TRUE : FALSE;

        CD_IO_COMPLETE completion{};
        completion.Identifier = id;
        completion.IoStatus.Status = STATUS_SUCCESS;
        completion.Write.Data = &reply;
        completion.Write.Size = sizeof(reply);
        self.completeIo(completion);
        return true;
    });
}

// Helper: Convert payload to UTF-8 and write to VT output.
static void writePayloadAsVt(Server& self, const std::vector<uint8_t>& payload, bool unicode, UINT outputCP)
{
    if (unicode)
    {
        self.vtAppendUTF16({ reinterpret_cast<const wchar_t*>(payload.data()), payload.size() / sizeof(wchar_t) });
    }
    else
    {
        const auto len = static_cast<int>(payload.size());
        if (len > 0)
        {
            const auto wideLen = MultiByteToWideChar(outputCP, 0, reinterpret_cast<const char*>(payload.data()), len, nullptr, 0);
            if (wideLen > 0)
            {
                std::wstring wide(wideLen, L'\0');
                MultiByteToWideChar(outputCP, 0, reinterpret_cast<const char*>(payload.data()), len, wide.data(), wideLen);
                self.vtAppendUTF16(wide);
            }
        }
    }
    self.vtFlush();
}

// L1: WriteConsole
// OG: SrvWriteConsole in stream.cpp → DoWriteConsole in _stream.cpp.
// If output is paused (CONSOLE_SUSPENDED), the write is deferred on the
// OutputQueue. When output is unpaused, UnblockWriteConsole wakes all
// pending writes. See also handleRawWrite for the same pattern.
NTSTATUS Server::handleUserL1WriteConsole()
{
    auto& a = m_req.u.consoleMsgL1.WriteConsole;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    auto payload = std::make_shared<std::vector<uint8_t>>(readTrailingInput());
    const auto payloadSize = static_cast<ULONG>(payload->size());
    const bool unicode = a.Unicode != 0;
    const auto outputCP = m_outputCP;

    if (m_outputPaused)
    {
        const auto id = m_req.Descriptor.Identifier;
        return pendWrite([payload, payloadSize, unicode, outputCP, id](Server& self) -> bool {
            if (self.m_outputPaused)
                return false;

            writePayloadAsVt(self, *payload, unicode, outputCP);

            CONSOLE_WRITECONSOLE_MSG reply{};
            reply.NumBytes = payloadSize;
            reply.Unicode = unicode ? TRUE : FALSE;

            CD_IO_COMPLETE completion{};
            completion.Identifier = id;
            completion.IoStatus.Status = STATUS_SUCCESS;
            completion.Write.Data = &reply;
            completion.Write.Size = sizeof(reply);
            self.completeIo(completion);
            return true;
        });
    }

    writePayloadAsVt(*this, *payload, unicode, outputCP);
    a.NumBytes = payloadSize;
    return STATUS_SUCCESS;
}

// L1: GetConsoleLangId
// OG: SrvGetConsoleLangId in srvinit.cpp — no handle validation.
// Returns a->LangId based on output code page.
NTSTATUS Server::handleUserL1GetConsoleLangId()
{
    auto& a = m_req.u.consoleMsgL1.GetConsoleLangId;

    // TODO: Derive from actual output code page.
    a.LangId = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
    return STATUS_SUCCESS;
}

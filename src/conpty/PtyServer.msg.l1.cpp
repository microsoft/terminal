#include "pch.h"
#include "PtyServer.h"

// L1: GetConsoleCP / GetConsoleOutputCP
// OG: SrvGetConsoleCP in getset.cpp — no handle validation.
// Reads a->Output to decide input vs output CP, returns a->CodePage.
NTSTATUS PtyServer::handleUserL1GetConsoleCP()
{
    auto& a = m_req.u.consoleMsgL1.GetConsoleCP;
    a.CodePage = a.Output ? m_outputCP : m_inputCP;
    return STATUS_SUCCESS;
}

// L1: GetConsoleMode
// OG: SrvGetConsoleMode in getset.cpp
// DereferenceIoHandle(obj, INPUT|OUTPUT, GENERIC_READ)
// Returns a->Mode with the handle's mode flags.
NTSTATUS PtyServer::handleUserL1GetConsoleMode()
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
NTSTATUS PtyServer::handleUserL1SetConsoleMode()
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
NTSTATUS PtyServer::handleUserL1GetNumberOfConsoleInputEvents()
{
    auto& a = m_req.u.consoleMsgL1.GetNumberOfConsoleInputEvents;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_INPUT_HANDLE, GENERIC_READ);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Return actual count from input queue.
    a.ReadyEvents = 0;
    return STATUS_SUCCESS;
}

// L1: GetConsoleInput (ReadConsoleInput / PeekConsoleInput)
// OG: SrvGetConsoleInput in directio.cpp
// DereferenceIoHandle(obj, INPUT, GENERIC_READ)
// Reads a->Flags (CONSOLE_READ_NOREMOVE = peek), a->Unicode.
// Writes INPUT_RECORD array to output buffer, returns a->NumRecords.
// Can block (ReplyPending) if no data and CONSOLE_READ_NOWAIT not set.
NTSTATUS PtyServer::handleUserL1GetConsoleInput()
{
    auto& a = m_req.u.consoleMsgL1.GetConsoleInput;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_INPUT_HANDLE, GENERIC_READ);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Read events from input queue, optionally peek (CONSOLE_READ_NOREMOVE).
    //       Write INPUT_RECORD array via writeOutput().
    //       May need to defer (return STATUS_NO_RESPONSE) if blocking.
    a.NumRecords = 0;
    return STATUS_SUCCESS;
}

// L1: ReadConsole
// OG: SrvReadConsole in stream.cpp
// DereferenceIoHandle(obj, INPUT, GENERIC_READ)
// Reads a->Unicode, a->ProcessControlZ, a->ExeNameLength, a->InitialNumBytes, a->CtrlWakeupMask.
// Writes text to output buffer (GetAugmentedOutputBuffer, factor=2 for ANSI->Unicode).
// Returns a->NumBytes, a->ControlKeyState.
// Can block (ReplyPending) waiting for user input.
NTSTATUS PtyServer::handleUserL1ReadConsole()
{
    auto& a = m_req.u.consoleMsgL1.ReadConsole;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_INPUT_HANDLE, GENERIC_READ);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    // TODO: Read cooked/raw text from input buffer.
    //       Write text via writeOutput().
    //       May need to defer (return STATUS_NO_RESPONSE) if blocking.
    a.NumBytes = 0;
    a.ControlKeyState = 0;
    return STATUS_SUCCESS;
}

// L1: WriteConsole
// OG: SrvWriteConsole in stream.cpp
// DereferenceIoHandle(obj, OUTPUT, GENERIC_WRITE)
// Reads a->Unicode, trailing input payload = text to write.
// Returns a->NumBytes (bytes actually written).
// Can block (ReplyPending) if output is suspended.
NTSTATUS PtyServer::handleUserL1WriteConsole()
{
    auto& a = m_req.u.consoleMsgL1.WriteConsole;

    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_OUTPUT_HANDLE, GENERIC_WRITE);
    if (!h)
    {
        return STATUS_INVALID_HANDLE;
    }

    auto payload = readTrailingInput();

    if (a.Unicode)
    {
        vtAppendUTF16({ reinterpret_cast<const wchar_t*>(payload.data()), payload.size() / sizeof(wchar_t) });
    }
    else
    {
        // Convert from the current output code page to UTF-16, then to UTF-8.
        const auto len = static_cast<int>(payload.size());
        if (len > 0)
        {
            const auto wideLen = MultiByteToWideChar(m_outputCP, 0, reinterpret_cast<const char*>(payload.data()), len, nullptr, 0);
            if (wideLen > 0)
            {
                std::wstring wide(wideLen, L'\0');
                MultiByteToWideChar(m_outputCP, 0, reinterpret_cast<const char*>(payload.data()), len, wide.data(), wideLen);
                vtAppendUTF16(wide);
            }
        }
    }

    vtFlush();
    a.NumBytes = static_cast<ULONG>(payload.size());
    return STATUS_SUCCESS;
}

// L1: GetConsoleLangId
// OG: SrvGetConsoleLangId in srvinit.cpp — no handle validation.
// Returns a->LangId based on output code page.
NTSTATUS PtyServer::handleUserL1GetConsoleLangId()
{
    auto& a = m_req.u.consoleMsgL1.GetConsoleLangId;

    // TODO: Derive from actual output code page.
    a.LangId = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
    return STATUS_SUCCESS;
}

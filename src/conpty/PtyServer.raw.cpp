#include "pch.h"
#include "PtyServer.h"

// Handles CONSOLE_IO_RAW_WRITE messages.
//
// Protocol (from OG ConsoleIoThread RAW_WRITE case + SrvWriteConsole in stream.cpp):
//  1. The client's write data is available via readInput (IOCTL_CONDRV_READ_INPUT).
//     Descriptor.InputSize tells us how many bytes are available.
//  2. In the OG, SrvWriteConsole calls GetInputBuffer() to pull all client data,
//     then DoSrvWriteConsole() feeds it to the output renderer.
//  3. If output is paused (e.g. Pause key), the write is deferred: we save the
//     message and complete it later when output is resumed. This is the
//     ReplyPending mechanism from the OG.
//
// Returns true if the reply is pending (caller must NOT reply inline).
// Returns false if the write completed immediately (caller replies inline).
NTSTATUS PtyServer::handleRawWrite()
{
    const auto size = m_req.Descriptor.InputSize;

    // Read the client's write payload from the driver upfront, regardless
    // of whether we can process it now. The driver expects us to consume it.
    std::vector<uint8_t> buffer(size);
    if (size > 0)
    {
        readInput(0, buffer.data(), size);
    }

    // If output is paused, defer this write. The OG creates a wait block
    // that gets signaled when output is resumed (ConsoleNotifyWait).
    if (m_outputPaused)
    {
        PendingIO pending;
        pending.identifier = m_req.Descriptor.Identifier;
        pending.process = m_req.Descriptor.Process;
        pending.object = m_req.Descriptor.Object;
        pending.function = CONSOLE_IO_RAW_WRITE;
        pending.inputData = std::move(buffer);
        m_pendingWrites.push_back(std::move(pending));
        return STATUS_NO_RESPONSE;
    }

    m_host->HandleUTF8Output((const char*)buffer.data(), buffer.size());
    return STATUS_SUCCESS;
}

// Handles CONSOLE_IO_RAW_READ messages.
//
// Protocol (from OG ConsoleIoThread RAW_READ case + SrvReadConsole in stream.cpp):
//  1. Descriptor.OutputSize tells us the max bytes the client wants to read.
//  2. In the OG, SrvReadConsole calls GetAugmentedOutputBuffer() to allocate a
//     server-side buffer, then ReadChars() fills it with input data.
//  3. If the input queue is empty, the OG returns CONSOLE_STATUS_WAIT and
//     creates a wait block on the input buffer's ReadWaitQueue. When input
//     arrives, ConsoleNotifyWait completes the pending read.
//  4. On success, ReleaseMessageBuffers() writes the output buffer back via
//     IOCTL_CONDRV_WRITE_OUTPUT, and IoStatus.Information is set to bytes read.
//
// Returns true if the reply is pending (caller must NOT reply inline).
// Returns false if the read completed immediately.
NTSTATUS PtyServer::handleRawRead()
{
    const auto maxBytes = m_req.Descriptor.OutputSize;

    // TODO: Try to read data from the input queue.
    // For now, we always pend — there's no input source yet.
    // When input data becomes available, call completePendingRead().

    PendingIO pending;
    pending.identifier = m_req.Descriptor.Identifier;
    pending.process = m_req.Descriptor.Process;
    pending.object = m_req.Descriptor.Object;
    pending.function = CONSOLE_IO_RAW_READ;
    pending.outputSize = maxBytes;
    m_pendingReads.push_back(std::move(pending));
    return STATUS_NO_RESPONSE;
}

// Handles CONSOLE_IO_RAW_FLUSH messages.
//
// Protocol (from OG SrvFlushConsoleInputBuffer in getset.cpp + FlushInputBuffer in input.cpp):
//  1. Descriptor.Object contains the handle to flush.
//  2. DereferenceIoHandle validates it is a CONSOLE_INPUT_HANDLE with GENERIC_WRITE access.
//  3. FlushInputBuffer resets the circular buffer (In = Out = InputBuffer)
//     and resets the InputWaitEvent.
//
// In the prototype there is no circular input buffer yet, so we just
// validate the handle and reset the input-available event.
NTSTATUS PtyServer::handleRawFlush()
{
    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_INPUT_HANDLE, GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    // TODO: Clear the input event queue when one is implemented.

    // Reset the input-available event, matching the OG ResetEvent(pInputInfo->InputWaitEvent).
    m_inputAvailableEvent.ResetEvent();
    return STATUS_SUCCESS;
}

// Completes the oldest pending read with the given data.
// Called when input data becomes available (e.g. from the terminal's input pipeline).
//
// In the OG, this is analogous to ConsoleNotifyWait on the ReadWaitQueue,
// which re-invokes the read routine and, on success, calls ReleaseMessageBuffers
// to write the output data back to the client via IOCTL_CONDRV_WRITE_OUTPUT.
void PtyServer::completePendingRead(const void* data, ULONG size)
{
    if (m_pendingReads.empty())
    {
        return;
    }

    auto pending = std::move(m_pendingReads.front());
    m_pendingReads.pop_front();

    auto bytesToWrite = std::min(size, pending.outputSize);

    // Write the data back to the client's read buffer.
    if (bytesToWrite > 0)
    {
        CD_IO_OPERATION op{};
        op.Identifier = pending.identifier;
        op.Buffer.Offset = 0;
        op.Buffer.Data = const_cast<void*>(data);
        op.Buffer.Size = bytesToWrite;
        THROW_IF_NTSTATUS_FAILED(ioctl(IOCTL_CONDRV_WRITE_OUTPUT, &op, sizeof(op), nullptr, 0));
    }

    // Complete the read with the number of bytes returned.
    CD_IO_COMPLETE completion{};
    completion.Identifier = pending.identifier;
    completion.IoStatus.Status = STATUS_SUCCESS;
    completion.IoStatus.Information = bytesToWrite;

    completeIo(completion);
}

// Completes all pending writes (called when output is unpaused).
//
// In the OG, this is analogous to ConsoleNotifyWait on the OutputQueue,
// which re-invokes DoSrvWriteConsole for each deferred write.
void PtyServer::completePendingWrites()
{
    while (!m_pendingWrites.empty())
    {
        auto pending = std::move(m_pendingWrites.front());
        m_pendingWrites.pop_front();

        // TODO: Feed pending.inputData to the output/rendering pipeline.

        // Complete the write.
        CD_IO_COMPLETE completion{};
        completion.Identifier = pending.identifier;
        completion.IoStatus.Status = STATUS_SUCCESS;
        completion.IoStatus.Information = 0;

        completeIo(completion);
    }
}

// Cancels all pending IOs for a disconnecting process.
//
// In the OG, FreeProcessData iterates the WaitBlockQueue and calls
// ConsoleNotifyWaitBlock with fThreadDying=TRUE for each pending wait.
// The wait routines then complete the IO with an error status.
void PtyServer::cancelPendingIOs(ULONG_PTR process)
{
    // Cancel pending reads from this process.
    while (true)
    {
        auto it = std::find_if(m_pendingReads.begin(), m_pendingReads.end(),
                               [process](const PendingIO& p) { return p.process == process; });
        if (it == m_pendingReads.end())
            break;

        CD_IO_COMPLETE completion{};
        completion.Identifier = it->identifier;
        completion.IoStatus.Status = STATUS_CANCELLED;
        completion.IoStatus.Information = 0;

        // Best-effort: if the process is gone, the driver may reject this.
        ioctl(IOCTL_CONDRV_COMPLETE_IO, &completion, sizeof(completion), nullptr, 0);
        m_pendingReads.erase(it);
    }

    // Cancel pending writes from this process.
    while (true)
    {
        auto it = std::find_if(m_pendingWrites.begin(), m_pendingWrites.end(),
                               [process](const PendingIO& p) { return p.process == process; });
        if (it == m_pendingWrites.end())
            break;

        CD_IO_COMPLETE completion{};
        completion.Identifier = it->identifier;
        completion.IoStatus.Status = STATUS_CANCELLED;
        completion.IoStatus.Information = 0;

        ioctl(IOCTL_CONDRV_COMPLETE_IO, &completion, sizeof(completion), nullptr, 0);
        m_pendingWrites.erase(it);
    }
}

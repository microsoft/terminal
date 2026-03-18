#include "pch.h"
#include "Server.h"

// ============================================================================
// Pending IO infrastructure
// ============================================================================

// Queue a read that will be retried when new input arrives.
NTSTATUS Server::pendRead(std::function<bool(Server&)> retry)
{
    PendingIO pending;
    pending.identifier = m_req.Descriptor.Identifier;
    pending.process = m_req.Descriptor.Process;
    pending.retry = std::move(retry);
    m_pendingReads.push_back(std::move(pending));
    return STATUS_NO_RESPONSE;
}

// Queue a write that will be retried when output is unpaused.
NTSTATUS Server::pendWrite(std::function<bool(Server&)> retry)
{
    PendingIO pending;
    pending.identifier = m_req.Descriptor.Identifier;
    pending.process = m_req.Descriptor.Process;
    pending.retry = std::move(retry);
    m_pendingWrites.push_back(std::move(pending));
    return STATUS_NO_RESPONSE;
}

// Send a completion for a specific pending IO.
void Server::completePendingIo(const LUID& identifier, NTSTATUS status, ULONG_PTR information)
{
    CD_IO_COMPLETE completion{};
    completion.Identifier = identifier;
    completion.IoStatus.Status = status;
    completion.IoStatus.Information = information;
    completeIo(completion);
}

// Walk the pending-read queue and retry each. Remove any that succeed.
// Called when new input arrives (WriteInput, WriteConsoleInput, etc.).
//
// Analogous to OG WakeUpReadersWaitingForData → ConsoleNotifyWait.
// In the OG, only ONE reader is woken per input arrival (fSatisfyAll=FALSE).
// We follow the same pattern: stop after the first successful retry.
void Server::drainPendingInputReads()
{
    auto it = m_pendingReads.begin();
    while (it != m_pendingReads.end())
    {
        if (it->retry(*this))
        {
            it = m_pendingReads.erase(it);
            break; // OG: only satisfy one reader at a time.
        }
        else
        {
            ++it;
        }
    }

    if (!m_input.hasData())
        m_inputAvailableEvent.ResetEvent();
}

// Walk the pending-write queue and retry each. Remove any that succeed.
// Called when output is unpaused.
//
// Analogous to OG UnblockWriteConsole → ConsoleNotifyWait(OutputQueue, TRUE).
// In the OG, ALL pending writes are retried (fSatisfyAll=TRUE).
void Server::drainPendingOutputWrites()
{
    auto it = m_pendingWrites.begin();
    while (it != m_pendingWrites.end())
    {
        if (it->retry(*this))
        {
            it = m_pendingWrites.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// Cancel all pending IOs for a disconnecting process.
//
// Analogous to OG FreeProcessData which calls ConsoleNotifyWaitBlock
// with fThreadDying=TRUE for each wait block owned by the process.
void Server::cancelPendingIOs(ULONG_PTR process)
{
    auto cancelMatching = [&](std::deque<PendingIO>& queue) {
        auto it = queue.begin();
        while (it != queue.end())
        {
            if (it->process == process)
            {
                // Best-effort: complete with STATUS_CANCELLED.
                CD_IO_COMPLETE completion{};
                completion.Identifier = it->identifier;
                completion.IoStatus.Status = STATUS_CANCELLED;
                ioctl(IOCTL_CONDRV_COMPLETE_IO, &completion, sizeof(completion), nullptr, 0);
                it = queue.erase(it);
            }
            else
            {
                ++it;
            }
        }
    };

    cancelMatching(m_pendingReads);
    cancelMatching(m_pendingWrites);
}

// ============================================================================
// Raw IO handlers
// ============================================================================

// Handles CONSOLE_IO_RAW_WRITE.
//
// OG path: ConsoleIoThread RAW_WRITE → SrvWriteConsole → DoWriteConsole.
// If output is paused (CONSOLE_SUSPENDED), the write is deferred on the
// OutputQueue and retried when UnblockWriteConsole is called.
NTSTATUS Server::handleRawWrite()
{
    const auto size = m_req.Descriptor.InputSize;

    // Read the payload upfront — the driver expects us to consume it.
    auto buffer = std::make_shared<std::vector<uint8_t>>(size);
    if (size > 0)
    {
        readInput(0, buffer->data(), size);
    }

    if (m_outputPaused)
    {
        // Capture the data and identifier for the retry lambda.
        const auto id = m_req.Descriptor.Identifier;
        return pendWrite([buffer, id](Server& self) -> bool {
            if (self.m_outputPaused)
                return false; // Still paused.

            self.m_host->WriteUTF8({ reinterpret_cast<const char*>(buffer->data()), buffer->size() });

            self.completePendingIo(id, STATUS_SUCCESS, static_cast<ULONG_PTR>(buffer->size()));
            return true;
        });
    }

    m_host->WriteUTF8({ reinterpret_cast<const char*>(buffer->data()), buffer->size() });
    return STATUS_SUCCESS;
}

// Handles CONSOLE_IO_RAW_READ.
//
// OG path: ConsoleIoThread RAW_READ → SrvReadConsole → ReadChars → GetChar
// → ReadInputBuffer. If the input buffer is empty, a wait block is created
// on the ReadWaitQueue. When input arrives, WakeUpReadersWaitingForData
// walks the queue and retries each pending read.
NTSTATUS Server::handleRawRead()
{
    const auto maxBytes = m_req.Descriptor.OutputSize;

    // Try to satisfy immediately.
    if (m_input.hasData() && maxBytes > 0)
    {
        std::vector<char> buf(maxBytes);
        const auto n = m_input.readRawText(buf.data(), maxBytes);
        if (n > 0)
        {
            writeOutput(0, buf.data(), static_cast<ULONG>(n));

            if (!m_input.hasData())
                m_inputAvailableEvent.ResetEvent();

            // Complete out-of-band (the response carries write data).
            completePendingIo(m_req.Descriptor.Identifier, STATUS_SUCCESS, static_cast<ULONG_PTR>(n));
            return STATUS_NO_RESPONSE;
        }
    }

    // No data — defer until input arrives.
    const auto id = m_req.Descriptor.Identifier;
    return pendRead([maxBytes, id](Server& self) -> bool {
        if (!self.m_input.hasData())
            return false;

        std::vector<char> buf(maxBytes);
        const auto n = self.m_input.readRawText(buf.data(), maxBytes);
        if (n == 0)
            return false;

        // Write data back to the client's read buffer.
        CD_IO_OPERATION op{};
        op.Identifier = id;
        op.Buffer.Offset = 0;
        op.Buffer.Data = buf.data();
        op.Buffer.Size = static_cast<ULONG>(n);
        THROW_IF_NTSTATUS_FAILED(self.ioctl(IOCTL_CONDRV_WRITE_OUTPUT, &op, sizeof(op), nullptr, 0));

        self.completePendingIo(id, STATUS_SUCCESS, static_cast<ULONG_PTR>(n));
        return true;
    });
}

// Handles CONSOLE_IO_RAW_FLUSH.
//
// OG path: SrvFlushConsoleInputBuffer → FlushInputBuffer.
// Clears the input buffer and resets the input-available event.
NTSTATUS Server::handleRawFlush()
{
    auto* h = findHandle(m_req.Descriptor.Object, CONSOLE_INPUT_HANDLE, GENERIC_WRITE);
    if (!h)
        return STATUS_INVALID_HANDLE;

    m_input.flush();
    m_inputAvailableEvent.ResetEvent();
    return STATUS_SUCCESS;
}

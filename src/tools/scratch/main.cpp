// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <thread>
#include <sstream>

#include <wil\result.h>
#include <wil\resource.h>
#include <wil\com.h>

#include <evntcons.h>
#include <evntrace.h>

namespace
{
    wil::unique_event g_stopEvent{ wil::EventOptions::None };

} // namespace anonymous

void WINAPI ProcessEtwEvent(_In_ PEVENT_RECORD rawEvent)
{
    // This is the task id from srh.man (with the same name).
    constexpr auto InitiateSpeaking = 5;

    auto data = rawEvent->UserData;
    auto dataLen = rawEvent->UserDataLength;
    auto processId = rawEvent->EventHeader.ProcessId;
    auto task = rawEvent->EventHeader.EventDescriptor.Task;

    if (task == InitiateSpeaking)
    {
        // The payload first has an int32 representing the channel we're writing to. This is basically always writing to the
        // default channel, so just skip over those bytes... the rest is the string payload we want to speak,
        // as a null terminated string.
        if (dataLen <= 4)
        {
            return;
        }

        const auto stringPayloadSize = (dataLen - 4) / sizeof(wchar_t);
        // We don't need the null terminator, because wstring intends to handle that on its own.
        const auto stringLen = stringPayloadSize - 1;
        const auto payload = std::wstring(reinterpret_cast<wchar_t*>(static_cast<char*>(data) + 4), stringLen);

        wprintf(L"[Narrator pid=%d]: %s\n", processId, payload.c_str());
        fflush(stdout);

        if (payload == L"Exiting Narrator")
        {
            g_stopEvent.SetEvent();
        }
    }
}

int __cdecl wmain(int argc, wchar_t* argv[])
{
    const bool runForever = (argc == 2) &&
                            std::wstring{ argv[1] } == L"-forever";

    GUID sessionGuid;
    FAIL_FAST_IF_FAILED(::CoCreateGuid(&sessionGuid));

    std::array<wchar_t, 64> traceSessionName{};
    FAIL_FAST_IF_FAILED(StringCchPrintf(traceSessionName.data(), static_cast<DWORD>(traceSessionName.size()), L"NarratorTraceSession_%d", ::GetCurrentProcessId()));

    unsigned int traceSessionNameBytes = static_cast<unsigned int>((wcslen(traceSessionName.data()) + 1) * sizeof(wchar_t));

    // Now, to get tracing. Most settings below are defaults from MSDN, except where noted.
    // First, set up a session (StartTrace) - which requires a PROPERTIES struct. This has to have
    // the session name after it in the same block of memory...
    const unsigned int propertiesByteSize = sizeof(EVENT_TRACE_PROPERTIES) + traceSessionNameBytes;
    std::vector<char> eventTracePropertiesBuffer(propertiesByteSize);
    auto properties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(eventTracePropertiesBuffer.data());

    // Set up properties struct for a real-time session...
    properties->Wnode.BufferSize = propertiesByteSize;
    properties->Wnode.Guid = sessionGuid;
    properties->Wnode.ClientContext = 1;
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_USE_PAGED_MEMORY;
    properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    properties->FlushTimer = 1;
    // Finally, copy the session name...
    memcpy(properties + 1, traceSessionName.data(), traceSessionNameBytes);

    std::thread traceThread;
    auto joinTraceThread = wil::scope_exit([&]() {
        if (traceThread.joinable())
        {
            traceThread.join();
        }
    });

    TRACEHANDLE session{};
    const auto rc = ::StartTrace(&session, traceSessionName.data(), properties);
    FAIL_FAST_IF(rc != ERROR_SUCCESS);
    auto stopTrace = wil::scope_exit([&]() {
        EVENT_TRACE_PROPERTIES properties{};
        properties.Wnode.BufferSize = sizeof(properties);
        properties.Wnode.Guid = sessionGuid;
        properties.Wnode.Flags = WNODE_FLAG_TRACED_GUID;

        ::ControlTrace(session, nullptr, &properties, EVENT_TRACE_CONTROL_STOP);
    });

    constexpr GUID narratorProviderGuid = { 0x835b79e2, 0xe76a, 0x44c4, 0x98, 0x85, 0x26, 0xad, 0x12, 0x2d, 0x3b, 0x4d };
    FAIL_FAST_IF(ERROR_SUCCESS != ::EnableTrace(TRUE /* enable */, 0 /* enableFlag */, TRACE_LEVEL_VERBOSE, &narratorProviderGuid, session));
    auto disableTrace = wil::scope_exit([&]() {
        ::EnableTrace(FALSE /* enable */, 0 /* enableFlag */, TRACE_LEVEL_VERBOSE, &narratorProviderGuid, session);
    });

    // Finally, start listening (OpenTrace/ProcessTrace/CloseTrace)...
    EVENT_TRACE_LOGFILE trace{};
    trace.LoggerName = traceSessionName.data();
    trace.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_REAL_TIME;
    trace.EventRecordCallback = ProcessEtwEvent;

    using unique_tracehandle = wil::unique_any<TRACEHANDLE, decltype(::CloseTrace), ::CloseTrace>;
    unique_tracehandle traceHandle{ ::OpenTrace(&trace) };

    // Since the actual call to ProcessTrace blocks while it's working,
    // we spin up a separate thread to do that.
    traceThread = std::thread([traceHandle(std::move(traceHandle))]() mutable {
        ::ProcessTrace(traceHandle.addressof(), 1 /* handleCount */, nullptr /* startTime */, nullptr /* endTime */);
    });

    if (runForever)
    {
        ::Sleep(INFINITE);
    }
    else
    {
        g_stopEvent.wait(INFINITE);
    }

    return 0;
}

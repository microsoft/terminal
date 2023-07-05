// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

/*
Module Name:
- tracing.hpp

Abstract:
- This module is used for recording tracing/debugging information to the telemetry ETW channel
- The data is not automatically broadcast to telemetry backends.
- NOTE: Many functions in this file appear to be copy/pastes. This is because the TraceLog documentation warns
        to not be "cute" in trying to reduce its macro usages with variables as it can cause unexpected behavior.
*/

#pragma once

// Including TraceLogging essentials for the binary
#include <winmeta.h>
#include <TraceLoggingProvider.h>

TRACELOGGING_DECLARE_PROVIDER(g_hConsoleVirtTermParserEventTraceProvider);

namespace Microsoft::Console::VirtualTerminal
{
    class ParserTracing sealed
    {
    public:
        // NOTE: This code uses
        //   (_In_z_ const wchar_t* name)
        // as arguments instead of the more modern std::wstring_view
        // for performance reasons.
        //
        // Passing structures larger than the register size is very expensive
        // due to Microsoft's x64 calling convention. We could reduce the
        // overhead by passing the string-view by reference, but this forces us
        // to allocate the parameters as static string-views on the data
        // segment of our binary. I've found that passing them as classic
        // C-strings is more ergonomic instead and fits the need for
        // high performance in this particular code.

        void TraceStateChange(_In_z_ const wchar_t* name) const noexcept;
        void TraceOnAction(_In_z_ const wchar_t* name) const noexcept;
        void TraceOnExecute(const wchar_t wch) const noexcept;
        void TraceOnExecuteFromEscape(const wchar_t wch) const noexcept;
        void TraceOnEvent(_In_z_ const wchar_t* name) const noexcept;
        void TraceCharInput(const wchar_t wch);

        void AddSequenceTrace(const wchar_t wch);
        void DispatchSequenceTrace(const bool fSuccess) noexcept;
        void ClearSequenceTrace() noexcept;
        void DispatchPrintRunTrace(const std::wstring_view& string) const;

    private:
        std::wstring _sequenceTrace;
    };
}

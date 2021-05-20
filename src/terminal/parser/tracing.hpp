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

#include "telemetry.hpp"

namespace Microsoft::Console::VirtualTerminal
{
    class ParserTracing sealed
    {
    public:
        ParserTracing() noexcept;

        void TraceStateChange(const std::wstring_view name) const noexcept;
        void TraceOnAction(const std::wstring_view name) const noexcept;
        void TraceOnExecute(const wchar_t wch) const noexcept;
        void TraceOnExecuteFromEscape(const wchar_t wch) const noexcept;
        void TraceOnEvent(const std::wstring_view name) const noexcept;
        void TraceCharInput(const wchar_t wch);

        void AddSequenceTrace(const wchar_t wch);
        void DispatchSequenceTrace(const bool fSuccess) noexcept;
        void ClearSequenceTrace() noexcept;
        void DispatchPrintRunTrace(const std::wstring_view string) const;

    private:
        std::wstring _sequenceTrace;
    };
}

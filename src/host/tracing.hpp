/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- tracing.hpp

Abstract:
- This module is used for recording tracing/debugging information to the telemetry ETW channel
- The data is not automatically broadcast to telemetry backends as it does not set the TELEMETRY keyword.
- NOTE: Many functions in this file appear to be copy/pastes. This is because the TraceLog documentation warns
        to not be "cute" in trying to reduce its macro usages with variables as it can cause unexpected behavior.

Author(s):
- Michael Niksa (miniksa)     25-Nov-2014
--*/

#pragma once

#include "../types/inc/Viewport.hpp"

#define TraceLoggingConsoleCoord(value, name)        \
    TraceLoggingPackedData(&value, sizeof(COORD)),   \
        TraceLoggingPackedStruct(2, name),           \
        TraceLoggingPackedMetadata(TlgInINT16, "X"), \
        TraceLoggingPackedMetadata(TlgInINT16, "Y")

#define TraceLoggingConsoleSmallRect(value, name)       \
    TraceLoggingPackedData(&value, sizeof(SMALL_RECT)), \
        TraceLoggingPackedStruct(4, name),              \
        TraceLoggingInt16(TlgInINT16, "Left"),          \
        TraceLoggingInt16(TlgInINT16, "Top"),           \
        TraceLoggingInt16(TlgInINT16, "Right"),         \
        TraceLoggingInt16(TlgInINT16, "Bottom")

// We intentionally don't differentiate between A and W versions of CHAR_INFO, because some particularly nasty
// applications smuggle data in the upper bytes of the UnicodeChar field while using the A APIs and then they
// expect to read the same values back at a later time, which is something we stopped supporting.
#define TraceLoggingConsoleCharInfo(value, name)           \
    TraceLoggingStruct(2, name),                           \
        TraceLoggingInt16(value.Char.UnicodeChar, "Char"), \
        TraceLoggingInt16(value.Attributes, "Attributes")

class Tracing
{
public:
    static void s_TraceWindowViewport(const Microsoft::Console::Types::Viewport& viewport);

    static void s_TraceChars(_In_z_ const char* pszMessage, ...);
    static void s_TraceOutput(_In_z_ const char* pszMessage, ...);

    static void s_TraceWindowMessage(const MSG& msg);
    static void s_TraceInputRecord(const INPUT_RECORD& inputRecord);

    static void s_TraceCookedRead(_In_ ConsoleProcessHandle* const pConsoleProcessHandle, const std::wstring_view& text);
    static void s_TraceConsoleAttachDetach(_In_ ConsoleProcessHandle* const pConsoleProcessHandle, _In_ bool bIsAttach);

    static void __stdcall TraceFailure(const wil::FailureInfo& failure) noexcept;

private:
    static ULONG s_ulDebugFlag;
};

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "tracing.hpp"
#include "../types/UiaTextRangeBase.hpp"
#include "../types/ScreenInfoUiaProviderBase.h"

using namespace Microsoft::Console::Types;

// NOTE: See `til.h` for which keyword flags are reserved
// to ensure newly added ones do NOT overlap.
enum TraceKeywords
{
    //Font = 0x001, // _DBGFONTS
    //Font2 = 0x002, // _DBGFONTS2
    Chars = 0x004, // _DBGCHARS
    Output = 0x008, // _DBGOUTPUT
    General = 0x100,
    Input = 0x200,
    //API = 0x400, // No longer used
    UIA = 0x800,
    CookedRead = 0x1000,
    ConsoleAttachDetach = 0x2000,
    All = 0x1FFF
};
DEFINE_ENUM_FLAG_OPERATORS(TraceKeywords);

ULONG Tracing::s_ulDebugFlag = 0x0;

void Tracing::s_TraceWindowViewport(const Viewport& viewport)
{
    TraceLoggingWrite(
        g_hConhostV2EventTraceProvider,
        "WindowViewport",
        TraceLoggingInt32(viewport.Height(), "ViewHeight"),
        TraceLoggingInt32(viewport.Width(), "ViewWidth"),
        TraceLoggingInt32(viewport.Top(), "OriginTop"),
        TraceLoggingInt32(viewport.Left(), "OriginLeft"),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingKeyword(TIL_KEYWORD_TRACE),
        TraceLoggingKeyword(TraceKeywords::General));
}

void Tracing::s_TraceChars(_In_z_ const char* pszMessage, ...)
{
    va_list args;
    va_start(args, pszMessage);
    char szBuffer[256] = "";
    vsprintf_s(szBuffer, ARRAYSIZE(szBuffer), pszMessage, args);
    va_end(args);

    TraceLoggingWrite(
        g_hConhostV2EventTraceProvider,
        "CharsTrace",
        TraceLoggingString(szBuffer),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingKeyword(TIL_KEYWORD_TRACE),
        TraceLoggingKeyword(TraceKeywords::Chars));

    if (s_ulDebugFlag & TraceKeywords::Chars)
    {
        OutputDebugStringA(szBuffer);
    }
}

void Tracing::s_TraceOutput(_In_z_ const char* pszMessage, ...)
{
    va_list args;
    va_start(args, pszMessage);
    char szBuffer[256] = "";
    vsprintf_s(szBuffer, ARRAYSIZE(szBuffer), pszMessage, args);
    va_end(args);

    TraceLoggingWrite(
        g_hConhostV2EventTraceProvider,
        "OutputTrace",
        TraceLoggingString(szBuffer),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingKeyword(TIL_KEYWORD_TRACE),
        TraceLoggingKeyword(TraceKeywords::Output));

    if (s_ulDebugFlag & TraceKeywords::Output)
    {
        OutputDebugStringA(szBuffer);
    }
}

void Tracing::s_TraceWindowMessage(const MSG& msg)
{
    TraceLoggingWrite(
        g_hConhostV2EventTraceProvider,
        "Window Message",
        TraceLoggingHexUInt32(msg.message, "message"),
        TraceLoggingHexUInt64(msg.wParam, "wParam"),
        TraceLoggingHexUInt64(msg.lParam, "lParam"),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingKeyword(TIL_KEYWORD_TRACE),
        TraceLoggingKeyword(TraceKeywords::Input));
}

void Tracing::s_TraceInputRecord(const INPUT_RECORD& inputRecord)
{
    switch (inputRecord.EventType)
    {
    case KEY_EVENT:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "Key Event Input Record",
            TraceLoggingBool(inputRecord.Event.KeyEvent.bKeyDown, "bKeyDown"),
            TraceLoggingUInt16(inputRecord.Event.KeyEvent.wRepeatCount, "wRepeatCount"),
            TraceLoggingHexUInt16(inputRecord.Event.KeyEvent.wVirtualKeyCode, "wVirtualKeyCode"),
            TraceLoggingHexUInt16(inputRecord.Event.KeyEvent.wVirtualScanCode, "wVirtualScanCode"),
            TraceLoggingWChar(inputRecord.Event.KeyEvent.uChar.UnicodeChar, "UnicodeChar"),
            TraceLoggingWChar(inputRecord.Event.KeyEvent.uChar.AsciiChar, "AsciiChar"),
            TraceLoggingHexUInt16(inputRecord.Event.KeyEvent.uChar.UnicodeChar, "Hex UnicodeChar"),
            TraceLoggingHexUInt8(inputRecord.Event.KeyEvent.uChar.AsciiChar, "Hex AsciiChar"),
            TraceLoggingHexUInt32(inputRecord.Event.KeyEvent.dwControlKeyState, "dwControlKeyState"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE),
            TraceLoggingKeyword(TraceKeywords::Input));
        break;
    case MOUSE_EVENT:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "Mouse Event Input Record",
            TraceLoggingInt16(inputRecord.Event.MouseEvent.dwMousePosition.X, "dwMousePosition.X"),
            TraceLoggingInt16(inputRecord.Event.MouseEvent.dwMousePosition.Y, "dwMousePosition.Y"),
            TraceLoggingHexUInt32(inputRecord.Event.MouseEvent.dwButtonState, "dwButtonState"),
            TraceLoggingHexUInt32(inputRecord.Event.MouseEvent.dwControlKeyState, "dwControlKeyState"),
            TraceLoggingHexUInt32(inputRecord.Event.MouseEvent.dwEventFlags, "dwEventFlags"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE),
            TraceLoggingKeyword(TraceKeywords::Input));
        break;
    case WINDOW_BUFFER_SIZE_EVENT:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "Window Buffer Size Event Input Record",
            TraceLoggingInt16(inputRecord.Event.WindowBufferSizeEvent.dwSize.X, "dwSize.X"),
            TraceLoggingInt16(inputRecord.Event.WindowBufferSizeEvent.dwSize.Y, "dwSize.Y"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE),
            TraceLoggingKeyword(TraceKeywords::Input));
        break;
    case MENU_EVENT:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "Menu Event Input Record",
            TraceLoggingHexUInt64(inputRecord.Event.MenuEvent.dwCommandId, "dwCommandId"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE),
            TraceLoggingKeyword(TraceKeywords::Input));
        break;
    case FOCUS_EVENT:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "Focus Event Input Record",
            TraceLoggingBool(inputRecord.Event.FocusEvent.bSetFocus, "bSetFocus"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE),
            TraceLoggingKeyword(TraceKeywords::Input));
        break;
    default:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "Unknown Input Record",
            TraceLoggingHexUInt16(inputRecord.EventType, "EventType"),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE),
            TraceLoggingKeyword(TraceKeywords::Input));
        break;
    }
}

void Tracing::s_TraceCookedRead(_In_ ConsoleProcessHandle* const pConsoleProcessHandle, _In_reads_(cchCookedBufferLength) const wchar_t* pwchCookedBuffer, _In_ ULONG cchCookedBufferLength)
{
    if (TraceLoggingProviderEnabled(g_hConhostV2EventTraceProvider, 0, TraceKeywords::CookedRead))
    {
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "CookedRead",
            TraceLoggingPid(pConsoleProcessHandle->dwProcessId, "AttachedProcessId"),
            TraceLoggingCountedWideString(pwchCookedBuffer, cchCookedBufferLength, "ReadBuffer"),
            TraceLoggingULong(cchCookedBufferLength, "ReadBufferLength"),
            TraceLoggingFileTime(pConsoleProcessHandle->GetProcessCreationTime(), "AttachedProcessCreationTime"),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE),
            TraceLoggingKeyword(TraceKeywords::CookedRead));
    }
}

void Tracing::s_TraceConsoleAttachDetach(_In_ ConsoleProcessHandle* const pConsoleProcessHandle, _In_ bool bIsAttach)
{
    if (TraceLoggingProviderEnabled(g_hConhostV2EventTraceProvider, 0, TraceKeywords::ConsoleAttachDetach))
    {
        auto bIsUserInteractive = Telemetry::Instance().IsUserInteractive();

        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ConsoleAttachDetach",
            TraceLoggingPid(pConsoleProcessHandle->dwProcessId, "AttachedProcessId"),
            TraceLoggingFileTime(pConsoleProcessHandle->GetProcessCreationTime(), "AttachedProcessCreationTime"),
            TraceLoggingBool(bIsAttach, "IsAttach"),
            TraceLoggingBool(bIsUserInteractive, "IsUserInteractive"),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE),
            TraceLoggingKeyword(TraceKeywords::ConsoleAttachDetach));
    }
}

void __stdcall Tracing::TraceFailure(const wil::FailureInfo& failure) noexcept
{
    TraceLoggingWrite(
        g_hConhostV2EventTraceProvider,
        "Failure",
        TraceLoggingHexUInt32(failure.hr, "HResult"),
        TraceLoggingString(failure.pszFile, "File"),
        TraceLoggingUInt32(failure.uLineNumber, "LineNumber"),
        TraceLoggingString(failure.pszFunction, "Function"),
        TraceLoggingWideString(failure.pszMessage, "Message"),
        TraceLoggingString(failure.pszCallContext, "CallingContext"),
        TraceLoggingString(failure.pszModule, "Module"),
        TraceLoggingPointer(failure.returnAddress, "Site"),
        TraceLoggingString(failure.pszCode, "Code"),
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
        TraceLoggingKeyword(TIL_KEYWORD_TRACE));
}

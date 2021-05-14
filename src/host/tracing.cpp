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
    API = 0x400,
    UIA = 0x800,
    All = 0xFFF
};
DEFINE_ENUM_FLAG_OPERATORS(TraceKeywords);

// Routine Description:
// - Creates a tracing object to assist with automatically firing a stop event
//   when this object goes out of scope.
// - Give it back to the caller and they will release it when the event period is over.
// Arguments:
// - onExit - Function to process when the object is destroyed (on exit)
Tracing::Tracing(std::function<void()> onExit) :
    _onExit(onExit)
{
}

// Routine Description:
// - Destructs a tracing object, running any on exit routine, if necessary.
Tracing::~Tracing()
{
    if (_onExit)
    {
        _onExit();
    }
}

// Routine Description:
// - Provides generic tracing for all API call types in the form of
//   start/stop period events for timing and region-of-interest purposes
//   while doing performance analysis.
// Arguments:
// - result - Reference to the area where the result code from the Api call
//            will be stored for use in the stop event.
// - traceName - The name of the API call to list in the trace details
// Return Value:
// - An object for the caller to hold until the API call is complete.
//   Then destroy it to signal that the call is over so the stop trace can be written.
Tracing Tracing::s_TraceApiCall(const NTSTATUS& result, PCSTR traceName)
{
    // clang-format off
    TraceLoggingWrite(
        g_hConhostV2EventTraceProvider,
        "ApiCall",
        TraceLoggingString(traceName, "ApiName"),
        TraceLoggingOpcode(WINEVENT_OPCODE_START),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingKeyword(TIL_KEYWORD_TRACE),
        TraceLoggingKeyword(TraceKeywords::API));

    return Tracing([traceName, &result] {
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ApiCall",
            TraceLoggingString(traceName, "ApiName"),
            TraceLoggingHResult(result, "Result"),
            TraceLoggingOpcode(WINEVENT_OPCODE_STOP),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE),
            TraceLoggingKeyword(TraceKeywords::API));
    });
    // clang-format on
}

ULONG Tracing::s_ulDebugFlag = 0x0;

void Tracing::s_TraceApi(const NTSTATUS status, const CONSOLE_GETLARGESTWINDOWSIZE_MSG* const a)
{
    TraceLoggingWrite(
        g_hConhostV2EventTraceProvider,
        "API_GetLargestWindowSize",
        TraceLoggingHexInt32(status, "ResultCode"),
        TraceLoggingInt32(a->Size.X, "MaxWindowWidthInChars"),
        TraceLoggingInt32(a->Size.Y, "MaxWindowHeightInChars"),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingKeyword(TIL_KEYWORD_TRACE),
        TraceLoggingKeyword(TraceKeywords::API));
}

void Tracing::s_TraceApi(const NTSTATUS status, const CONSOLE_SCREENBUFFERINFO_MSG* const a, const bool fSet)
{
    // Duplicate copies required by TraceLogging documentation ("don't get cute" examples)
    // Using logic inside these macros can make problems. Do all logic outside macros.

    if (fSet)
    {
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "API_SetConsoleScreenBufferInfo",
            TraceLoggingHexInt32(status, "ResultCode"),
            TraceLoggingInt32(a->Size.X, "BufferWidthInChars"),
            TraceLoggingInt32(a->Size.Y, "BufferHeightInChars"),
            TraceLoggingInt32(a->CurrentWindowSize.X, "WindowWidthInChars"),
            TraceLoggingInt32(a->CurrentWindowSize.Y, "WindowHeightInChars"),
            TraceLoggingInt32(a->MaximumWindowSize.X, "MaxWindowWidthInChars"),
            TraceLoggingInt32(a->MaximumWindowSize.Y, "MaxWindowHeightInChars"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE),
            TraceLoggingKeyword(TraceKeywords::API));
    }
    else
    {
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "API_GetConsoleScreenBufferInfo",
            TraceLoggingHexInt32(status, "ResultCode"),
            TraceLoggingInt32(a->Size.X, "BufferWidthInChars"),
            TraceLoggingInt32(a->Size.Y, "BufferHeightInChars"),
            TraceLoggingInt32(a->CurrentWindowSize.X, "WindowWidthInChars"),
            TraceLoggingInt32(a->CurrentWindowSize.Y, "WindowHeightInChars"),
            TraceLoggingInt32(a->MaximumWindowSize.X, "MaxWindowWidthInChars"),
            TraceLoggingInt32(a->MaximumWindowSize.Y, "MaxWindowHeightInChars"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE),
            TraceLoggingKeyword(TraceKeywords::API));
    }
}

void Tracing::s_TraceApi(const NTSTATUS status, const CONSOLE_SETSCREENBUFFERSIZE_MSG* const a)
{
    TraceLoggingWrite(
        g_hConhostV2EventTraceProvider,
        "API_SetConsoleScreenBufferSize",
        TraceLoggingHexInt32(status, "ResultCode"),
        TraceLoggingInt32(a->Size.X, "BufferWidthInChars"),
        TraceLoggingInt32(a->Size.Y, "BufferHeightInChars"),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingKeyword(TIL_KEYWORD_TRACE),
        TraceLoggingKeyword(TraceKeywords::API));
}

void Tracing::s_TraceApi(const NTSTATUS status, const CONSOLE_SETWINDOWINFO_MSG* const a)
{
    TraceLoggingWrite(
        g_hConhostV2EventTraceProvider,
        "API_SetConsoleWindowInfo",
        TraceLoggingHexInt32(status, "ResultCode"),
        TraceLoggingBool(a->Absolute, "IsWindowRectAbsolute"),
        TraceLoggingInt32(a->Window.Left, "WindowRectLeft"),
        TraceLoggingInt32(a->Window.Right, "WindowRectRight"),
        TraceLoggingInt32(a->Window.Top, "WindowRectTop"),
        TraceLoggingInt32(a->Window.Bottom, "WindowRectBottom"),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingKeyword(TIL_KEYWORD_TRACE),
        TraceLoggingKeyword(TraceKeywords::API));
}

void Tracing::s_TraceApi(_In_ const void* const buffer, const CONSOLE_WRITECONSOLE_MSG* const a)
{
    // clang-format off
    if (a->Unicode)
    {
        const wchar_t* const buf = static_cast<const wchar_t* const>(buffer);
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "API_WriteConsole",
            TraceLoggingBoolean(a->Unicode, "Unicode"),
            TraceLoggingUInt32(a->NumBytes, "NumBytes"),
            TraceLoggingCountedWideString(buf, static_cast<UINT16>(a->NumBytes / sizeof(wchar_t)), "input buffer"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE),
            TraceLoggingKeyword(TraceKeywords::API));
    }
    else
    {
        const char* const buf = static_cast<const char* const>(buffer);
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "API_WriteConsole",
            TraceLoggingBoolean(a->Unicode, "Unicode"),
            TraceLoggingUInt32(a->NumBytes, "NumBytes"),
            TraceLoggingCountedString(buf, static_cast<UINT16>(a->NumBytes / sizeof(char)), "input buffer"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE),
            TraceLoggingKeyword(TraceKeywords::API));
    }
    // clang-format on
}

void Tracing::s_TraceApi(const CONSOLE_SCREENBUFFERINFO_MSG* const a)
{
    TraceLoggingWrite(
        g_hConhostV2EventTraceProvider,
        "API_GetConsoleScreenBufferInfo",
        TraceLoggingInt16(a->Size.X, "Size.X"),
        TraceLoggingInt16(a->Size.Y, "Size.Y"),
        TraceLoggingInt16(a->CursorPosition.X, "CursorPosition.X"),
        TraceLoggingInt16(a->CursorPosition.Y, "CursorPosition.Y"),
        TraceLoggingInt16(a->ScrollPosition.X, "ScrollPosition.X"),
        TraceLoggingInt16(a->ScrollPosition.Y, "ScrollPosition.Y"),
        TraceLoggingHexUInt16(a->Attributes, "Attributes"),
        TraceLoggingInt16(a->CurrentWindowSize.X, "CurrentWindowSize.X"),
        TraceLoggingInt16(a->CurrentWindowSize.Y, "CurrentWindowSize.Y"),
        TraceLoggingInt16(a->MaximumWindowSize.X, "MaximumWindowSize.X"),
        TraceLoggingInt16(a->MaximumWindowSize.Y, "MaximumWindowSize.Y"),
        TraceLoggingHexUInt16(a->PopupAttributes, "PopupAttributes"),
        TraceLoggingBoolean(a->FullscreenSupported, "FullscreenSupported"),
        TraceLoggingHexUInt32FixedArray((UINT32 const*)a->ColorTable, _countof(a->ColorTable), "ColorTable"),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingKeyword(TIL_KEYWORD_TRACE),
        TraceLoggingKeyword(TraceKeywords::API));
    static_assert(sizeof(UINT32) == sizeof(*a->ColorTable), "a->ColorTable");
}

void Tracing::s_TraceApi(const CONSOLE_MODE_MSG* const a, const std::wstring_view handleType)
{
    TraceLoggingWrite(
        g_hConhostV2EventTraceProvider,
        "API_GetConsoleMode",
        TraceLoggingHexUInt32(a->Mode, "Mode"),
        TraceLoggingCountedWideString(handleType.data(), gsl::narrow_cast<ULONG>(handleType.size()), "Handle type"),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingKeyword(TIL_KEYWORD_TRACE),
        TraceLoggingKeyword(TraceKeywords::API));
}

void Tracing::s_TraceApi(const CONSOLE_SETTEXTATTRIBUTE_MSG* const a)
{
    TraceLoggingWrite(
        g_hConhostV2EventTraceProvider,
        "API_SetConsoleTextAttribute",
        TraceLoggingHexUInt16(a->Attributes, "Attributes"),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingKeyword(TIL_KEYWORD_TRACE),
        TraceLoggingKeyword(TraceKeywords::API));
}

void Tracing::s_TraceApi(const CONSOLE_WRITECONSOLEOUTPUTSTRING_MSG* const a)
{
    TraceLoggingWrite(
        g_hConhostV2EventTraceProvider,
        "API_WriteConsoleOutput",
        TraceLoggingInt16(a->WriteCoord.X, "WriteCoord.X"),
        TraceLoggingInt16(a->WriteCoord.Y, "WriteCoord.Y"),
        TraceLoggingHexUInt32(a->StringType, "StringType"),
        TraceLoggingUInt32(a->NumRecords, "NumRecords"),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingKeyword(TIL_KEYWORD_TRACE),
        TraceLoggingKeyword(TraceKeywords::API));
}

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

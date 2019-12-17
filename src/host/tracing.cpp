// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "tracing.hpp"
#include "../types/UiaTextRangeBase.hpp"
#include "../types/ScreenInfoUiaProviderBase.h"

#include "../types/WindowUiaProviderBase.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity::Win32;

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
        TraceLoggingKeyword(TraceKeywords::API));

    return Tracing([traceName, &result] {
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ApiCall",
            TraceLoggingString(traceName, "ApiName"),
            TraceLoggingHResult(result, "Result"),
            TraceLoggingOpcode(WINEVENT_OPCODE_STOP),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
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
        TraceLoggingKeyword(TraceKeywords::API));
    static_assert(sizeof(UINT32) == sizeof(*a->ColorTable), "a->ColorTable");
}

void Tracing::s_TraceApi(const CONSOLE_MODE_MSG* const a, const std::wstring& handleType)
{
    TraceLoggingWrite(
        g_hConhostV2EventTraceProvider,
        "API_GetConsoleMode",
        TraceLoggingHexUInt32(a->Mode, "Mode"),
        TraceLoggingWideString(handleType.c_str(), "Handle type"),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingKeyword(TraceKeywords::API));
}

void Tracing::s_TraceApi(const CONSOLE_SETTEXTATTRIBUTE_MSG* const a)
{
    TraceLoggingWrite(
        g_hConhostV2EventTraceProvider,
        "API_SetConsoleTextAttribute",
        TraceLoggingHexUInt16(a->Attributes, "Attributes"),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
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
            TraceLoggingKeyword(TraceKeywords::Input));
        break;
    case WINDOW_BUFFER_SIZE_EVENT:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "Window Buffer Size Event Input Record",
            TraceLoggingInt16(inputRecord.Event.WindowBufferSizeEvent.dwSize.X, "dwSize.X"),
            TraceLoggingInt16(inputRecord.Event.WindowBufferSizeEvent.dwSize.Y, "dwSize.Y"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::Input));
        break;
    case MENU_EVENT:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "Menu Event Input Record",
            TraceLoggingHexUInt64(inputRecord.Event.MenuEvent.dwCommandId, "dwCommandId"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::Input));
        break;
    case FOCUS_EVENT:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "Focus Event Input Record",
            TraceLoggingBool(inputRecord.Event.FocusEvent.bSetFocus, "bSetFocus"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::Input));
        break;
    default:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "Unknown Input Record",
            TraceLoggingHexUInt16(inputRecord.EventType, "EventType"),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
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
        TraceLoggingLevel(WINEVENT_LEVEL_ERROR));
}

// TODO GitHub #1914: Re-attach Tracing to UIA Tree
#if 0
void Tracing::s_TraceUia(const UiaTextRange* const range,
                         const UiaTextRangeTracing::ApiCall apiCall,
                         const UiaTextRangeTracing::IApiMsg* const apiMsg)
{
    unsigned long long id = 0u;
    bool degenerate = true;
    Endpoint start = 0u;
    Endpoint end = 0u;
    if (range)
    {
        id = range->GetId();
        degenerate = range->IsDegenerate();
        start = range->GetStart();
        end = range->GetEnd();
    }

    switch (apiCall)
    {
    case UiaTextRangeTracing::ApiCall::Constructor:
    {
        id = static_cast<const UiaTextRangeTracing::ApiMsgConstructor* const>(apiMsg)->Id;
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::Constructor",
            TraceLoggingValue(id, "_id"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    }
    case UiaTextRangeTracing::ApiCall::AddRef:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::AddRef",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case UiaTextRangeTracing::ApiCall::Release:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::Release",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case UiaTextRangeTracing::ApiCall::QueryInterface:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::QueryInterface",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case UiaTextRangeTracing::ApiCall::Clone:
    {
        if (apiMsg == nullptr)
        {
            return;
        }
        auto cloneId = reinterpret_cast<const UiaTextRangeTracing::ApiMsgClone* const>(apiMsg)->CloneId;
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::Clone",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingValue(cloneId, "clone's _id"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    }
    case UiaTextRangeTracing::ApiCall::Compare:
    {
        const UiaTextRangeTracing::ApiMsgCompare* const msg = static_cast<const UiaTextRangeTracing::ApiMsgCompare* const>(apiMsg);
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::Compare",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingValue(msg->OtherId, "Other's Id"),
            TraceLoggingValue(msg->Equal, "Equal"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    }
    case UiaTextRangeTracing::ApiCall::CompareEndpoints:
    {
        const UiaTextRangeTracing::ApiMsgCompareEndpoints* const msg = static_cast<const UiaTextRangeTracing::ApiMsgCompareEndpoints* const>(apiMsg);
        const wchar_t* const pEndpoint = _textPatternRangeEndpointToString(msg->Endpoint);
        const wchar_t* const pTargetEndpoint = _textPatternRangeEndpointToString(msg->TargetEndpoint);
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::CompareEndpoints",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingValue(msg->OtherId, "Other's Id"),
            TraceLoggingValue(pEndpoint, "endpoint"),
            TraceLoggingValue(pTargetEndpoint, "targetEndpoint"),
            TraceLoggingValue(msg->Result, "Result"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    }
    case UiaTextRangeTracing::ApiCall::ExpandToEnclosingUnit:
    {
        const UiaTextRangeTracing::ApiMsgExpandToEnclosingUnit* const msg = static_cast<const UiaTextRangeTracing::ApiMsgExpandToEnclosingUnit* const>(apiMsg);
        const wchar_t* const pUnitName = _textUnitToString(msg->Unit);
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::ExpandToEnclosingUnit",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingValue(pUnitName, "Unit"),
            TraceLoggingValue(msg->OriginalStart, "Original Start"),
            TraceLoggingValue(msg->OriginalEnd, "Original End"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    }
    case UiaTextRangeTracing::ApiCall::FindAttribute:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::FindAttribute",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case UiaTextRangeTracing::ApiCall::FindText:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::FindText",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case UiaTextRangeTracing::ApiCall::GetAttributeValue:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::GetAttributeValue",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case UiaTextRangeTracing::ApiCall::GetBoundingRectangles:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::GetBoundingRectangles",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case UiaTextRangeTracing::ApiCall::GetEnclosingElement:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::GetEnclosingElement",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case UiaTextRangeTracing::ApiCall::GetText:
    {
        const UiaTextRangeTracing::ApiMsgGetText* const msg = static_cast<const UiaTextRangeTracing::ApiMsgGetText* const>(apiMsg);
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::GetText",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingValue(msg->Text, "Text"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    }
    case UiaTextRangeTracing::ApiCall::Move:
    {
        const UiaTextRangeTracing::ApiMsgMove* const msg = static_cast<const UiaTextRangeTracing::ApiMsgMove* const>(apiMsg);
        const wchar_t* const unitStr = _textUnitToString(msg->Unit);
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::Move",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingValue(msg->OriginalStart, "Original Start"),
            TraceLoggingValue(msg->OriginalEnd, "Original End"),
            TraceLoggingValue(unitStr, "unit"),
            TraceLoggingValue(msg->RequestedCount, "Requested Count"),
            TraceLoggingValue(msg->MovedCount, "Moved Count"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    }
    case UiaTextRangeTracing::ApiCall::MoveEndpointByUnit:
    {
        const UiaTextRangeTracing::ApiMsgMoveEndpointByUnit* const msg = static_cast<const UiaTextRangeTracing::ApiMsgMoveEndpointByUnit* const>(apiMsg);
        const wchar_t* const pEndpoint = _textPatternRangeEndpointToString(msg->Endpoint);
        const wchar_t* const unitStr = _textUnitToString(msg->Unit);
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::MoveEndpointByUnit",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingValue(msg->OriginalStart, "Original Start"),
            TraceLoggingValue(msg->OriginalEnd, "Original End"),
            TraceLoggingValue(pEndpoint, "endpoint"),
            TraceLoggingValue(unitStr, "unit"),
            TraceLoggingValue(msg->RequestedCount, "Requested Count"),
            TraceLoggingValue(msg->MovedCount, "Moved Count"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    }
    case UiaTextRangeTracing::ApiCall::MoveEndpointByRange:
    {
        const UiaTextRangeTracing::ApiMsgMoveEndpointByRange* const msg = static_cast<const UiaTextRangeTracing::ApiMsgMoveEndpointByRange* const>(apiMsg);
        const wchar_t* const pEndpoint = _textPatternRangeEndpointToString(msg->Endpoint);
        const wchar_t* const pTargetEndpoint = _textPatternRangeEndpointToString(msg->TargetEndpoint);
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::MoveEndpointByRange",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingValue(msg->OriginalStart, "Original Start"),
            TraceLoggingValue(msg->OriginalEnd, "Original End"),
            TraceLoggingValue(pEndpoint, "endpoint"),
            TraceLoggingValue(pTargetEndpoint, "targetEndpoint"),
            TraceLoggingValue(msg->OtherId, "Other's _id"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    }
    case UiaTextRangeTracing::ApiCall::Select:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::Select",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case UiaTextRangeTracing::ApiCall::AddToSelection:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::AddToSelection",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case UiaTextRangeTracing::ApiCall::RemoveFromSelection:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::RemoveFromSelection",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case UiaTextRangeTracing::ApiCall::ScrollIntoView:
    {
        const UiaTextRangeTracing::ApiMsgScrollIntoView* const msg = static_cast<const UiaTextRangeTracing::ApiMsgScrollIntoView* const>(apiMsg);
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::ScrollIntoView",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingValue(msg->AlignToTop, "alignToTop"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    }
    case UiaTextRangeTracing::ApiCall::GetChildren:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "UiaTextRange::GetChildren",
            TraceLoggingValue(id, "_id"),
            TraceLoggingValue(start, "_start"),
            TraceLoggingValue(end, "_end"),
            TraceLoggingValue(degenerate, "_degenerate"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    default:
        break;
    }
}

void Tracing::s_TraceUia(const Microsoft::Console::Interactivity::Win32::ScreenInfoUiaProvider* const /*pProvider*/,
                         const ScreenInfoUiaProviderTracing::ApiCall apiCall,
                         const ScreenInfoUiaProviderTracing::IApiMsg* const apiMsg)
{
    switch (apiCall)
    {
    case ScreenInfoUiaProviderTracing::ApiCall::Constructor:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::Constructor",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::Signal:
    {
        const ScreenInfoUiaProviderTracing::ApiMsgSignal* const msg = static_cast<const ScreenInfoUiaProviderTracing::ApiMsgSignal* const>(apiMsg);
        const wchar_t* const signalName = _eventIdToString(msg->Signal);
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::Signal",
            TraceLoggingValue(msg->Signal),
            TraceLoggingValue(signalName, "Event Name"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    }
    case ScreenInfoUiaProviderTracing::ApiCall::AddRef:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::AddRef",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::Release:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::Release",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::QueryInterface:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::QueryInterface",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::GetProviderOptions:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::GetProviderOptions",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::GetPatternProvider:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::GetPatternProvider",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::GetPropertyValue:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::GetPropertyValue",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::GetHostRawElementProvider:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::GetHostRawElementProvider",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::Navigate:
    {
        const ScreenInfoUiaProviderTracing::ApiMsgNavigate* const msg = static_cast<const ScreenInfoUiaProviderTracing::ApiMsgNavigate* const>(apiMsg);
        const wchar_t* const direction = _directionToString(msg->Direction);
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::Navigate",
            TraceLoggingValue(direction, "direction"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    }
    case ScreenInfoUiaProviderTracing::ApiCall::GetRuntimeId:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::GetRuntimeId",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::GetBoundingRectangle:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::GetBoundingRectangles",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::GetEmbeddedFragmentRoots:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::GetEmbeddedFragmentRoots",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::SetFocus:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::SetFocus",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::GetFragmentRoot:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::GetFragmentRoot",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::GetSelection:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::GetSelection",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::GetVisibleRanges:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::GetVisibleRanges",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::RangeFromChild:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::RangeFromChild",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::RangeFromPoint:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::RangeFromPoint",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::GetDocumentRange:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::GetDocumentRange",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case ScreenInfoUiaProviderTracing::ApiCall::GetSupportedTextSelection:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "ScreenInfoUiaProvider::GetSupportedTextSelection",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    default:
        break;
    }
}

void Tracing::s_TraceUia(const Microsoft::Console::Types::WindowUiaProvider* const /*pProvider*/,
                         const Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall apiCall,
                         const Microsoft::Console::Types::WindowUiaProviderTracing::IApiMsg* const apiMsg)
{
    switch (apiCall)
    {
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::Create:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::Create",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::Signal:
    {
        const Microsoft::Console::Types::WindowUiaProviderTracing::ApiMessageSignal* const msg = static_cast<const Microsoft::Console::Types::WindowUiaProviderTracing::ApiMessageSignal* const>(apiMsg);
        const wchar_t* const eventName = _eventIdToString(msg->Signal);
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::Signal",
            TraceLoggingValue(msg->Signal, "Signal"),
            TraceLoggingValue(eventName, "Signal Name"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    }
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::AddRef:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::AddRef",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::Release:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::Release",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::QueryInterface:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::QueryInterface",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::GetProviderOptions:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::GetProviderOptions",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::GetPatternProvider:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::GetPatternProvider",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::GetPropertyValue:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::GetPropertyValue",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::GetHostRawElementProvider:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::GetHostRawElementProvider",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::Navigate:
    {
        const Microsoft::Console::Types::WindowUiaProviderTracing::ApiMsgNavigate* const msg = static_cast<const Microsoft::Console::Types::WindowUiaProviderTracing::ApiMsgNavigate* const>(apiMsg);
        const wchar_t* const direction = _directionToString(msg->Direction);
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::Navigate",
            TraceLoggingValue(direction, "direction"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    }
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::GetRuntimeId:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::GetRuntimeId",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::GetBoundingRectangle:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::GetBoundingRectangle",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::GetEmbeddedFragmentRoots:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::GetEmbeddedFragmentRoots",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::SetFocus:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::SetFocus",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::GetFragmentRoot:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::GetFragmentRoot",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::ElementProviderFromPoint:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::ElementProviderFromPoint",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    case Microsoft::Console::Types::WindowUiaProviderTracing::ApiCall::GetFocus:
        TraceLoggingWrite(
            g_hConhostV2EventTraceProvider,
            "WindowUiaProvider::GetFocus",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TraceKeywords::UIA));
        break;
    default:
        break;
    }
}
#endif

const wchar_t* const Tracing::_textPatternRangeEndpointToString(int endpoint)
{
    switch (endpoint)
    {
    case TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start:
        return L"Start";
    case TextPatternRangeEndpoint::TextPatternRangeEndpoint_End:
        return L"End";
    default:
        return L"Unknown";
    }
}

const wchar_t* const Tracing::_textUnitToString(int unit)
{
    switch (unit)
    {
    case TextUnit::TextUnit_Character:
        return L"TextUnit_Character";
    case TextUnit::TextUnit_Format:
        return L"TextUnit_Format";
    case TextUnit::TextUnit_Word:
        return L"TextUnit_Word";
    case TextUnit::TextUnit_Line:
        return L"TextUnit_Line";
    case TextUnit::TextUnit_Paragraph:
        return L"TextUnit_Paragraph";
    case TextUnit::TextUnit_Page:
        return L"TextUnit_Page";
    case TextUnit::TextUnit_Document:
        return L"TextUnit_Document";
    default:
        return L"Unknown";
    }
}

const wchar_t* const Tracing::_eventIdToString(long eventId)
{
    switch (eventId)
    {
    case UIA_AutomationFocusChangedEventId:
        return L"UIA_AutomationFocusChangedEventId";
    case UIA_Text_TextChangedEventId:
        return L"UIA_Text_TextChangedEventId";
    case UIA_Text_TextSelectionChangedEventId:
        return L"UIA_Text_TextSelectionChangedEventId";
    default:
        return L"Unknown";
    }
}

const wchar_t* const Tracing::_directionToString(int direction)
{
    switch (direction)
    {
    case NavigateDirection::NavigateDirection_FirstChild:
        return L"NavigateDirection_FirstChild";
    case NavigateDirection::NavigateDirection_LastChild:
        return L"NavigateDirection_LastChild";
    case NavigateDirection::NavigateDirection_NextSibling:
        return L"NavigateDirection_NextSibling";
    case NavigateDirection::NavigateDirection_Parent:
        return L"NavigateDirection_Parent";
    case NavigateDirection::NavigateDirection_PreviousSibling:
        return L"NavigateDirection_PreviousSibling";
    default:
        return L"Unknown";
    }
}

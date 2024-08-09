// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "UiaTracing.h"

// we need to disable a few warnings because
// the TraceLogging code is out of our control
#pragma warning(push)
#pragma warning(disable : 26446 26447 26477 26482 26485 26494 26496)
TRACELOGGING_DEFINE_PROVIDER(g_UiaProviderTraceProvider,
                             "Microsoft.Windows.Console.UIA",
                             // tl:{e7ebce59-2161-572d-b263-2f16a6afb9e5}
                             (0xe7ebce59, 0x2161, 0x572d, 0xb2, 0x63, 0x2f, 0x16, 0xa6, 0xaf, 0xb9, 0xe5));

using namespace Microsoft::Console::Types;

// The first valid ID is 1 for each of our traced UIA object types
// ID assignment is handled between UiaTracing and IUiaTraceable to...
//  - prevent multiple objects with the same ID
//  - only assign IDs if UiaTracing is enabled
//  - ensure objects are only assigned an ID once
IdType UiaTracing::_utrId = 1;
IdType UiaTracing::_siupId = 1;

// Routine Description:
// - assign an ID to the UiaTextRange, if it doesn't have one already
// Arguments:
// - utr - the UiaTextRange we are assigning an ID to
// Return Value:
// - N/A
void UiaTracing::_assignId(UiaTextRangeBase& utr) noexcept
{
    auto temp = utr.AssignId(_utrId);
    if (temp)
    {
        ++_utrId;
    }
}

// Routine Description:
// - assign an ID to the ScreenInfoUiaProvider, if it doesn't have one already
// Arguments:
// - siup - the ScreenInfoUiaProvider we are assigning an ID to
// Return Value:
// - N/A
void UiaTracing::_assignId(ScreenInfoUiaProviderBase& siup) noexcept
{
    auto temp = siup.AssignId(_siupId);
    if (temp)
    {
        ++_siupId;
    }
}

UiaTracing::UiaTracing() noexcept
{
    TraceLoggingRegister(g_UiaProviderTraceProvider);
}

UiaTracing::~UiaTracing() noexcept
{
    TraceLoggingUnregister(g_UiaProviderTraceProvider);
}

std::wstring UiaTracing::_getValue(const ScreenInfoUiaProviderBase& siup) noexcept
{
    return fmt::format(FMT_COMPILE(L"_id:{}"), siup.GetId());
}

std::wstring UiaTracing::_getValue(const UiaTextRangeBase& utr) noexcept
{
    const auto start = utr.GetEndpoint(TextPatternRangeEndpoint_Start);
    const auto end = utr.GetEndpoint(TextPatternRangeEndpoint_End);
    return fmt::format(FMT_COMPILE(L"_id:{} _start:{},{} _end:{},{} _degenerate:{} _wordDelimiters:{} content:{}"), utr.GetId(), start.x, start.y, end.x, end.y, utr.IsDegenerate(), utr._wordDelimiters, utr._getTextValue());
}

std::wstring UiaTracing::_getValue(const TextPatternRangeEndpoint endpoint) noexcept
{
    switch (endpoint)
    {
    case TextPatternRangeEndpoint_Start:
        return L"Start";
    case TextPatternRangeEndpoint_End:
        return L"End";
    default:
        return L"UNKNOWN VALUE";
    }
}

std::wstring UiaTracing::_getValue(const TextUnit unit) noexcept
{
    switch (unit)
    {
    case TextUnit_Character:
        return L"Character";
    case TextUnit_Format:
        return L"Format";
    case TextUnit_Word:
        return L"Word";
    case TextUnit_Line:
        return L"Line";
    case TextUnit_Paragraph:
        return L"Paragraph";
    case TextUnit_Page:
        return L"Page";
    case TextUnit_Document:
        return L"Document";
    default:
        return L"UNKNOWN VALUE";
    }
}

std::wstring UiaTracing::_getValue(const VARIANT val) noexcept
{
    // This is not a comprehensive conversion of VARIANT result to string
    // We're only including the one's we need at this time.
    switch (val.vt)
    {
    case VT_BSTR:
        return val.bstrVal;
    case VT_R8:
        return std::to_wstring(val.dblVal);
    case VT_BOOL:
        return std::to_wstring(val.boolVal);
    case VT_I4:
        return std::to_wstring(val.lVal);
    case VT_UNKNOWN:
    default:
    {
        return L"unknown";
    }
    }
}

std::wstring UiaTracing::_getValue(const AttributeType attrType) noexcept
{
    switch (attrType)
    {
    case AttributeType::Mixed:
        return L"Mixed";
    case AttributeType::Unsupported:
        return L"Unsupported";
    case AttributeType::Error:
        return L"Error";
    case AttributeType::Standard:
    default:
        return L"Standard";
    }
}

void UiaTracing::TextRange::Constructor(UiaTextRangeBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        _assignId(result);
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::Constructor",
            TraceLoggingValue(_getValue(result).c_str(), "UiaTextRange"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::Clone(const UiaTextRangeBase& utr, UiaTextRangeBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        _assignId(result);
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::Clone",
            TraceLoggingValue(_getValue(utr).c_str(), "base"),
            TraceLoggingValue(_getValue(result).c_str(), "clone"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::Compare(const UiaTextRangeBase& utr, const UiaTextRangeBase& other, bool result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::Compare",
            TraceLoggingValue(_getValue(utr).c_str(), "base"),
            TraceLoggingValue(_getValue(other).c_str(), "other"),
            TraceLoggingValue(result, "result"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::CompareEndpoints(const UiaTextRangeBase& utr, const TextPatternRangeEndpoint endpoint, const UiaTextRangeBase& other, TextPatternRangeEndpoint otherEndpoint, int result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::CompareEndpoints",
            TraceLoggingValue(_getValue(utr).c_str(), "base"),
            TraceLoggingValue(_getValue(endpoint).c_str(), "baseEndpoint"),
            TraceLoggingValue(_getValue(other).c_str(), "other"),
            TraceLoggingValue(_getValue(otherEndpoint).c_str(), "otherEndpoint"),
            TraceLoggingValue(result, "result"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::ExpandToEnclosingUnit(TextUnit unit, const UiaTextRangeBase& utr) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::ExpandToEnclosingUnit",
            TraceLoggingValue(_getValue(unit).c_str(), "textUnit"),
            TraceLoggingValue(_getValue(utr).c_str(), "result"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::FindAttribute(const UiaTextRangeBase& utr, TEXTATTRIBUTEID id, VARIANT val, BOOL searchBackwards, const UiaTextRangeBase& result, AttributeType attrType) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::FindAttribute",
            TraceLoggingValue(_getValue(utr).c_str(), "base"),
            TraceLoggingValue(id, "text attribute ID"),
            TraceLoggingValue(_getValue(val).c_str(), "text attribute sub-data"),
            TraceLoggingValue(searchBackwards ? L"true" : L"false", "search backwards"),
            TraceLoggingValue(_getValue(attrType).c_str(), "attribute type"),
            TraceLoggingValue(_getValue(result).c_str(), "result"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::FindText(const UiaTextRangeBase& base, const std::wstring_view& text, bool searchBackward, bool ignoreCase, const UiaTextRangeBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::FindText",
            TraceLoggingValue(_getValue(base).c_str(), "base"),
            TraceLoggingCountedWideString(text.data(), (ULONG)text.size(), "text"),
            TraceLoggingValue(searchBackward, "searchBackward"),
            TraceLoggingValue(ignoreCase, "ignoreCase"),
            TraceLoggingValue(_getValue(result).c_str(), "result"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::GetAttributeValue(const UiaTextRangeBase& base, TEXTATTRIBUTEID id, VARIANT result, AttributeType attrType) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::GetAttributeValue",
            TraceLoggingValue(_getValue(base).c_str(), "base"),
            TraceLoggingValue(id, "text attribute ID"),
            TraceLoggingValue(_getValue(result).c_str(), "result"),
            TraceLoggingValue(_getValue(attrType).c_str(), "attribute type"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::GetBoundingRectangles(const UiaTextRangeBase& utr) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::GetBoundingRectangles",
            TraceLoggingValue(_getValue(utr).c_str(), "base"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::GetEnclosingElement(const UiaTextRangeBase& utr) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::GetEnclosingElement",
            TraceLoggingValue(_getValue(utr).c_str(), "base"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::GetText(const UiaTextRangeBase& utr, int maxLength, const std::wstring_view& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::GetText",
            TraceLoggingValue(_getValue(utr).c_str(), "base"),
            TraceLoggingValue(maxLength, "maxLength"),
            TraceLoggingCountedWideString(result.data(), (ULONG)result.size(), "result"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::Move(TextUnit unit, int count, int resultCount, const UiaTextRangeBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::Move",
            TraceLoggingValue(_getValue(unit).c_str(), "textUnit"),
            TraceLoggingValue(count, "count"),
            TraceLoggingValue(resultCount, "resultCount"),
            TraceLoggingValue(_getValue(result).c_str(), "result"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::MoveEndpointByUnit(TextPatternRangeEndpoint endpoint, TextUnit unit, int count, int resultCount, const UiaTextRangeBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::MoveEndpointByUnit",
            TraceLoggingValue(_getValue(endpoint).c_str(), "endpoint"),
            TraceLoggingValue(_getValue(unit).c_str(), "textUnit"),
            TraceLoggingValue(count, "count"),
            TraceLoggingValue(resultCount, "resultCount"),
            TraceLoggingValue(_getValue(result).c_str(), "result"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::MoveEndpointByRange(TextPatternRangeEndpoint endpoint, const UiaTextRangeBase& other, TextPatternRangeEndpoint otherEndpoint, const UiaTextRangeBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::MoveEndpointByRange",
            TraceLoggingValue(_getValue(endpoint).c_str(), "endpoint"),
            TraceLoggingValue(_getValue(other).c_str(), "other"),
            TraceLoggingValue(_getValue(otherEndpoint).c_str(), "otherEndpoint"),
            TraceLoggingValue(_getValue(result).c_str(), "result"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::Select(const UiaTextRangeBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::Select",
            TraceLoggingValue(_getValue(result).c_str(), "base"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::AddToSelection(const UiaTextRangeBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::AddToSelection (UNSUPPORTED)",
            TraceLoggingValue(_getValue(result).c_str(), "base"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::RemoveFromSelection(const UiaTextRangeBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::RemoveFromSelection (UNSUPPORTED)",
            TraceLoggingValue(_getValue(result).c_str(), "base"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::ScrollIntoView(bool alignToTop, const UiaTextRangeBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::ScrollIntoView",
            TraceLoggingValue(alignToTop, "alignToTop"),
            TraceLoggingValue(_getValue(result).c_str(), "result"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextRange::GetChildren(const UiaTextRangeBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "UiaTextRange::AddToSelection (UNSUPPORTED)",
            TraceLoggingValue(_getValue(result).c_str(), "base"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextProvider::Constructor(ScreenInfoUiaProviderBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        _assignId(result);
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "ScreenInfoUiaProvider::Constructor",
            TraceLoggingValue(_getValue(result).c_str(), "ScreenInfoUiaProvider"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextProvider::get_ProviderOptions(const ScreenInfoUiaProviderBase& siup, ProviderOptions options) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        static constexpr auto getOptions = [](ProviderOptions options) {
            switch (options)
            {
            case ProviderOptions_ServerSideProvider:
                return L"ServerSideProvider";
            default:
                return L"UNKNOWN VALUE";
            }
        };

        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "ScreenInfoUiaProvider::get_ProviderOptions",
            TraceLoggingValue(_getValue(siup).c_str(), "base"),
            TraceLoggingValue(getOptions(options), "providerOptions"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextProvider::GetPatternProvider(const ScreenInfoUiaProviderBase& siup, PATTERNID patternId) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        static constexpr auto getPattern = [](PATTERNID patternId) {
            switch (patternId)
            {
            case UIA_TextPatternId:
                return L"TextPattern";
            default:
                return L"UNKNOWN VALUE";
            }
        };

        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "ScreenInfoUiaProvider::get_ProviderOptions",
            TraceLoggingValue(_getValue(siup).c_str(), "base"),
            TraceLoggingValue(getPattern(patternId), "patternId"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextProvider::GetPropertyValue(const ScreenInfoUiaProviderBase& siup, PROPERTYID propertyId) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        static constexpr auto getProperty = [](PROPERTYID propertyId) {
            switch (propertyId)
            {
            case UIA_ControlTypePropertyId:
                return L"ControlTypePropertyId";
            case UIA_NamePropertyId:
                return L"NamePropertyId";
            case UIA_AutomationIdPropertyId:
                return L"AutomationIdPropertyId";
            case UIA_IsControlElementPropertyId:
                return L"IsControlElementPropertyId";
            case UIA_IsContentElementPropertyId:
                return L"IsContentElementPropertyId";
            case UIA_IsKeyboardFocusablePropertyId:
                return L"IsKeyboardFocusablePropertyId";
            case UIA_HasKeyboardFocusPropertyId:
                return L"HasKeyboardFocusPropertyId";
            case UIA_ProviderDescriptionPropertyId:
                return L"ProviderDescriptionPropertyId";
            case UIA_IsEnabledPropertyId:
                return L"IsEnabledPropertyId";
            default:
                return L"UNKNOWN VALUE";
            }
        };

        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "ScreenInfoUiaProvider::GetPropertyValue",
            TraceLoggingValue(_getValue(siup).c_str(), "base"),
            TraceLoggingValue(getProperty(propertyId), "propertyId"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextProvider::get_HostRawElementProvider(const ScreenInfoUiaProviderBase& siup) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "ScreenInfoUiaProvider::get_HostRawElementProvider (UNSUPPORTED)",
            TraceLoggingValue(_getValue(siup).c_str(), "base"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextProvider::GetRuntimeId(const ScreenInfoUiaProviderBase& siup) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "ScreenInfoUiaProvider::GetRuntimeId",
            TraceLoggingValue(_getValue(siup).c_str(), "base"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextProvider::GetEmbeddedFragmentRoots(const ScreenInfoUiaProviderBase& siup) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "ScreenInfoUiaProvider::GetEmbeddedFragmentRoots (UNSUPPORTED)",
            TraceLoggingValue(_getValue(siup).c_str(), "base"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextProvider::SetFocus(const ScreenInfoUiaProviderBase& siup) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "ScreenInfoUiaProvider::SetFocus",
            TraceLoggingValue(_getValue(siup).c_str(), "base"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextProvider::GetSelection(const ScreenInfoUiaProviderBase& siup, const UiaTextRangeBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "ScreenInfoUiaProvider::GetSelection",
            TraceLoggingValue(_getValue(siup).c_str(), "base"),
            TraceLoggingValue(_getValue(result).c_str(), "result (utr)"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextProvider::GetVisibleRanges(const ScreenInfoUiaProviderBase& siup, const UiaTextRangeBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "ScreenInfoUiaProvider::GetVisibleRanges",
            TraceLoggingValue(_getValue(siup).c_str(), "base"),
            TraceLoggingValue(_getValue(result).c_str(), "result (utr)"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextProvider::RangeFromChild(const ScreenInfoUiaProviderBase& siup, const UiaTextRangeBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "ScreenInfoUiaProvider::GetVisibleRanges",
            TraceLoggingValue(_getValue(siup).c_str(), "base"),
            TraceLoggingValue(_getValue(result).c_str(), "result (utr)"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextProvider::RangeFromPoint(const ScreenInfoUiaProviderBase& siup, UiaPoint point, const UiaTextRangeBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        static constexpr auto getPoint = [](const UiaPoint& point) {
            return fmt::format(FMT_COMPILE(L"{},{}"), (int)point.x, (int)point.y);
        };

        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "ScreenInfoUiaProvider::RangeFromPoint",
            TraceLoggingValue(_getValue(siup).c_str(), "base"),
            TraceLoggingValue(getPoint(point).c_str(), "uiaPoint"),
            TraceLoggingValue(_getValue(result).c_str(), "result (utr)"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextProvider::get_DocumentRange(const ScreenInfoUiaProviderBase& siup, const UiaTextRangeBase& result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "ScreenInfoUiaProvider::GetVisibleRanges",
            TraceLoggingValue(_getValue(siup).c_str(), "base"),
            TraceLoggingValue(_getValue(result).c_str(), "result (utr)"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::TextProvider::get_SupportedTextSelection(const ScreenInfoUiaProviderBase& siup, SupportedTextSelection result) noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        static constexpr auto getResult = [](SupportedTextSelection result) {
            switch (result)
            {
            case SupportedTextSelection_Single:
                return L"Single";
            default:
                return L"UNKNOWN VALUE";
            }
        };

        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "ScreenInfoUiaProvider::get_SupportedTextSelection",
            TraceLoggingValue(_getValue(siup).c_str(), "base"),
            TraceLoggingValue(getResult(result), "result"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::Signal::SelectionChanged() noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "Signal::SelectionChanged",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::Signal::TextChanged() noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "Signal::TextChanged",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

void UiaTracing::Signal::CursorChanged() noexcept
{
    EnsureRegistration();
    if (TraceLoggingProviderEnabled(g_UiaProviderTraceProvider, WINEVENT_LEVEL_VERBOSE, TIL_KEYWORD_TRACE))
    {
        TraceLoggingWrite(
            g_UiaProviderTraceProvider,
            "Signal::CursorChanged",
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));
    }
}

#pragma warning(pop)

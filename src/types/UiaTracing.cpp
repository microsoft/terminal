// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "UiaTracing.h"

TRACELOGGING_DEFINE_PROVIDER(g_UiaProviderTraceProvider,
                             "Microsoft.Windows.Console.UIA",
                             // tl:{e7ebce59-2161-572d-b263-2f16a6afb9e5}
                             (0xe7ebce59, 0x2161, 0x572d, 0xb2, 0x63, 0x2f, 0x16, 0xa6, 0xaf, 0xb9, 0xe5));

using namespace Microsoft::Console::Types;

IdType UiaTracing::_utrId = 1;
IdType UiaTracing::_siupId = 1;

void UiaTracing::_assignId(IUiaTraceable& traceable, IdType& id) noexcept
{
    if (traceable.AssignId(id))
    {
        ++id;
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

std::wstring UiaTracing::_getValue(const IUiaTraceable& traceable) noexcept
{
    std::wstring value;
    value += L"_id: " + std::to_wstring(traceable.GetId());
    return value;
}

std::wstring UiaTracing::_getValue(TextPatternRangeEndpoint endpoint) noexcept
{
    static const std::wstring endpointNames[] = {
        L"Start",
        L"End",
        L"UNKNOWN VALUE"
    };
    const size_t index = static_cast<size_t>(endpoint);
    if (index < std::size(endpointNames))
    {
        return endpointNames[index];
    }
    return endpointNames[std::size(endpointNames) - 1];
}

std::wstring UiaTracing::_getValue(TextUnit unit) noexcept
{
    static const std::wstring unitNames[] = {
        L"Character",
        L"Format",
        L"Word",
        L"Line",
        L"Paragraph",
        L"Page",
        L"Document",
        L"UNKNOWN VALUE"
    };
    const size_t index = static_cast<size_t>(unit);
    if (index < std::size(unitNames))
    {
        return unitNames[index];
    }
    return unitNames[std::size(unitNames) - 1];
}

std::wstring UiaTracing::_getValue(TextPatternRangeEndpoint srcEnd,
                                   TextPatternRangeEndpoint targetEnd) noexcept
{
    std::wstring value;
    value += L"srcEnd: " + _getValue(srcEnd);
    value += L", targetEnd: " + _getValue(targetEnd);
    return value;
}

void UiaTracing::TextRange::Constructor(IUiaTraceable& traceable)
{
    _assignId(traceable, _utrId);
    TraceLoggingWrite(g_UiaProviderTraceProvider,
                      "TextRange_Constructor",
                      TraceLoggingLevel(TRACE_LEVEL_VERBOSE),
                      TraceLoggingUInt64(traceable.GetId(), "_id"));
}

void UiaTracing::TextRange::MethodCall(IUiaTraceable& traceable, const std::wstring& methodName)
{
    TraceLoggingWrite(g_UiaProviderTraceProvider,
                      "TextRange_MethodCall",
                      TraceLoggingLevel(TRACE_LEVEL_VERBOSE),
                      TraceLoggingUInt64(traceable.GetId(), "_id"),
                      TraceLoggingWideString(methodName.c_str(), "methodName"));
}

void UiaTracing::TextRange::GetPropertyValue(IUiaTraceable& traceable, const std::wstring& propertyName)
{
    TraceLoggingWrite(g_UiaProviderTraceProvider,
                      "TextRange_GetPropertyValue",
                      TraceLoggingLevel(TRACE_LEVEL_VERBOSE),
                      TraceLoggingUInt64(traceable.GetId(), "_id"),
                      TraceLoggingWideString(propertyName.c_str(), "propertyName"));
}

void UiaTracing::TextRange::PatternCall(IUiaTraceable& traceable, const std::wstring& patternName)
{
    TraceLoggingWrite(g_UiaProviderTraceProvider,
                      "TextRange_PatternCall",
                      TraceLoggingLevel(TRACE_LEVEL_VERBOSE),
                      TraceLoggingUInt64(traceable.GetId(), "_id"),
                      TraceLoggingWideString(patternName.c_str(), "patternName"));
}

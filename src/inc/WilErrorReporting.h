/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- WilErrorReporting.h
--*/
#pragma once

#include <winmeta.h>
#include <wil/common.h>

#define CONSOLE_WIL_TRACELOGGING_COMMON_FAILURE_PARAMS(failure)                                                                                                                                    \
    TraceLoggingUInt32((failure).hr, "hresult", "Failure error code"),                                                                                                                             \
        TraceLoggingString((failure).pszFile, "fileName", "Source code file name where the error occurred"),                                                                                       \
        TraceLoggingUInt32((failure).uLineNumber, "lineNumber", "Line number within the source code file where the error occurred"),                                                               \
        TraceLoggingString((failure).pszModule, "module", "Name of the binary where the error occurred"),                                                                                          \
        TraceLoggingUInt32(static_cast<DWORD>((failure).type), "failureType", "Indicates what type of failure was observed (exception, returned error, logged error or fail fast"),                \
        TraceLoggingWideString((failure).pszMessage, "message", "Custom message associated with the failure (if any)"),                                                                            \
        TraceLoggingUInt32((failure).threadId, "threadId", "Identifier of the thread the error occurred on"),                                                                                      \
        TraceLoggingString((failure).pszCallContext, "callContext", "List of containing this error"),                                                                                              \
        TraceLoggingUInt32((failure).callContextOriginating.contextId, "originatingContextId", "Identifier for the oldest activity containing this error"),                                        \
        TraceLoggingString((failure).callContextOriginating.contextName, "originatingContextName", "Name of the oldest activity containing this error"),                                           \
        TraceLoggingWideString((failure).callContextOriginating.contextMessage, "originatingContextMessage", "Custom message associated with the oldest activity containing this error (if any)"), \
        TraceLoggingUInt32((failure).callContextCurrent.contextId, "currentContextId", "Identifier for the newest activity containing this error"),                                                \
        TraceLoggingString((failure).callContextCurrent.contextName, "currentContextName", "Name of the newest activity containing this error"),                                                   \
        TraceLoggingWideString((failure).callContextCurrent.contextMessage, "currentContextMessage", "Custom message associated with the newest activity containing this error (if any)")

#define CONSOLE_WIL_TRACELOGGING_FAILURE_PARAMS(failure)       \
    TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance), \
        TraceLoggingStruct(14, "wilResult"),                   \
        CONSOLE_WIL_TRACELOGGING_COMMON_FAILURE_PARAMS(failure)

namespace Microsoft::Console::ErrorReporting
{
    __declspec(selectany) TraceLoggingHProvider FallbackProvider;
    __declspec(noinline) inline void WINAPI ReportFailureToFallbackProvider(bool alreadyReported, const wil::FailureInfo& failure) noexcept
    {
        if (!alreadyReported && FallbackProvider)
        {
            TraceLoggingWrite(FallbackProvider, "FallbackError", TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY), TraceLoggingLevel(WINEVENT_LEVEL_ERROR), CONSOLE_WIL_TRACELOGGING_FAILURE_PARAMS(failure));
        }
    }

    __declspec(noinline) inline void EnableFallbackFailureReporting(TraceLoggingHProvider provider) noexcept
    {
        FallbackProvider = provider;
        ::wil::SetResultTelemetryFallback(::Microsoft::Console::ErrorReporting::ReportFailureToFallbackProvider);
    }
}

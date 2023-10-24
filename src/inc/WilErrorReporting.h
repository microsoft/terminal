/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- WilErrorReporting.h
--*/
#pragma once

#include <winmeta.h>
#include <wil/common.h>

namespace Microsoft::Console::ErrorReporting
{
    __declspec(selectany) TraceLoggingHProvider FallbackProvider;
    __declspec(noinline) inline void WINAPI ReportFailureToFallbackProvider(bool alreadyReported, const wil::FailureInfo& failure) noexcept
    try
    {
        if (failure.hr == 0x80131515L)
        {
            // XAML requires that we reply with this HR for the accessibility code in XamlUiaTextRange to work.
            // Unfortunately, due to C++/WinRT, we have to _throw_ it. That results in us ending up here,
            // trying to report the error to telemetry. It's not an actual error, per se, so we don't
            // want to log it. It's also incredibly noisy, which results in bugs getting filed on us.
            // See https://github.com/microsoft/cppwinrt/issues/798 for more discussion about throwing HRESULTs.
            return;
        }

        if (!alreadyReported && FallbackProvider)
        {
#pragma warning(suppress : 26477) // Use 'nullptr' rather than 0 or NULL
            TraceLoggingWrite(
                FallbackProvider,
                "FallbackError",
                TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance),
                TraceLoggingStruct(14, "wilResult", "wilResult"),
                TraceLoggingUInt32(failure.hr, "hresult", "Failure error code"),
                TraceLoggingString(failure.pszFile, "fileName", "Source code file name where the error occurred"),
                TraceLoggingUInt32(failure.uLineNumber, "lineNumber", "Line number within the source code file where the error occurred"),
                TraceLoggingString(failure.pszModule, "module", "Name of the binary where the error occurred"),
                TraceLoggingUInt32(static_cast<DWORD>(failure.type), "failureType", "Indicates what type of failure was observed (exception, returned error, logged error or fail fast"),
                TraceLoggingWideString(failure.pszMessage, "message", "Custom message associated with the failure (if any)"),
                TraceLoggingUInt32(failure.threadId, "threadId", "Identifier of the thread the error occurred on"),
                TraceLoggingString(failure.pszCallContext, "callContext", "List of telemetry activities containing this error"),
                TraceLoggingUInt32(failure.callContextOriginating.contextId, "originatingContextId", "Identifier for the oldest telemetry activity containing this error"),
                TraceLoggingString(failure.callContextOriginating.contextName, "originatingContextName", "Name of the oldest telemetry activity containing this error"),
                TraceLoggingWideString(failure.callContextOriginating.contextMessage, "originatingContextMessage", "Custom message associated with the oldest telemetry activity containing this error (if any)"),
                TraceLoggingUInt32(failure.callContextCurrent.contextId, "currentContextId", "Identifier for the newest telemetry activity containing this error"),
                TraceLoggingString(failure.callContextCurrent.contextName, "currentContextName", "Name of the newest telemetry activity containing this error"),
                TraceLoggingWideString(failure.callContextCurrent.contextMessage, "currentContextMessage", "Custom message associated with the newest telemetry activity containing this error (if any)"));
        }
    }
    catch (...)
    {
        // Don't log anything. We just failed to trace, where will we go now?
    }

    __declspec(noinline) inline void EnableFallbackFailureReporting(TraceLoggingHProvider provider) noexcept
    try
    {
        FallbackProvider = provider;
        ::wil::SetResultTelemetryFallback(::Microsoft::Console::ErrorReporting::ReportFailureToFallbackProvider);
    }
    catch (...)
    {
        // Don't log anything. We just failed to set up WIL -- how are we going to log anything?
    }
}

/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- UiaTracing.hpp

Abstract:
- This module is used for recording tracing/debugging information to the telemetry ETW channel
- The data is not automatically broadcast to telemetry backends as it does not set the TELEMETRY keyword.
- NOTE: Many functions in this file appear to be copy/pastes. This is because the TraceLog documentation warns
        to not be "cute" in trying to reduce its macro usages with variables as it can cause unexpected behavior.

Author(s):
- Carlos Zamora (cazamor)     Feb 2020
--*/

#pragma once

#include <winmeta.h>
#include <TraceLoggingProvider.h>
#include "UiaTextRangeBase.hpp"
#include "ScreenInfoUiaProviderBase.h"

TRACELOGGING_DECLARE_PROVIDER(g_UiaProviderTraceProvider);

namespace Microsoft::Console::Types
{
    class UiaTracing final
    {
    public:
        class TextRange final
        {
        public:
            static void Constructor(const UiaTextRangeBase& result);
            static void Clone(const UiaTextRangeBase& base, const UiaTextRangeBase& result);
            static void Compare(const UiaTextRangeBase& base, const UiaTextRangeBase& other, bool result);
            static void CompareEndpoints(const UiaTextRangeBase& base, const TextPatternRangeEndpoint endpoint, const UiaTextRangeBase& other, TextPatternRangeEndpoint otherEndpoint, int result);
            static void ExpandToEnclosingUnit(TextUnit unit, const UiaTextRangeBase& result);
            static void FindAttribute(const UiaTextRangeBase& base);
            static void FindText(const UiaTextRangeBase& base, std::wstring text, bool searchBackward, bool ignoreCase, const UiaTextRangeBase& result);
            static void GetAttributeValue(const UiaTextRangeBase& base, TEXTATTRIBUTEID id, VARIANT result);
            static void GetBoundingRectangles(const UiaTextRangeBase& base);
            static void GetEnclosingElement(const UiaTextRangeBase& base, const ScreenInfoUiaProviderBase& siup);
            static void GetText(const UiaTextRangeBase& base, int maxLength, std::wstring result);
            static void Move(TextUnit unit, int count, int resultCount, const UiaTextRangeBase& result);
            static void MoveEndpointByUnit(TextPatternRangeEndpoint endpoint, TextUnit unit, int count, int resultCount, const UiaTextRangeBase& result);
            static void MoveEndpointByRange(TextPatternRangeEndpoint endpoint, const UiaTextRangeBase& other, TextPatternRangeEndpoint otherEndpoint, const UiaTextRangeBase& result);
            static void Select(const UiaTextRangeBase& base);
            static void AddToSelection(const UiaTextRangeBase& base);
            static void RemoveFromSelection(const UiaTextRangeBase& base);
            static void ScrollIntoView(bool alignToTop, const UiaTextRangeBase& result);
            static void GetChildren(const UiaTextRangeBase& base);
        };

        class TextProvider final
        {
        public:
            static void Constructor(const ScreenInfoUiaProviderBase& result);
            static void get_ProviderOptions(const ScreenInfoUiaProviderBase& base, ProviderOptions options);
            static void GetPatternProvider(const ScreenInfoUiaProviderBase& base, PATTERNID patternId);
            static void GetPropertyValue(const ScreenInfoUiaProviderBase& base, PROPERTYID propertyId);
            static void get_HostRawElementProvider(const ScreenInfoUiaProviderBase& base);
            static void GetRuntimeId(const ScreenInfoUiaProviderBase& base);
            static void GetEmbeddedFragmentRoots(const ScreenInfoUiaProviderBase& base);
            static void SetFocus(const ScreenInfoUiaProviderBase& base);
            static void GetSelection(const ScreenInfoUiaProviderBase& base);
            static void GetVisibleRanges(const ScreenInfoUiaProviderBase& base, const UiaTextRangeBase& result);
            static void RangeFromChild(const ScreenInfoUiaProviderBase& base, const UiaTextRangeBase& result);
            static void RangeFromPoint(const ScreenInfoUiaProviderBase& base, UiaPoint point, const UiaTextRangeBase& result);
            static void get_DocumentRange(const ScreenInfoUiaProviderBase& base, const UiaTextRangeBase& result);
            static void get_SupportedTextSelection(const ScreenInfoUiaProviderBase& base, SupportedTextSelection result);
        };

    private:
        // Implement this as a singleton class.
        static UiaTracing& EnsureRegistration() noexcept
        {
            static UiaTracing instance;
            return instance;
        }

        // Used to prevent multiple instances
        UiaTracing() noexcept;
        ~UiaTracing() noexcept;
        UiaTracing(UiaTracing const&) = delete;
        UiaTracing(UiaTracing&&) = delete;
        UiaTracing& operator=(const UiaTracing&) = delete;
        UiaTracing& operator=(UiaTracing&&) = delete;

        static std::wstring _getValue(const ScreenInfoUiaProviderBase& siup);
        static std::wstring _getValue(const UiaTextRangeBase& utr);
        static std::wstring _getValue(const TextPatternRangeEndpoint endpoint);
        static std::wstring _getValue(const TextUnit unit);
    };
}

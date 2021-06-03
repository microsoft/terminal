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
        static std::wstring convertAttributeId(const TEXTATTRIBUTEID attrId) noexcept;

        enum class AttributeType
        {
            Standard,
            Mixed,
            Unsupported,
            Error
        };

        class TextRange final
        {
        public:
            static void Constructor(UiaTextRangeBase& result) noexcept;
            static void Clone(const UiaTextRangeBase& base, UiaTextRangeBase& result) noexcept;
            static void Compare(const UiaTextRangeBase& base, const UiaTextRangeBase& other, bool result) noexcept;
            static void CompareEndpoints(const UiaTextRangeBase& base, const TextPatternRangeEndpoint endpoint, const UiaTextRangeBase& other, TextPatternRangeEndpoint otherEndpoint, int result) noexcept;
            static void ExpandToEnclosingUnit(TextUnit unit, const UiaTextRangeBase& result) noexcept;
            static void FindAttribute(const UiaTextRangeBase& base, TEXTATTRIBUTEID attributeId, VARIANT val, BOOL searchBackwards, const UiaTextRangeBase& result, AttributeType attrType = AttributeType::Standard) noexcept;
            static void FindText(const UiaTextRangeBase& base, std::wstring text, bool searchBackward, bool ignoreCase, const UiaTextRangeBase& result) noexcept;
            static void GetAttributeValue(const UiaTextRangeBase& base, TEXTATTRIBUTEID id, VARIANT result, AttributeType attrType = AttributeType::Standard) noexcept;
            static void GetBoundingRectangles(const UiaTextRangeBase& base) noexcept;
            static void GetEnclosingElement(const UiaTextRangeBase& base) noexcept;
            static void GetText(const UiaTextRangeBase& base, int maxLength, std::wstring result) noexcept;
            static void Move(TextUnit unit, int count, int resultCount, const UiaTextRangeBase& result) noexcept;
            static void MoveEndpointByUnit(TextPatternRangeEndpoint endpoint, TextUnit unit, int count, int resultCount, const UiaTextRangeBase& result) noexcept;
            static void MoveEndpointByRange(TextPatternRangeEndpoint endpoint, const UiaTextRangeBase& other, TextPatternRangeEndpoint otherEndpoint, const UiaTextRangeBase& result) noexcept;
            static void Select(const UiaTextRangeBase& base) noexcept;
            static void AddToSelection(const UiaTextRangeBase& base) noexcept;
            static void RemoveFromSelection(const UiaTextRangeBase& base) noexcept;
            static void ScrollIntoView(bool alignToTop, const UiaTextRangeBase& result) noexcept;
            static void GetChildren(const UiaTextRangeBase& base) noexcept;
        };

        class TextProvider final
        {
        public:
            static void Constructor(ScreenInfoUiaProviderBase& result) noexcept;
            static void get_ProviderOptions(const ScreenInfoUiaProviderBase& base, ProviderOptions options) noexcept;
            static void GetPatternProvider(const ScreenInfoUiaProviderBase& base, PATTERNID patternId) noexcept;
            static void GetPropertyValue(const ScreenInfoUiaProviderBase& base, PROPERTYID propertyId) noexcept;
            static void get_HostRawElementProvider(const ScreenInfoUiaProviderBase& base) noexcept;
            static void GetRuntimeId(const ScreenInfoUiaProviderBase& base) noexcept;
            static void GetEmbeddedFragmentRoots(const ScreenInfoUiaProviderBase& base) noexcept;
            static void SetFocus(const ScreenInfoUiaProviderBase& base) noexcept;
            static void GetSelection(const ScreenInfoUiaProviderBase& base, const UiaTextRangeBase& result) noexcept;
            static void GetVisibleRanges(const ScreenInfoUiaProviderBase& base, const UiaTextRangeBase& result) noexcept;
            static void RangeFromChild(const ScreenInfoUiaProviderBase& base, const UiaTextRangeBase& result) noexcept;
            static void RangeFromPoint(const ScreenInfoUiaProviderBase& base, UiaPoint point, const UiaTextRangeBase& result) noexcept;
            static void get_DocumentRange(const ScreenInfoUiaProviderBase& base, const UiaTextRangeBase& result) noexcept;
            static void get_SupportedTextSelection(const ScreenInfoUiaProviderBase& base, SupportedTextSelection result) noexcept;
        };

        class Signal final
        {
        public:
            static void SelectionChanged() noexcept;
            static void TextChanged() noexcept;
            static void CursorChanged() noexcept;
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

        static inline std::wstring _getValue(const ScreenInfoUiaProviderBase& siup) noexcept;
        static inline std::wstring _getValue(const UiaTextRangeBase& utr) noexcept;
        static inline std::wstring _getValue(const TextPatternRangeEndpoint endpoint) noexcept;
        static inline std::wstring _getValue(const TextUnit unit) noexcept;

        static inline std::wstring _getValue(const AttributeType attrType) noexcept;
        static inline std::wstring _getValue(const VARIANT val) noexcept;

        // these are used to assign IDs to new UiaTextRanges and ScreenInfoUiaProviders respectively
        static IdType _utrId;
        static IdType _siupId;

        static void _assignId(UiaTextRangeBase& utr) noexcept;
        static void _assignId(ScreenInfoUiaProviderBase& siup) noexcept;
    };
}

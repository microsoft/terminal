// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "UiaTextRangeBase.hpp"
#include "ScreenInfoUiaProviderBase.h"
#include "../buffer/out/search.h"
#include "UiaTracing.h"

using namespace Microsoft::Console::Types;

// Foreground/Background text color doesn't care about the alpha.
static constexpr long _RemoveAlpha(COLORREF color) noexcept
{
    return color & 0x00ffffff;
}

// degenerate range constructor.
#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT UiaTextRangeBase::RuntimeClassInitialize(_In_ Render::IRenderData* pData, _In_ IRawElementProviderSimple* const pProvider, _In_ std::wstring_view wordDelimiters) noexcept
try
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pProvider);
    RETURN_HR_IF_NULL(E_INVALIDARG, pData);

    _pProvider = pProvider;
    _pData = pData;
    _start = pData->GetViewport().Origin();
    _end = pData->GetViewport().Origin();
    _blockRange = false;
    _wordDelimiters = wordDelimiters;

    UiaTracing::TextRange::Constructor(*this);
    return S_OK;
}
CATCH_RETURN();

#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT UiaTextRangeBase::RuntimeClassInitialize(_In_ Render::IRenderData* pData,
                                                 _In_ IRawElementProviderSimple* const pProvider,
                                                 _In_ const Cursor& cursor,
                                                 _In_ std::wstring_view wordDelimiters) noexcept
try
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pData);
    RETURN_IF_FAILED(RuntimeClassInitialize(pData, pProvider, wordDelimiters));

    // GH#8730: The cursor position may be in a delayed state, resulting in it being out of bounds.
    // If that's the case, clamp it to be within bounds.
    // TODO GH#12440: We should be able to just check some fields off of the Cursor object,
    // but Windows Terminal isn't updating those flags properly.
    _start = cursor.GetPosition();
    pData->GetTextBuffer().GetSize().Clamp(_start);
    _end = _start;

    UiaTracing::TextRange::Constructor(*this);
    return S_OK;
}
CATCH_RETURN();

#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT UiaTextRangeBase::RuntimeClassInitialize(_In_ Render::IRenderData* pData,
                                                 _In_ IRawElementProviderSimple* const pProvider,
                                                 _In_ const til::point start,
                                                 _In_ const til::point end,
                                                 _In_ bool blockRange,
                                                 _In_ std::wstring_view wordDelimiters) noexcept
try
{
    RETURN_IF_FAILED(RuntimeClassInitialize(pData, pProvider, wordDelimiters));

    // start must be before or equal to end
    _start = std::min(start, end);
    _end = std::max(start, end);

    // This should be the only way to set if we are a blockRange
    // This is used for blockSelection
    _blockRange = blockRange;

    UiaTracing::TextRange::Constructor(*this);
    return S_OK;
}
CATCH_RETURN();

void UiaTextRangeBase::Initialize(_In_ const UiaPoint point)
{
    til::point clientPoint;
    clientPoint.x = static_cast<LONG>(point.x);
    clientPoint.y = static_cast<LONG>(point.y);
    // get row that point resides in
    const auto windowRect = _getTerminalRect();
    const auto viewport = _pData->GetViewport().ToInclusive();
    til::CoordType row = 0;
    if (clientPoint.y <= windowRect.top)
    {
        row = viewport.top;
    }
    else if (clientPoint.y >= windowRect.bottom)
    {
        row = viewport.bottom;
    }
    else
    {
        // change point coords to pixels relative to window
        _TranslatePointFromScreen(&clientPoint);

        const auto currentFontSize = _getScreenFontSize();
        row = clientPoint.y / currentFontSize.height + viewport.top;
    }
    _start = { 0, row };
    _end = _start;
}

#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT UiaTextRangeBase::RuntimeClassInitialize(const UiaTextRangeBase& a) noexcept
try
{
    _pProvider = a._pProvider;
    _start = a._start;
    _end = a._end;
    _pData = a._pData;
    _wordDelimiters = a._wordDelimiters;
    _blockRange = a._blockRange;

    UiaTracing::TextRange::Constructor(*this);
    return S_OK;
}
CATCH_RETURN();

til::point UiaTextRangeBase::GetEndpoint(TextPatternRangeEndpoint endpoint) const noexcept
{
    switch (endpoint)
    {
    case TextPatternRangeEndpoint_End:
        return _end;
    case TextPatternRangeEndpoint_Start:
    default:
        return _start;
    }
}

// Routine Description:
// - sets the target endpoint to the given til::point value
// - if the target endpoint crosses the other endpoint, become a degenerate range
// Arguments:
// - endpoint - the target endpoint (start or end)
// - val - the value that it will be set to
// Return Value:
// - true if range is degenerate, false otherwise.
bool UiaTextRangeBase::SetEndpoint(TextPatternRangeEndpoint endpoint, const til::point val) noexcept
{
    // GH#6402: Get the actual buffer size here, instead of the one
    //          constrained by the virtual bottom.
    const auto bufferSize = _pData->GetTextBuffer().GetSize();
    switch (endpoint)
    {
    case TextPatternRangeEndpoint_End:
        _end = val;
        // if start is past end, make this a degenerate range
        _start = std::min(_start, _end);
        break;
    case TextPatternRangeEndpoint_Start:
        _start = val;
        // if end is before start, make this a degenerate range
        _end = std::max(_start, _end);
        break;
    default:
        break;
    }
    return IsDegenerate();
}

// Routine Description:
// - returns true if the range is currently degenerate (empty range).
// Arguments:
// - <none>
// Return Value:
// - true if range is degenerate, false otherwise.
bool UiaTextRangeBase::IsDegenerate() const noexcept
{
    return _start == _end;
}

#pragma region ITextRangeProvider

IFACEMETHODIMP UiaTextRangeBase::Compare(_In_opt_ ITextRangeProvider* pRange, _Out_ BOOL* pRetVal) noexcept
{
    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });

    RETURN_HR_IF(E_INVALIDARG, pRetVal == nullptr);
    *pRetVal = FALSE;
    const UiaTextRangeBase* other = static_cast<UiaTextRangeBase*>(pRange);
    if (other)
    {
        *pRetVal = (_start == other->GetEndpoint(TextPatternRangeEndpoint_Start) &&
                    _end == other->GetEndpoint(TextPatternRangeEndpoint_End));
    }

    UiaTracing::TextRange::Compare(*this, *other, *pRetVal);
    return S_OK;
}

IFACEMETHODIMP UiaTextRangeBase::CompareEndpoints(_In_ TextPatternRangeEndpoint endpoint,
                                                  _In_ ITextRangeProvider* pTargetRange,
                                                  _In_ TextPatternRangeEndpoint targetEndpoint,
                                                  _Out_ int* pRetVal) noexcept
try
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pRetVal);
    *pRetVal = 0;

    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });
    RETURN_HR_IF(E_FAIL, !_pData->IsUiaDataInitialized());

    // get the text range that we're comparing to
    const UiaTextRangeBase* range = static_cast<UiaTextRangeBase*>(pTargetRange);
    RETURN_HR_IF_NULL(E_INVALIDARG, range);

    // get endpoint value that we're comparing to
    const auto other = range->GetEndpoint(targetEndpoint);

    // get the values of our endpoint
    const auto mine = GetEndpoint(endpoint);

    // TODO GH#5406: create a different UIA parent object for each TextBuffer
    //   This is a temporary solution to comparing two UTRs from different TextBuffers
    //   Ensure both endpoints fit in the current buffer.
    const auto bufferSize = _pData->GetTextBuffer().GetSize();
    RETURN_HR_IF(E_FAIL, !bufferSize.IsInBounds(mine, true) || !bufferSize.IsInBounds(other, true));

    // compare them
    *pRetVal = bufferSize.CompareInBounds(mine, other, true);

    UiaTracing::TextRange::CompareEndpoints(*this, endpoint, *range, targetEndpoint, *pRetVal);
    return S_OK;
}
CATCH_RETURN();

IFACEMETHODIMP UiaTextRangeBase::ExpandToEnclosingUnit(_In_ TextUnit unit) noexcept
{
    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });
    RETURN_HR_IF(E_FAIL, !_pData->IsUiaDataInitialized());

    try
    {
        _expandToEnclosingUnit(unit);
        UiaTracing::TextRange::ExpandToEnclosingUnit(unit, *this);
        return S_OK;
    }
    CATCH_RETURN();
}

// Method Description:
// - Moves _start and _end endpoints to encompass the enclosing text unit.
//   (i.e. word --> enclosing word, line --> enclosing line)
// - IMPORTANT: this does _not_ lock the console
// Arguments:
// - attributeId - the UIA text attribute identifier we're expanding by
// Return Value:
// - <none>
void UiaTextRangeBase::_expandToEnclosingUnit(TextUnit unit)
{
    const auto& buffer = _pData->GetTextBuffer();
    const auto bufferSize{ buffer.GetSize() };
    const auto documentEnd{ _getDocumentEnd() };

    // If we're past document end,
    // set us to ONE BEFORE the document end.
    // This allows us to expand properly.
    if (_start >= documentEnd)
    {
        _start = documentEnd;
        bufferSize.DecrementInBounds(_start, true);
    }

    if (unit == TextUnit_Character)
    {
        _start = buffer.GetGlyphStart(_start, documentEnd);
        _end = buffer.GetGlyphEnd(_start, true, documentEnd);
    }
    else if (unit <= TextUnit_Word)
    {
        // expand to word
        _start = buffer.GetWordStart(_start, _wordDelimiters, true, documentEnd);
        _end = buffer.GetWordEnd(_start, _wordDelimiters, true, documentEnd);
    }
    else if (unit <= TextUnit_Line)
    {
        // expand to line
        _start.x = 0;
        if (_start.y == documentEnd.y)
        {
            // we're on the last line
            _end = documentEnd;
            bufferSize.IncrementInBounds(_end, true);
        }
        else
        {
            _end.x = 0;
            _end.y = _start.y + 1;
        }
    }
    else
    {
        // expand to document
        _start = bufferSize.Origin();
        _end = documentEnd;
    }
}

// Method Description:
// - Verify that the given attribute has the desired formatting saved in the attributeId and val
// Arguments:
// - attributeId - the UIA text attribute identifier we're looking for
// - val - the attributeId's sub-type we're looking for
// - attr - the text attribute we're checking
// Return Value:
// - true, if the given attribute has the desired formatting.
// - false, if the given attribute does not have the desired formatting.
// - nullopt, if checking for the desired formatting is not supported.
std::optional<bool> UiaTextRangeBase::_verifyAttr(TEXTATTRIBUTEID attributeId, VARIANT val, const TextAttribute& attr) const
{
    // Most of the attributes we're looking for just require us to check TextAttribute.
    // So if we support it, we'll return a function to verify if the TextAttribute
    // has the desired attribute.
    switch (attributeId)
    {
    case UIA_BackgroundColorAttributeId:
    {
        // Expected type: VT_I4
        THROW_HR_IF(E_INVALIDARG, val.vt != VT_I4);

        // The foreground color is stored as a COLORREF.
        const auto queryBackgroundColor{ val.lVal };
        return _RemoveAlpha(_pData->GetAttributeColors(attr).second) == queryBackgroundColor;
    }
    case UIA_FontWeightAttributeId:
    {
        // Expected type: VT_I4
        THROW_HR_IF(E_INVALIDARG, val.vt != VT_I4);

        // The font weight can be any value from 0 to 900.
        // The text buffer doesn't store the actual value,
        // we just store "IsIntense" and "IsFaint".
        const auto queryFontWeight{ val.lVal };

        if (queryFontWeight > FW_NORMAL)
        {
            // we're looking for a bold font weight
            return attr.IsIntense();
        }
        else
        {
            // we're looking for "normal" font weight
            return !attr.IsIntense();
        }
    }
    case UIA_ForegroundColorAttributeId:
    {
        // Expected type: VT_I4
        THROW_HR_IF(E_INVALIDARG, val.vt != VT_I4);

        // The foreground color is stored as a COLORREF.
        const auto queryForegroundColor{ val.lVal };
        return _RemoveAlpha(_pData->GetAttributeColors(attr).first) == queryForegroundColor;
    }
    case UIA_IsItalicAttributeId:
    {
        // Expected type: VT_I4
        THROW_HR_IF(E_INVALIDARG, val.vt != VT_BOOL);

        // The text is either italic or it isn't.
        const auto queryIsItalic{ val.boolVal };
        return queryIsItalic ? attr.IsItalic() : !attr.IsItalic();
    }
    case UIA_StrikethroughStyleAttributeId:
    {
        // Expected type: VT_I4
        THROW_HR_IF(E_INVALIDARG, val.vt != VT_I4);

        // The strikethrough style is stored as a TextDecorationLineStyle.
        // However, The text buffer doesn't have different styles for being crossed out.
        // Instead, we just store whether or not the text is crossed out.
        switch (val.lVal)
        {
        case TextDecorationLineStyle_None:
            return !attr.IsCrossedOut();
        case TextDecorationLineStyle_Single:
            return attr.IsCrossedOut();
        default:
            return std::nullopt;
        }
    }
    case UIA_UnderlineStyleAttributeId:
    {
        // Expected type: VT_I4
        THROW_HR_IF(E_INVALIDARG, val.vt != VT_I4);

        // The underline style is stored as a TextDecorationLineStyle.
        // However, The text buffer doesn't have that many different styles for being underlined.
        // Instead, we only have single and double underlined.
        switch (val.lVal)
        {
        case TextDecorationLineStyle_None:
            return !attr.IsUnderlined() && !attr.IsDoublyUnderlined();
        case TextDecorationLineStyle_Double:
            return attr.IsDoublyUnderlined();
        case TextDecorationLineStyle_Single:
            return attr.IsUnderlined();
        default:
            return std::nullopt;
        }
    }
    default:
        return std::nullopt;
    }
}

IFACEMETHODIMP UiaTextRangeBase::FindAttribute(_In_ TEXTATTRIBUTEID attributeId,
                                               _In_ VARIANT val,
                                               _In_ BOOL searchBackwards,
                                               _Outptr_result_maybenull_ ITextRangeProvider** ppRetVal) noexcept
try
{
    RETURN_HR_IF(E_INVALIDARG, ppRetVal == nullptr);
    *ppRetVal = nullptr;

    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });
    RETURN_HR_IF(E_FAIL, !_pData->IsUiaDataInitialized());

    // AttributeIDs that require special handling
    switch (attributeId)
    {
    case UIA_FontNameAttributeId:
    {
        RETURN_HR_IF(E_INVALIDARG, val.vt != VT_BSTR);

        // Technically, we'll truncate early if there's an embedded null in the BSTR.
        // But we're probably fine in this circumstance.

        const std::wstring queryFontName{ val.bstrVal };
        if (queryFontName == _pData->GetFontInfo().GetFaceName())
        {
            Clone(ppRetVal);
        }
        UiaTracing::TextRange::FindAttribute(*this, attributeId, val, searchBackwards, static_cast<UiaTextRangeBase&>(**ppRetVal));
        return S_OK;
    }
    case UIA_IsReadOnlyAttributeId:
    {
        RETURN_HR_IF(E_INVALIDARG, val.vt != VT_BOOL);
        if (!val.boolVal)
        {
            Clone(ppRetVal);
        }
        UiaTracing::TextRange::FindAttribute(*this, attributeId, val, searchBackwards, static_cast<UiaTextRangeBase&>(**ppRetVal));
        return S_OK;
    }
    default:
        break;
    }

    // AttributeIDs that are exposed via TextAttribute
    try
    {
        if (!_verifyAttr(attributeId, val, {}).has_value())
        {
            // The AttributeID is not supported.
            UiaTracing::TextRange::FindAttribute(*this, attributeId, val, searchBackwards, static_cast<UiaTextRangeBase&>(**ppRetVal), UiaTracing::AttributeType::Unsupported);
            return E_NOTIMPL;
        }
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
        UiaTracing::TextRange::FindAttribute(*this, attributeId, val, searchBackwards, static_cast<UiaTextRangeBase&>(**ppRetVal), UiaTracing::AttributeType::Error);
        return E_INVALIDARG;
    }

    // Get some useful variables
    const auto& buffer{ _pData->GetTextBuffer() };
    const auto bufferSize{ buffer.GetSize() };
    const auto inclusiveEnd{ _getInclusiveEnd() };

    // Start/End for the resulting range.
    // NOTE: we store these as "first" and "second" anchor because,
    //       we just want to know what the inclusive range is.
    //       We'll do some post-processing to fix this on the way out.
    std::optional<til::point> resultFirstAnchor;
    std::optional<til::point> resultSecondAnchor;
    const auto attemptUpdateAnchors = [=, &resultFirstAnchor, &resultSecondAnchor](const TextBufferCellIterator iter) {
        const auto attrFound{ _verifyAttr(attributeId, val, iter->TextAttr()).value() };
        if (attrFound)
        {
            // populate the first anchor if it's not populated.
            // otherwise, populate the second anchor.
            if (!resultFirstAnchor.has_value())
            {
                resultFirstAnchor = iter.Pos();
                resultSecondAnchor = iter.Pos();
            }
            else
            {
                resultSecondAnchor = iter.Pos();
            }
        }
        return attrFound;
    };

    // Start/End for the direction to perform the search in
    // We need searchEnd to be exclusive. This allows the for-loop below to
    // iterate up until the exclusive searchEnd, and not attempt to read the
    // data at that position.
    const auto searchStart{ searchBackwards ? inclusiveEnd : _start };
    const auto searchEndInclusive{ searchBackwards ? _start : inclusiveEnd };
    auto searchEndExclusive{ searchEndInclusive };
    if (searchBackwards)
    {
        bufferSize.DecrementInBounds(searchEndExclusive, true);
    }
    else
    {
        bufferSize.IncrementInBounds(searchEndExclusive, true);
    }

    // Iterate from searchStart to searchEnd in the buffer.
    // If we find the attribute we're looking for, we update resultFirstAnchor/SecondAnchor appropriately.
#pragma warning(suppress : 26496) // TRANSITIONAL: false positive in VS 16.11
    auto viewportRange{ bufferSize };
    if (_blockRange)
    {
        const auto originX{ std::min(_start.x, inclusiveEnd.x) };
        const auto originY{ std::min(_start.y, inclusiveEnd.y) };
        const auto width{ std::abs(inclusiveEnd.x - _start.x + 1) };
        const auto height{ std::abs(inclusiveEnd.y - _start.y + 1) };
        viewportRange = Viewport::FromDimensions({ originX, originY }, width, height);
    }
    auto iter{ buffer.GetCellDataAt(searchStart, viewportRange) };
    const auto iterStep{ searchBackwards ? -1 : 1 };
    for (; iter && iter.Pos() != searchEndExclusive; iter += iterStep)
    {
        if (!attemptUpdateAnchors(iter) && resultFirstAnchor.has_value() && resultSecondAnchor.has_value())
        {
            // Exit the loop early if...
            // - the cell we're looking at doesn't have the attr we're looking for
            // - the anchors have been populated
            // This means that we've found a contiguous range where the text attribute was found.
            // No point in searching through the rest of the search space.
            // TLDR: keep updating the second anchor and make the range wider until the attribute changes.
            break;
        }
    }

    // Corner case: we couldn't actually move the searchEnd to make it exclusive
    // (i.e. DecrementInBounds on Origin doesn't move it)
    if (searchEndInclusive == searchEndExclusive)
    {
        attemptUpdateAnchors(iter);
    }

    // If a result was found, populate ppRetVal with the UiaTextRange
    // representing the found selection anchors.
    if (resultFirstAnchor.has_value() && resultSecondAnchor.has_value())
    {
        RETURN_IF_FAILED(Clone(ppRetVal));
        auto& range = static_cast<UiaTextRangeBase&>(**ppRetVal);

        // IMPORTANT: resultFirstAnchor and resultSecondAnchor make up an inclusive range.
        range._start = searchBackwards ? *resultSecondAnchor : *resultFirstAnchor;
        range._end = searchBackwards ? *resultFirstAnchor : *resultSecondAnchor;

        // We need to make the end exclusive!
        // But be careful here, we might be a block range
        auto exclusiveIter{ buffer.GetCellDataAt(range._end, viewportRange) };
        ++exclusiveIter;
        range._end = exclusiveIter.Pos();
    }

    UiaTracing::TextRange::FindAttribute(*this, attributeId, val, searchBackwards, static_cast<UiaTextRangeBase&>(**ppRetVal));
    return S_OK;
}
CATCH_RETURN();

IFACEMETHODIMP UiaTextRangeBase::FindText(_In_ BSTR text,
                                          _In_ BOOL searchBackward,
                                          _In_ BOOL ignoreCase,
                                          _Outptr_result_maybenull_ ITextRangeProvider** ppRetVal) noexcept
try
{
    RETURN_HR_IF(E_INVALIDARG, ppRetVal == nullptr);
    *ppRetVal = nullptr;

    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });
    RETURN_HR_IF(E_FAIL, !_pData->IsUiaDataInitialized());

    const std::wstring queryText{ text, SysStringLen(text) };
    const auto bufferSize = _getOptimizedBufferSize();
    const auto sensitivity = ignoreCase ? Search::Sensitivity::CaseInsensitive : Search::Sensitivity::CaseSensitive;

    auto searchDirection = Search::Direction::Forward;
    auto searchAnchor = _start;
    if (searchBackward)
    {
        searchDirection = Search::Direction::Backward;

        // we need to convert the end to inclusive
        // because Search operates with an inclusive til::point
        searchAnchor = _end;
        bufferSize.DecrementInBounds(searchAnchor, true);
    }

    Search searcher{ *_pData, queryText, searchDirection, sensitivity, searchAnchor };

    if (searcher.FindNext())
    {
        const auto foundLocation = searcher.GetFoundLocation();
        const auto start = foundLocation.first;

        // we need to increment the position of end because it's exclusive
        auto end = foundLocation.second;
        bufferSize.IncrementInBounds(end, true);

        // make sure what was found is within the bounds of the current range
        if ((searchDirection == Search::Direction::Forward && end < _end) ||
            (searchDirection == Search::Direction::Backward && start > _start))
        {
            RETURN_IF_FAILED(Clone(ppRetVal));
            auto& range = static_cast<UiaTextRangeBase&>(**ppRetVal);
            range._start = start;
            range._end = end;

            UiaTracing::TextRange::FindText(*this, queryText, searchBackward, ignoreCase, range);
        }
    }
    return S_OK;
}
CATCH_RETURN();

// Method Description:
// - (1) Checks the current range for the attributeId's sub-type
// - (2) Record the attributeId's sub-type
// Arguments:
// - attributeId - the UIA text attribute identifier we're looking for
// - pRetVal - the attributeId's sub-type for the first cell in the range (i.e. foreground color)
// - attr - the text attribute we're checking
// Return Value:
// - true, if the attributeId is supported. false, otherwise.
// - pRetVal is populated with the appropriate response relevant to the returned bool.
bool UiaTextRangeBase::_initializeAttrQuery(TEXTATTRIBUTEID attributeId, VARIANT* pRetVal, const TextAttribute& attr) const
{
    THROW_HR_IF(E_INVALIDARG, pRetVal == nullptr);

    switch (attributeId)
    {
    case UIA_BackgroundColorAttributeId:
    {
        pRetVal->vt = VT_I4;
        pRetVal->lVal = _RemoveAlpha(_pData->GetAttributeColors(attr).second);
        return true;
    }
    case UIA_FontWeightAttributeId:
    {
        // The font weight can be any value from 0 to 900.
        // The text buffer doesn't store the actual value,
        // we just store "IsIntense" and "IsFaint".
        // Source: https://docs.microsoft.com/en-us/windows/win32/winauto/uiauto-textattribute-ids
        pRetVal->vt = VT_I4;
        pRetVal->lVal = attr.IsIntense() ? FW_BOLD : FW_NORMAL;
        return true;
    }
    case UIA_ForegroundColorAttributeId:
    {
        pRetVal->vt = VT_I4;
        pRetVal->lVal = _RemoveAlpha(_pData->GetAttributeColors(attr).first);
        return true;
    }
    case UIA_IsItalicAttributeId:
    {
        pRetVal->vt = VT_BOOL;
        pRetVal->boolVal = attr.IsItalic();
        return true;
    }
    case UIA_StrikethroughStyleAttributeId:
    {
        pRetVal->vt = VT_I4;
        pRetVal->lVal = attr.IsCrossedOut() ? TextDecorationLineStyle_Single : TextDecorationLineStyle_None;
        return true;
    }
    case UIA_UnderlineStyleAttributeId:
    {
        pRetVal->vt = VT_I4;
        if (attr.IsDoublyUnderlined())
        {
            pRetVal->lVal = TextDecorationLineStyle_Double;
        }
        else if (attr.IsUnderlined())
        {
            pRetVal->lVal = TextDecorationLineStyle_Single;
        }
        else
        {
            pRetVal->lVal = TextDecorationLineStyle_None;
        }
        return true;
    }
    default:
        // This attribute is not supported.
        pRetVal->vt = VT_UNKNOWN;
        UiaGetReservedNotSupportedValue(&pRetVal->punkVal);
        return false;
    }
}

IFACEMETHODIMP UiaTextRangeBase::GetAttributeValue(_In_ TEXTATTRIBUTEID attributeId,
                                                   _Out_ VARIANT* pRetVal) noexcept
try
{
    RETURN_HR_IF(E_INVALIDARG, pRetVal == nullptr);
    VariantInit(pRetVal);

    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });
    RETURN_HR_IF(E_FAIL, !_pData->IsUiaDataInitialized());

    // AttributeIDs that require special handling
    switch (attributeId)
    {
    case UIA_FontNameAttributeId:
    {
        pRetVal->vt = VT_BSTR;
        pRetVal->bstrVal = SysAllocString(_pData->GetFontInfo().GetFaceName().data());
        UiaTracing::TextRange::GetAttributeValue(*this, attributeId, *pRetVal);
        return S_OK;
    }
    case UIA_IsReadOnlyAttributeId:
    {
        pRetVal->vt = VT_BOOL;
        pRetVal->boolVal = VARIANT_FALSE;
        UiaTracing::TextRange::GetAttributeValue(*this, attributeId, *pRetVal);
        return S_OK;
    }
    default:
        break;
    }

    // AttributeIDs that are exposed via TextAttribute
    try
    {
        // Unlike a normal text editor, which applies formatting at the caret,
        // we don't know what attributes are written at a degenerate range.
        // So instead, we'll use GetCurrentAttributes to get an idea of the default
        // text attributes used. And return a result based off of that.
        const auto attr{ IsDegenerate() ? _pData->GetTextBuffer().GetCurrentAttributes() :
                                          _pData->GetTextBuffer().GetCellDataAt(_start)->TextAttr() };
        if (!_initializeAttrQuery(attributeId, pRetVal, attr))
        {
            // The AttributeID is not supported.
            pRetVal->vt = VT_UNKNOWN;
            UiaTracing::TextRange::GetAttributeValue(*this, attributeId, *pRetVal, UiaTracing::AttributeType::Unsupported);
            return UiaGetReservedNotSupportedValue(&pRetVal->punkVal);
        }
        else if (IsDegenerate())
        {
            // If we're a degenerate range, we have all the information we need.
            UiaTracing::TextRange::GetAttributeValue(*this, attributeId, *pRetVal);
            return S_OK;
        }
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
        UiaTracing::TextRange::GetAttributeValue(*this, attributeId, *pRetVal, UiaTracing::AttributeType::Error);
        return E_INVALIDARG;
    }

    // Get some useful variables
    const auto& buffer{ _pData->GetTextBuffer() };
    const auto bufferSize{ buffer.GetSize() };
    const auto inclusiveEnd{ _getInclusiveEnd() };

    // Check if the entire text range has that text attribute
#pragma warning(suppress : 26496) // TRANSITIONAL: false positive in VS 16.11
    auto viewportRange{ bufferSize };
    if (_blockRange)
    {
        const auto originX{ std::min(_start.x, inclusiveEnd.x) };
        const auto originY{ std::min(_start.y, inclusiveEnd.y) };
        const auto width{ std::abs(inclusiveEnd.x - _start.x + 1) };
        const auto height{ std::abs(inclusiveEnd.y - _start.y + 1) };
        viewportRange = Viewport::FromDimensions({ originX, originY }, width, height);
    }
    auto iter{ buffer.GetCellDataAt(_start, viewportRange) };
    for (; iter && iter.Pos() != inclusiveEnd; ++iter)
    {
        if (!_verifyAttr(attributeId, *pRetVal, iter->TextAttr()).value())
        {
            // The value of the specified attribute varies over the text range
            // return UiaGetReservedMixedAttributeValue.
            // Source: https://docs.microsoft.com/en-us/windows/win32/api/uiautomationcore/nf-uiautomationcore-itextrangeprovider-getattributevalue
            pRetVal->vt = VT_UNKNOWN;
            UiaTracing::TextRange::GetAttributeValue(*this, attributeId, *pRetVal, UiaTracing::AttributeType::Mixed);
            return UiaGetReservedMixedAttributeValue(&pRetVal->punkVal);
        }
    }

    UiaTracing::TextRange::GetAttributeValue(*this, attributeId, *pRetVal);
    return S_OK;
}
CATCH_RETURN();

IFACEMETHODIMP UiaTextRangeBase::GetBoundingRectangles(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal) noexcept
{
    RETURN_HR_IF(E_INVALIDARG, ppRetVal == nullptr);
    *ppRetVal = nullptr;

    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });
    RETURN_HR_IF(E_FAIL, !_pData->IsUiaDataInitialized());

    try
    {
        // vector to put coords into. they go in as four doubles in the
        // order: left, top, width, height. each line will have its own
        // set of coords.
        std::vector<double> coords;

        // GH#6402: Get the actual buffer size here, instead of the one
        //          constrained by the virtual bottom.
        const auto& buffer = _pData->GetTextBuffer();
        const auto bufferSize = buffer.GetSize();

        // these viewport vars are converted to the buffer coordinate space
        const auto viewport = bufferSize.ConvertToOrigin(_pData->GetViewport());
        const auto viewportOrigin = viewport.Origin();
        const auto viewportEnd = viewport.EndExclusive();

        // startAnchor: the earliest til::point we will get a bounding rect for; at least the viewport origin
        const auto startAnchor = std::max(GetEndpoint(TextPatternRangeEndpoint_Start), viewportOrigin);

        // endAnchor: the latest til::point we will get a bounding rect for; at most the viewport end
        auto endAnchor = std::min(GetEndpoint(TextPatternRangeEndpoint_End), viewportEnd);

        // _end is exclusive, let's be inclusive so we don't have to think about it anymore for bounding rects
        bufferSize.DecrementInBounds(endAnchor, true);

        if (IsDegenerate() || _start > viewportEnd || _end < viewportOrigin)
        {
            // An empty array is returned for a degenerate (empty) text range.
            // reference: https://docs.microsoft.com/en-us/windows/win32/api/uiautomationclient/nf-uiautomationclient-iuiautomationtextrange-getboundingrectangles

            // Remember, start cannot be past end, so
            //   if start is past the viewport end,
            //   or end is past the viewport origin
            //   draw nothing
        }
        else
        {
            const auto textRects = buffer.GetTextRects(startAnchor, endAnchor, _blockRange, true);

            for (const auto& rect : textRects)
            {
                // Convert the buffer coordinates to an equivalent range of
                // screen cells, taking line rendition into account.
                const auto lineRendition = buffer.GetLineRendition(rect.top);
                til::rect r{ BufferToScreenLine(rect, lineRendition) };
                r -= viewportOrigin;
                _getBoundingRect(r, coords);
            }
        }

        // convert to a safearray
        *ppRetVal = SafeArrayCreateVector(VT_R8, 0, gsl::narrow<ULONG>(coords.size()));
        if (*ppRetVal == nullptr)
        {
            return E_OUTOFMEMORY;
        }
        auto hr = E_UNEXPECTED;
        const auto l = gsl::narrow<LONG>(coords.size());
        for (LONG i = 0; i < l; ++i)
        {
            hr = SafeArrayPutElement(*ppRetVal, &i, &coords.at(i));
            if (FAILED(hr))
            {
                SafeArrayDestroy(*ppRetVal);
                *ppRetVal = nullptr;
                return hr;
            }
        }
    }
    CATCH_RETURN();

    UiaTracing::TextRange::GetBoundingRectangles(*this);
    return S_OK;
}

IFACEMETHODIMP UiaTextRangeBase::GetEnclosingElement(_Outptr_result_maybenull_ IRawElementProviderSimple** ppRetVal) noexcept
try
{
    RETURN_HR_IF(E_INVALIDARG, ppRetVal == nullptr);
    *ppRetVal = nullptr;

    const auto hr = _pProvider->QueryInterface(IID_PPV_ARGS(ppRetVal));
    UiaTracing::TextRange::GetEnclosingElement(*this);
    return hr;
}
CATCH_RETURN();

IFACEMETHODIMP UiaTextRangeBase::GetText(_In_ int maxLength, _Out_ BSTR* pRetVal) noexcept
try
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pRetVal);
    RETURN_HR_IF(E_INVALIDARG, maxLength < -1);
    *pRetVal = nullptr;

    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });
    RETURN_HR_IF(E_FAIL, !_pData->IsUiaDataInitialized());

    const auto text = _getTextValue(maxLength);
    Unlock.reset();

    *pRetVal = SysAllocString(text.c_str());
    RETURN_HR_IF_NULL(E_OUTOFMEMORY, *pRetVal);

    UiaTracing::TextRange::GetText(*this, maxLength, text);
    return S_OK;
}
CATCH_RETURN();

// Method Description:
// - Helper method for GetText(). Retrieves the text that the UiaTextRange encompasses as a wstring
// Arguments:
// - maxLength - the maximum size of the retrieved text. nullopt means we don't care about the size.
// Return Value:
// - the text that the UiaTextRange encompasses
#pragma warning(push)
#pragma warning(disable : 26447) // compiler isn't filtering throws inside the try/catch
std::wstring UiaTextRangeBase::_getTextValue(til::CoordType maxLength) const
{
    std::wstring textData{};
    if (!IsDegenerate())
    {
        const auto& buffer = _pData->GetTextBuffer();
        const auto bufferSize = buffer.GetSize();

        // -1 is supposed to be interpreted as "no limit",
        // so leverage size_t(-1) being converted to 0xffff...
        const auto maxLengthAsSize = gsl::narrow_cast<size_t>(maxLength);

        // TODO GH#5406: create a different UIA parent object for each TextBuffer
        // nvaccess/nvda#11428: Ensure our endpoints are in bounds
        THROW_HR_IF(E_FAIL, !bufferSize.IsInBounds(_start, true) || !bufferSize.IsInBounds(_end, true));

        // convert _end to be inclusive
        auto inclusiveEnd = _end;
        bufferSize.DecrementInBounds(inclusiveEnd, true);

        // reserve size in accordance to extracted text
        const auto textRects = buffer.GetTextRects(_start, inclusiveEnd, _blockRange, true);
        const auto bufferData = buffer.GetText(true,
                                               false,
                                               textRects);
        const size_t textDataSize = bufferData.text.size() * bufferSize.Width();
        textData.reserve(textDataSize);
        for (const auto& text : bufferData.text)
        {
            if (textData.size() >= maxLengthAsSize)
            {
                // early exit; we're already at/past max length
                break;
            }
            textData += text;
        }

        // only use maxLength to resize down.
        // if maxLength > size, we don't want to resize and append unnecessary L'\0'.
        if (textData.size() > maxLengthAsSize)
        {
            textData.resize(maxLengthAsSize);
        }
    }

    return textData;
}
#pragma warning(pop)

IFACEMETHODIMP UiaTextRangeBase::Move(_In_ TextUnit unit,
                                      _In_ int count,
                                      _Out_ int* pRetVal) noexcept
try
{
    RETURN_HR_IF(E_INVALIDARG, pRetVal == nullptr);
    *pRetVal = 0;

    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });
    RETURN_HR_IF(E_FAIL, !_pData->IsUiaDataInitialized());

    // We can abstract this movement by moving _start
    // GH#7342: check if we're past the documentEnd
    // If so, clamp each endpoint to the end of the document.
    constexpr auto endpoint = TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start;
    const auto bufferSize{ _pData->GetTextBuffer().GetSize() };
    const auto documentEnd = _getDocumentEnd();
    _start = std::min(_start, documentEnd);
    _end = std::min(_end, documentEnd);

    const auto wasDegenerate = IsDegenerate();
    if (count != 0)
    {
        const auto preventBoundary = !wasDegenerate;
        if (unit == TextUnit::TextUnit_Character)
        {
            _moveEndpointByUnitCharacter(count, endpoint, pRetVal, preventBoundary);
        }
        else if (unit <= TextUnit::TextUnit_Word)
        {
            _moveEndpointByUnitWord(count, endpoint, pRetVal, preventBoundary);
        }
        else if (unit <= TextUnit::TextUnit_Line)
        {
            _moveEndpointByUnitLine(count, endpoint, pRetVal, preventBoundary);
        }
        else if (unit <= TextUnit::TextUnit_Document)
        {
            _moveEndpointByUnitDocument(count, endpoint, pRetVal, preventBoundary);
        }
    }

    if (wasDegenerate)
    {
        // GH#7342: The range was degenerate before the move.
        // To keep it that way, move _end to the new _start.
        _end = _start;
    }
    else
    {
        // then just expand to get our _end
        _expandToEnclosingUnit(unit);
    }

    UiaTracing::TextRange::Move(unit, count, *pRetVal, *this);
    return S_OK;
}
CATCH_RETURN();

IFACEMETHODIMP UiaTextRangeBase::MoveEndpointByUnit(_In_ TextPatternRangeEndpoint endpoint,
                                                    _In_ TextUnit unit,
                                                    _In_ int count,
                                                    _Out_ int* pRetVal) noexcept
{
    RETURN_HR_IF(E_INVALIDARG, pRetVal == nullptr);
    *pRetVal = 0;

    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });
    RETURN_HR_IF(E_FAIL, !_pData->IsUiaDataInitialized());
    RETURN_HR_IF(S_OK, count == 0);

    // GH#7342: check if we're past the documentEnd
    // If so, clamp each endpoint to the end of the document.
    const auto bufferSize{ _pData->GetTextBuffer().GetSize() };

    auto documentEnd = bufferSize.EndExclusive();
    try
    {
        documentEnd = _getDocumentEnd();
    }
    CATCH_LOG();

    _start = std::min(_start, documentEnd);
    _end = std::min(_end, documentEnd);

    try
    {
        if (unit == TextUnit::TextUnit_Character)
        {
            _moveEndpointByUnitCharacter(count, endpoint, pRetVal);
        }
        else if (unit <= TextUnit::TextUnit_Word)
        {
            _moveEndpointByUnitWord(count, endpoint, pRetVal);
        }
        else if (unit <= TextUnit::TextUnit_Line)
        {
            _moveEndpointByUnitLine(count, endpoint, pRetVal);
        }
        else if (unit <= TextUnit::TextUnit_Document)
        {
            _moveEndpointByUnitDocument(count, endpoint, pRetVal);
        }
    }
    CATCH_RETURN();

    UiaTracing::TextRange::MoveEndpointByUnit(endpoint, unit, count, *pRetVal, *this);
    return S_OK;
}

IFACEMETHODIMP UiaTextRangeBase::MoveEndpointByRange(_In_ TextPatternRangeEndpoint endpoint,
                                                     _In_ ITextRangeProvider* pTargetRange,
                                                     _In_ TextPatternRangeEndpoint targetEndpoint) noexcept
try
{
    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });

    const UiaTextRangeBase* range = static_cast<UiaTextRangeBase*>(pTargetRange);
    RETURN_HR_IF_NULL(E_INVALIDARG, range);
    RETURN_HR_IF(E_FAIL, !_pData->IsUiaDataInitialized());

    // TODO GH#5406: create a different UIA parent object for each TextBuffer
    //   This is a temporary solution to comparing two UTRs from different TextBuffers
    //   Ensure both endpoints fit in the current buffer.
    const auto bufferSize = _pData->GetTextBuffer().GetSize();
    const auto mine = GetEndpoint(endpoint);
    const auto other = range->GetEndpoint(targetEndpoint);
    RETURN_HR_IF(E_FAIL, !bufferSize.IsInBounds(mine, true) || !bufferSize.IsInBounds(other, true));

    SetEndpoint(endpoint, range->GetEndpoint(targetEndpoint));

    UiaTracing::TextRange::MoveEndpointByRange(endpoint, *range, targetEndpoint, *this);
    return S_OK;
}
CATCH_RETURN();

IFACEMETHODIMP UiaTextRangeBase::Select() noexcept
try
{
    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });
    RETURN_HR_IF(E_FAIL, !_pData->IsUiaDataInitialized());

    if (IsDegenerate())
    {
        // calling Select on a degenerate range should clear any current selections
        _pData->ClearSelection();
    }
    else
    {
        const auto bufferSize = _pData->GetTextBuffer().GetSize();
        if (!bufferSize.IsInBounds(_start, true) || !bufferSize.IsInBounds(_end, true))
        {
            return E_FAIL;
        }
        auto inclusiveEnd = _end;
        bufferSize.DecrementInBounds(inclusiveEnd);
        _pData->SelectNewRegion(_start, inclusiveEnd);
    }

    UiaTracing::TextRange::Select(*this);
    return S_OK;
}
CATCH_RETURN();

// we don't support this
IFACEMETHODIMP UiaTextRangeBase::AddToSelection() noexcept
{
    UiaTracing::TextRange::AddToSelection(*this);
    return E_NOTIMPL;
}

// we don't support this
IFACEMETHODIMP UiaTextRangeBase::RemoveFromSelection() noexcept
{
    UiaTracing::TextRange::RemoveFromSelection(*this);
    return E_NOTIMPL;
}

IFACEMETHODIMP UiaTextRangeBase::ScrollIntoView(_In_ BOOL alignToTop) noexcept
try
{
    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });
    RETURN_HR_IF(E_FAIL, !_pData->IsUiaDataInitialized());

    const auto oldViewport = _pData->GetViewport().ToInclusive();
    const auto viewportHeight = _getViewportHeight(oldViewport);
    // range rows
    const auto startScreenInfoRow = _start.y;
    const auto endScreenInfoRow = _end.y;
    // screen buffer rows
    constexpr til::CoordType topRow = 0;
    const auto bottomRow = _pData->GetTextBuffer().TotalRowCount() - 1;

    auto newViewport = oldViewport;

    // there's a bunch of +1/-1s here for setting the viewport. These
    // are to account for the inclusivity of the viewport boundaries.
    if (alignToTop)
    {
        // determine if we can align the start row to the top
        if (startScreenInfoRow + viewportHeight <= bottomRow)
        {
            // we can align to the top
            newViewport.top = startScreenInfoRow;
            newViewport.bottom = startScreenInfoRow + viewportHeight - 1;
        }
        else
        {
            // we can't align to the top so we'll just move the viewport
            // to the bottom of the screen buffer
            newViewport.bottom = bottomRow;
            newViewport.top = bottomRow - viewportHeight + 1;
        }
    }
    else
    {
        // we need to align to the bottom
        // check if we can align to the bottom
        if (endScreenInfoRow >= viewportHeight)
        {
            // GH#7839: endScreenInfoRow may be ExclusiveEnd
            //          ExclusiveEnd is past the bottomRow
            //          so we need to clamp to the bottom row to stay in bounds

            // we can align to bottom
            newViewport.bottom = std::min(endScreenInfoRow, bottomRow);
            newViewport.top = newViewport.bottom - viewportHeight + 1;
        }
        else
        {
            // we can't align to bottom so we'll move the viewport to
            // the top of the screen buffer
            newViewport.top = topRow;
            newViewport.bottom = topRow + viewportHeight - 1;
        }
    }

    assert(newViewport.top >= topRow);
    assert(newViewport.bottom <= bottomRow);
    assert(_getViewportHeight(oldViewport) == _getViewportHeight(newViewport));

    Unlock.reset();

    const gsl::not_null<ScreenInfoUiaProviderBase*> provider = static_cast<ScreenInfoUiaProviderBase*>(_pProvider);
    provider->ChangeViewport(newViewport);

    UiaTracing::TextRange::ScrollIntoView(alignToTop, *this);
    return S_OK;
}
CATCH_RETURN();

IFACEMETHODIMP UiaTextRangeBase::GetChildren(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal) noexcept
{
    RETURN_HR_IF(E_INVALIDARG, ppRetVal == nullptr);

    // we don't have any children
    *ppRetVal = SafeArrayCreateVector(VT_UNKNOWN, 0, 0);
    if (*ppRetVal == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    UiaTracing::TextRange::GetChildren(*this);
    return S_OK;
}

#pragma endregion

til::size UiaTextRangeBase::_getScreenFontSize() const noexcept
{
    auto coordRet = _pData->GetFontInfo().GetSize();

    // For sanity's sake, make sure not to leak 0 out as a possible value. These values are used in division operations.
    coordRet.width = std::max(coordRet.width, 1);
    coordRet.height = std::max(coordRet.height, 1);

    return coordRet;
}

// Routine Description:
// - Gets the viewport height, measured in char rows.
// Arguments:
// - viewport - The viewport to measure
// Return Value:
// - The viewport height
til::CoordType UiaTextRangeBase::_getViewportHeight(const til::inclusive_rect& viewport) const noexcept
{
    assert(viewport.bottom >= viewport.top);
    // + 1 because til::inclusive_rect is inclusive on both sides so subtracting top
    // and bottom gets rid of 1 more then it should.
    return viewport.bottom - viewport.top + 1;
}

// Routine Description:
// - Gets a viewport representing where valid text may be in the TextBuffer
// - Use this instead of `textBuffer.GetSize()`. This improves performance
//    because it's a smaller space to have to search through
// Arguments:
// - <none>
// Return Value:
// - A viewport representing the portion of the TextBuffer that has valid text
Viewport UiaTextRangeBase::_getOptimizedBufferSize() const noexcept
{
    // we need to add 1 to the X/Y of textBufferEnd
    // because we want the returned viewport to include this til::point
    const auto textBufferEnd = _pData->GetTextBufferEndPosition();
    const auto width = textBufferEnd.x + 1;
    const auto height = textBufferEnd.y + 1;

    return Viewport::FromDimensions({ 0, 0 }, width, height);
}

// We consider the "document end" to be the line beneath the cursor or
// last legible character (whichever is further down). In the event where
// the last legible character is on the last line of the buffer,
// we use the "end exclusive" position (left-most point on a line one past the end of the buffer).
// NOTE: "end exclusive" is naturally computed using the heuristic above.
til::point UiaTextRangeBase::_getDocumentEnd() const
{
    const auto optimizedBufferSize{ _getOptimizedBufferSize() };
    const auto& buffer{ _pData->GetTextBuffer() };
    const auto lastCharPos{ buffer.GetLastNonSpaceCharacter(optimizedBufferSize) };
    const auto cursorPos{ buffer.GetCursor().GetPosition() };
    return { optimizedBufferSize.Left(), std::max(lastCharPos.y, cursorPos.y) + 1 };
}

// Routine Description:
// - adds the relevant coordinate points from the row to coords.
// - it is assumed that startAnchor and endAnchor are within the same row
//    and NOT DEGENERATE
// Arguments:
// - startAnchor - the start anchor of interested data within the viewport. In text buffer coordinate space. Inclusive.
// - endAnchor - the end anchor of interested data within the viewport. In text buffer coordinate space. Inclusive
// - coords - vector to add the calculated coords to
// Return Value:
// - <none>
void UiaTextRangeBase::_getBoundingRect(const til::rect& textRect, _Inout_ std::vector<double>& coords) const
{
    const auto currentFontSize = _getScreenFontSize();

    til::point topLeft;
    til::point bottomRight;

    // we want to clamp to a long (output type), not a short (input type)
    // so we need to explicitly say <long,long>
    topLeft.x = textRect.left * currentFontSize.width;
    topLeft.y = textRect.top * currentFontSize.height;

    bottomRight.x = textRect.right * currentFontSize.width;
    bottomRight.y = textRect.bottom * currentFontSize.height;

    // convert the coords to be relative to the screen instead of
    // the client window
    _TranslatePointToScreen(&topLeft);
    _TranslatePointToScreen(&bottomRight);

    const long width = bottomRight.x - topLeft.x;
    const long height = bottomRight.y - topLeft.y;

    // insert the coords
    coords.push_back(topLeft.x);
    coords.push_back(topLeft.y);
    coords.push_back(width);
    coords.push_back(height);
}

// Routine Description:
// - moves the UTR's endpoint by moveCount times by character.
// - if endpoints crossed, the degenerate range is created and both endpoints are moved
// Arguments:
// - moveCount - the number of times to move
// - endpoint - the endpoint to move
// - pAmountMoved - the number of times that the return values are "moved"
// - preventBufferEnd - when enabled, prevent endpoint from being at the end of the buffer
//                      This is used for general movement, where you are not allowed to
//                      create a degenerate range
// Return Value:
// - <none>
void UiaTextRangeBase::_moveEndpointByUnitCharacter(_In_ const int moveCount,
                                                    _In_ const TextPatternRangeEndpoint endpoint,
                                                    _Out_ const gsl::not_null<int*> pAmountMoved,
                                                    _In_ const bool preventBufferEnd)
{
    *pAmountMoved = 0;

    if (moveCount == 0)
    {
        return;
    }

    const auto allowBottomExclusive = !preventBufferEnd;
    const auto moveDirection = (moveCount > 0) ? MovementDirection::Forward : MovementDirection::Backward;
    const auto& buffer = _pData->GetTextBuffer();

    auto success = true;
    til::point target{ GetEndpoint(endpoint) };
    const auto documentEnd{ _getDocumentEnd() };
    while (std::abs(*pAmountMoved) < std::abs(moveCount) && success)
    {
        switch (moveDirection)
        {
        case MovementDirection::Forward:
            success = buffer.MoveToNextGlyph(target, allowBottomExclusive, documentEnd);
            if (success)
            {
                (*pAmountMoved)++;
            }
            break;
        case MovementDirection::Backward:
            success = buffer.MoveToPreviousGlyph(target, documentEnd);
            if (success)
            {
                (*pAmountMoved)--;
            }
            break;
        default:
            break;
        }
    }

    SetEndpoint(endpoint, target);
}

// Routine Description:
// - moves the UTR's endpoint by moveCount times by word.
// - if endpoints crossed, the degenerate range is created and both endpoints are moved
// Arguments:
// - moveCount - the number of times to move
// - endpoint - the endpoint to move
// - pAmountMoved - the number of times that the return values are "moved"
// - preventBufferEnd - when enabled, prevent endpoint from being at the end of the buffer
//                      This is used for general movement, where you are not allowed to
//                      create a degenerate range
// Return Value:
// - <none>
void UiaTextRangeBase::_moveEndpointByUnitWord(_In_ const int moveCount,
                                               _In_ const TextPatternRangeEndpoint endpoint,
                                               _Out_ const gsl::not_null<int*> pAmountMoved,
                                               _In_ const bool preventBufferEnd)
{
    *pAmountMoved = 0;

    if (moveCount == 0)
    {
        return;
    }

    const auto allowBottomExclusive = !preventBufferEnd;
    const auto moveDirection = (moveCount > 0) ? MovementDirection::Forward : MovementDirection::Backward;
    const auto& buffer = _pData->GetTextBuffer();
    const auto bufferSize = buffer.GetSize();
    const auto bufferOrigin = bufferSize.Origin();
    const auto documentEnd = _getDocumentEnd();

    auto resultPos = GetEndpoint(endpoint);
    auto nextPos = resultPos;

    auto success = true;
    while (std::abs(*pAmountMoved) < std::abs(moveCount) && success)
    {
        nextPos = resultPos;
        switch (moveDirection)
        {
        case MovementDirection::Forward:
        {
            if (nextPos >= documentEnd)
            {
                success = false;
            }
            else if (buffer.MoveToNextWord(nextPos, _wordDelimiters, documentEnd))
            {
                resultPos = nextPos;
                (*pAmountMoved)++;
            }
            else if (allowBottomExclusive)
            {
                resultPos = documentEnd;
                (*pAmountMoved)++;
            }
            else
            {
                success = false;
            }
            break;
        }
        case MovementDirection::Backward:
        {
            if (nextPos == bufferOrigin)
            {
                success = false;
            }
            else if (allowBottomExclusive && _tryMoveToWordStart(buffer, documentEnd, resultPos))
            {
                // IMPORTANT: _tryMoveToWordStart modifies resultPos if successful
                // Degenerate ranges first move to the beginning of the word,
                // but if we're already at the beginning of the word, we continue
                // to the next branch and move to the previous word!
                (*pAmountMoved)--;
            }
            else if (buffer.MoveToPreviousWord(nextPos, _wordDelimiters))
            {
                resultPos = nextPos;
                (*pAmountMoved)--;
            }
            else
            {
                resultPos = bufferOrigin;
            }
            break;
        }
        default:
            return;
        }
    }

    SetEndpoint(endpoint, resultPos);
}

// Routine Description:
// - tries to move resultingPos to the beginning of the word
// Arguments:
// - buffer - the text buffer we're operating on
// - documentEnd - the document end of the buffer (see _getDocumentEnd())
// - resultingPos - the position we're starting from and modifying
// Return Value:
// - true --> we were not at the beginning of the word, and we updated resultingPos to be so
// - false --> otherwise (we're already at the beginning of the word)
bool UiaTextRangeBase::_tryMoveToWordStart(const TextBuffer& buffer, const til::point documentEnd, til::point& resultingPos) const
{
    const auto wordStart{ buffer.GetWordStart(resultingPos, _wordDelimiters, true, documentEnd) };
    if (resultingPos != wordStart)
    {
        resultingPos = wordStart;
        return true;
    }
    return false;
}

// Routine Description:
// - moves the UTR's endpoint by moveCount times by line.
// - if endpoints crossed, the degenerate range is created and both endpoints are moved
// - a successful movement on start entails start being at Left()
// - a successful movement on end entails end being at Left() of the NEXT line
// Arguments:
// - moveCount - the number of times to move
// - endpoint - the endpoint to move
// - pAmountMoved - the number of times that the return values are "moved"
// - preventBoundary - true --> the range encompasses the unit we're on; prevent movement onto boundaries
//                     false --> act like we're just moving an endpoint; allow movement onto boundaries
// Return Value:
// - <none>
void UiaTextRangeBase::_moveEndpointByUnitLine(_In_ const int moveCount,
                                               _In_ const TextPatternRangeEndpoint endpoint,
                                               _Out_ const gsl::not_null<int*> pAmountMoved,
                                               _In_ const bool preventBoundary) noexcept
{
    *pAmountMoved = 0;

    if (moveCount == 0)
    {
        return;
    }

    const auto allowBottomExclusive = !preventBoundary;
    const auto moveDirection = (moveCount > 0) ? MovementDirection::Forward : MovementDirection::Backward;
    const auto bufferSize = _getOptimizedBufferSize();

    auto documentEnd{ bufferSize.EndExclusive() };
    try
    {
        documentEnd = _getDocumentEnd();
    }
    CATCH_LOG();

    auto success = true;
    auto resultPos = GetEndpoint(endpoint);

    while (std::abs(*pAmountMoved) < std::abs(moveCount) && success)
    {
        auto nextPos = resultPos;
        switch (moveDirection)
        {
        case MovementDirection::Forward:
        {
            if (nextPos.y >= documentEnd.y)
            {
                // Corner Case: we're past the limit
                // Clamp us to the limit
                resultPos = documentEnd;
                success = false;
            }
            else if (preventBoundary && nextPos.y == documentEnd.y - 1)
            {
                // Corner Case: we're just before the limit
                // and we're not allowed onto the exclusive end.
                // Fail to move.
                success = false;
            }
            else
            {
                nextPos.x = bufferSize.RightInclusive();
                success = bufferSize.IncrementInBounds(nextPos, allowBottomExclusive);
                if (success)
                {
                    resultPos = nextPos;
                    (*pAmountMoved)++;
                }
            }
            break;
        }
        case MovementDirection::Backward:
        {
            if (preventBoundary)
            {
                if (nextPos.y == bufferSize.Top())
                {
                    // can't move past top
                    success = false;
                    break;
                }
                else
                {
                    // GH#10924: as a non-degenerate range, we are supposed to act
                    // like we already encompass the line.
                    // Move to the left boundary so we try to wrap around
                    nextPos.x = bufferSize.Left();
                }
            }

            // NOTE: Automatically detects if we are trying to move past origin
            success = bufferSize.DecrementInBounds(nextPos, true);

            if (success)
            {
                nextPos.x = bufferSize.Left();
                resultPos = nextPos;
                (*pAmountMoved)--;
            }
            break;
        }
        default:
            break;
        }
    }

    SetEndpoint(endpoint, resultPos);
}

// Routine Description:
// - moves the UTR's endpoint by moveCount times by document.
// - if endpoints crossed, the degenerate range is created and both endpoints are moved
// Arguments:
// - moveCount - the number of times to move
// - endpoint - the endpoint to move
// - pAmountMoved - the number of times that the return values are "moved"
// - preventBoundary - true --> the range encompasses the unit we're on; prevent movement onto boundaries
//                     false --> act like we're just moving an endpoint; allow movement onto boundaries
// Return Value:
// - <none>
void UiaTextRangeBase::_moveEndpointByUnitDocument(_In_ const int moveCount,
                                                   _In_ const TextPatternRangeEndpoint endpoint,
                                                   _Out_ const gsl::not_null<int*> pAmountMoved,
                                                   _In_ const bool preventBoundary) noexcept
{
    *pAmountMoved = 0;

    if (moveCount == 0)
    {
        return;
    }

    const auto moveDirection = (moveCount > 0) ? MovementDirection::Forward : MovementDirection::Backward;
    const auto bufferSize = _getOptimizedBufferSize();

    const auto target = GetEndpoint(endpoint);
    switch (moveDirection)
    {
    case MovementDirection::Forward:
    {
        auto documentEnd{ bufferSize.EndExclusive() };
        try
        {
            documentEnd = _getDocumentEnd();
        }
        CATCH_LOG();

        if (preventBoundary || target >= documentEnd)
        {
            return;
        }
        else
        {
            SetEndpoint(endpoint, documentEnd);
            (*pAmountMoved)++;
        }
        break;
    }
    case MovementDirection::Backward:
    {
        const auto documentBegin = bufferSize.Origin();
        if (preventBoundary || target == documentBegin)
        {
            return;
        }
        else
        {
            SetEndpoint(endpoint, documentBegin);
            (*pAmountMoved)--;
        }
        break;
    }
    default:
        break;
    }
}

til::rect UiaTextRangeBase::_getTerminalRect() const
{
    UiaRect result{};

    IRawElementProviderFragment* pRawElementProviderFragment;
    THROW_IF_FAILED(_pProvider->QueryInterface<IRawElementProviderFragment>(&pRawElementProviderFragment));
    if (pRawElementProviderFragment)
    {
        pRawElementProviderFragment->get_BoundingRectangle(&result);
    }

    return {
        gsl::narrow_cast<til::CoordType>(result.left),
        gsl::narrow_cast<til::CoordType>(result.top),
        gsl::narrow_cast<til::CoordType>(result.left + result.width),
        gsl::narrow_cast<til::CoordType>(result.top + result.height),
    };
}

til::point UiaTextRangeBase::_getInclusiveEnd() const noexcept
{
    auto result{ _end };
    _pData->GetTextBuffer().GetSize().DecrementInBounds(result, true);
    return result;
}

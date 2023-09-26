// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "DWriteTextAnalysis.h"

#pragma warning(disable : 4100) // '...': unreferenced formal parameter
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).

using namespace Microsoft::Console::Render::Atlas;

TextAnalysisSource::TextAnalysisSource(const wchar_t* locale, const wchar_t* text, const UINT32 textLength) noexcept :
    _locale{ locale },
    _text{ text },
    _textLength{ textLength }
{
}

// TextAnalysisSource will be allocated on the stack and reference counting is pointless because of that.
// The debug version will assert that we don't leak any references though.
#ifdef NDEBUG
ULONG __stdcall TextAnalysisSource::AddRef() noexcept
{
    return 1;
}

ULONG __stdcall TextAnalysisSource::Release() noexcept
{
    return 1;
}
#else
TextAnalysisSource::~TextAnalysisSource()
{
    assert(_refCount == 1);
}

ULONG __stdcall TextAnalysisSource::AddRef() noexcept
{
    return ++_refCount;
}

ULONG __stdcall TextAnalysisSource::Release() noexcept
{
    return --_refCount;
}
#endif

HRESULT TextAnalysisSource::QueryInterface(const IID& riid, void** ppvObject) noexcept
{
    __assume(ppvObject != nullptr);

    if (IsEqualGUID(riid, __uuidof(IDWriteTextAnalysisSource)))
    {
        *ppvObject = this;
        return S_OK;
    }

    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

HRESULT TextAnalysisSource::GetTextAtPosition(UINT32 textPosition, const WCHAR** textString, UINT32* textLength) noexcept
{
    // Writing to address 0 is a crash in practice. Just what we want.
    __assume(textString != nullptr);
    __assume(textLength != nullptr);

    textPosition = std::min(textPosition, _textLength);
    *textString = _text + textPosition;
    *textLength = _textLength - textPosition;
    return S_OK;
}

HRESULT TextAnalysisSource::GetTextBeforePosition(UINT32 textPosition, const WCHAR** textString, UINT32* textLength) noexcept
{
    // Writing to address 0 is a crash in practice. Just what we want.
    __assume(textString != nullptr);
    __assume(textLength != nullptr);

    textPosition = std::min(textPosition, _textLength);
    *textString = _text;
    *textLength = textPosition;
    return S_OK;
}

DWRITE_READING_DIRECTION TextAnalysisSource::GetParagraphReadingDirection() noexcept
{
    return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
}

HRESULT TextAnalysisSource::GetLocaleName(UINT32 textPosition, UINT32* textLength, const WCHAR** localeName) noexcept
{
    // Writing to address 0 is a crash in practice. Just what we want.
    __assume(textLength != nullptr);
    __assume(localeName != nullptr);

    *textLength = _textLength - textPosition;
    *localeName = _locale;
    return S_OK;
}

HRESULT TextAnalysisSource::GetNumberSubstitution(UINT32 textPosition, UINT32* textLength, IDWriteNumberSubstitution** numberSubstitution) noexcept
{
    return E_NOTIMPL;
}

TextAnalysisSink::TextAnalysisSink(std::vector<TextAnalysisSinkResult>& results) noexcept :
    _results{ results }
{
}

// TextAnalysisSource will be allocated on the stack and reference counting is pointless because of that.
// The debug version will assert that we don't leak any references though.
#ifdef NDEBUG
ULONG __stdcall TextAnalysisSink::AddRef() noexcept
{
    return 1;
}

ULONG __stdcall TextAnalysisSink::Release() noexcept
{
    return 1;
}
#else
TextAnalysisSink::~TextAnalysisSink()
{
    assert(_refCount == 1);
}

ULONG __stdcall TextAnalysisSink::AddRef() noexcept
{
    return ++_refCount;
}

ULONG __stdcall TextAnalysisSink::Release() noexcept
{
    return --_refCount;
}
#endif

HRESULT TextAnalysisSink::QueryInterface(const IID& riid, void** ppvObject) noexcept
{
    __assume(ppvObject != nullptr);

    if (IsEqualGUID(riid, __uuidof(IDWriteTextAnalysisSink)))
    {
        *ppvObject = this;
        return S_OK;
    }

    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

HRESULT __stdcall TextAnalysisSink::SetScriptAnalysis(UINT32 textPosition, UINT32 textLength, const DWRITE_SCRIPT_ANALYSIS* scriptAnalysis) noexcept
try
{
    __assume(scriptAnalysis != nullptr);
    _results.emplace_back(textPosition, textLength, *scriptAnalysis);
    return S_OK;
}
CATCH_RETURN()

HRESULT TextAnalysisSink::SetLineBreakpoints(UINT32 textPosition, UINT32 textLength, const DWRITE_LINE_BREAKPOINT* lineBreakpoints) noexcept
{
    return E_NOTIMPL;
}

HRESULT TextAnalysisSink::SetBidiLevel(UINT32 textPosition, UINT32 textLength, UINT8 explicitLevel, UINT8 resolvedLevel) noexcept
{
    return E_NOTIMPL;
}

HRESULT TextAnalysisSink::SetNumberSubstitution(UINT32 textPosition, UINT32 textLength, IDWriteNumberSubstitution* numberSubstitution) noexcept
{
    return E_NOTIMPL;
}

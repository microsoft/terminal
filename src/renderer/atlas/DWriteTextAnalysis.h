// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "common.h"

namespace Microsoft::Console::Render::Atlas
{
    struct TextAnalysisSource final : IDWriteTextAnalysisSource
    {
        TextAnalysisSource(const wchar_t* locale, const wchar_t* text, const UINT32 textLength) noexcept;
#ifndef NDEBUG
        ~TextAnalysisSource();
#endif

        ULONG __stdcall AddRef() noexcept override;
        ULONG __stdcall Release() noexcept override;
        HRESULT __stdcall QueryInterface(const IID& riid, void** ppvObject) noexcept override;
        HRESULT __stdcall GetTextAtPosition(UINT32 textPosition, const WCHAR** textString, UINT32* textLength) noexcept override;
        HRESULT __stdcall GetTextBeforePosition(UINT32 textPosition, const WCHAR** textString, UINT32* textLength) noexcept override;
        DWRITE_READING_DIRECTION __stdcall GetParagraphReadingDirection() noexcept override;
        HRESULT __stdcall GetLocaleName(UINT32 textPosition, UINT32* textLength, const WCHAR** localeName) noexcept override;
        HRESULT __stdcall GetNumberSubstitution(UINT32 textPosition, UINT32* textLength, IDWriteNumberSubstitution** numberSubstitution) noexcept override;

    private:
        const wchar_t* _locale;
        const wchar_t* _text;
        const UINT32 _textLength;
#ifndef NDEBUG
        ULONG _refCount = 1;
#endif
    };

    struct TextAnalysisSink final : IDWriteTextAnalysisSink
    {
        TextAnalysisSink(std::vector<TextAnalysisSinkResult>& results) noexcept;
#ifndef NDEBUG
        ~TextAnalysisSink();
#endif

        ULONG __stdcall AddRef() noexcept override;
        ULONG __stdcall Release() noexcept override;
        HRESULT __stdcall QueryInterface(const IID& riid, void** ppvObject) noexcept override;
        HRESULT __stdcall SetScriptAnalysis(UINT32 textPosition, UINT32 textLength, const DWRITE_SCRIPT_ANALYSIS* scriptAnalysis) noexcept override;
        HRESULT __stdcall SetLineBreakpoints(UINT32 textPosition, UINT32 textLength, const DWRITE_LINE_BREAKPOINT* lineBreakpoints) noexcept override;
        HRESULT __stdcall SetBidiLevel(UINT32 textPosition, UINT32 textLength, UINT8 explicitLevel, UINT8 resolvedLevel) noexcept override;
        HRESULT __stdcall SetNumberSubstitution(UINT32 textPosition, UINT32 textLength, IDWriteNumberSubstitution* numberSubstitution) noexcept override;

    private:
        std::vector<TextAnalysisSinkResult>& _results;
#ifndef NDEBUG
        ULONG _refCount = 1;
#endif
    };
}

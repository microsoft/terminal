// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <dwrite.h>
#include <d2d1.h>

#include <wrl.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "../inc/Cluster.hpp"

namespace Microsoft::Console::Render
{
    class CustomTextLayout : public ::Microsoft::WRL::RuntimeClass<::Microsoft::WRL::RuntimeClassFlags<::Microsoft::WRL::ClassicCom | ::Microsoft::WRL::InhibitFtmBase>, IDWriteTextAnalysisSource, IDWriteTextAnalysisSink>
    {
    public:
        // Based on the Windows 7 SDK sample at https://github.com/pauldotknopf/WindowsSDK7-Samples/tree/master/multimedia/DirectWrite/CustomLayout

        CustomTextLayout(IDWriteFactory2* const factory,
                         IDWriteTextAnalyzer1* const analyzer,
                         IDWriteTextFormat2* const format,
                         IDWriteFontFace5* const font,
                         const std::basic_string_view<::Microsoft::Console::Render::Cluster> clusters,
                         size_t const width);

        [[nodiscard]] HRESULT STDMETHODCALLTYPE GetColumns(_Out_ UINT32* columns);

        // IDWriteTextLayout methods (but we don't actually want to implement them all, so just this one matching the existing interface)
        [[nodiscard]] HRESULT STDMETHODCALLTYPE Draw(_In_opt_ void* clientDrawingContext,
                                                     _In_ IDWriteTextRenderer* renderer,
                                                     FLOAT originX,
                                                     FLOAT originY);

        // IDWriteTextAnalysisSource methods
        [[nodiscard]] virtual HRESULT STDMETHODCALLTYPE GetTextAtPosition(UINT32 textPosition,
                                                                          _Outptr_result_buffer_(*textLength) WCHAR const** textString,
                                                                          _Out_ UINT32* textLength) override;
        [[nodiscard]] virtual HRESULT STDMETHODCALLTYPE GetTextBeforePosition(UINT32 textPosition,
                                                                              _Outptr_result_buffer_(*textLength) WCHAR const** textString,
                                                                              _Out_ UINT32* textLength) override;
        [[nodiscard]] virtual DWRITE_READING_DIRECTION STDMETHODCALLTYPE GetParagraphReadingDirection() override;
        [[nodiscard]] virtual HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 textPosition,
                                                                      _Out_ UINT32* textLength,
                                                                      _Outptr_result_z_ WCHAR const** localeName) override;
        [[nodiscard]] virtual HRESULT STDMETHODCALLTYPE GetNumberSubstitution(UINT32 textPosition,
                                                                              _Out_ UINT32* textLength,
                                                                              _COM_Outptr_ IDWriteNumberSubstitution** numberSubstitution) override;

        // IDWriteTextAnalysisSink methods
        [[nodiscard]] virtual HRESULT STDMETHODCALLTYPE SetScriptAnalysis(UINT32 textPosition,
                                                                          UINT32 textLength,
                                                                          _In_ DWRITE_SCRIPT_ANALYSIS const* scriptAnalysis) override;
        [[nodiscard]] virtual HRESULT STDMETHODCALLTYPE SetLineBreakpoints(UINT32 textPosition,
                                                                           UINT32 textLength,
                                                                           _In_reads_(textLength) DWRITE_LINE_BREAKPOINT const* lineBreakpoints) override;
        [[nodiscard]] virtual HRESULT STDMETHODCALLTYPE SetBidiLevel(UINT32 textPosition,
                                                                     UINT32 textLength,
                                                                     UINT8 explicitLevel,
                                                                     UINT8 resolvedLevel) override;
        [[nodiscard]] virtual HRESULT STDMETHODCALLTYPE SetNumberSubstitution(UINT32 textPosition,
                                                                              UINT32 textLength,
                                                                              _In_ IDWriteNumberSubstitution* numberSubstitution) override;

    protected:
        // A single contiguous run of characters containing the same analysis results.
        struct Run
        {
            Run() :
                textStart(),
                textLength(),
                glyphStart(),
                glyphCount(),
                bidiLevel(),
                script(),
                isNumberSubstituted(),
                isSideways(),
                fontFace{ nullptr },
                fontScale{ 1.0 }
            {
            }

            UINT32 textStart; // starting text position of this run
            UINT32 textLength; // number of contiguous code units covered
            UINT32 glyphStart; // starting glyph in the glyphs array
            UINT32 glyphCount; // number of glyphs associated with this run of text
            DWRITE_SCRIPT_ANALYSIS script;
            UINT8 bidiLevel;
            bool isNumberSubstituted;
            bool isSideways;
            ::Microsoft::WRL::ComPtr<IDWriteFontFace5> fontFace;
            FLOAT fontScale;

            inline bool ContainsTextPosition(UINT32 desiredTextPosition) const
            {
                return desiredTextPosition >= textStart && desiredTextPosition < textStart + textLength;
            }

            inline bool operator==(UINT32 desiredTextPosition) const
            {
                // Search by text position using std::find
                return ContainsTextPosition(desiredTextPosition);
            }
        };

        // Single text analysis run, which points to the next run.
        struct LinkedRun : Run
        {
            LinkedRun() :
                nextRunIndex(0)
            {
            }

            UINT32 nextRunIndex; // index of next run
        };

        [[nodiscard]] LinkedRun& _FetchNextRun(UINT32& textLength);
        void _SetCurrentRun(const UINT32 textPosition);
        void _SplitCurrentRun(const UINT32 splitPosition);

        [[nodiscard]] HRESULT STDMETHODCALLTYPE _AnalyzeFontFallback(IDWriteTextAnalysisSource* const source, UINT32 textPosition, UINT32 textLength);
        [[nodiscard]] HRESULT STDMETHODCALLTYPE _SetMappedFont(UINT32 textPosition, UINT32 textLength, IDWriteFont* const font, FLOAT const scale);

        [[nodiscard]] HRESULT _AnalyzeRuns() noexcept;
        [[nodiscard]] HRESULT _ShapeGlyphRuns() noexcept;
        [[nodiscard]] HRESULT _ShapeGlyphRun(const UINT32 runIndex, UINT32& glyphStart) noexcept;
        [[nodiscard]] HRESULT _CorrectGlyphRuns() noexcept;
        [[nodiscard]] HRESULT _CorrectGlyphRun(const UINT32 runIndex) noexcept;
        [[nodiscard]] HRESULT _DrawGlyphRuns(_In_opt_ void* clientDrawingContext,
                                             IDWriteTextRenderer* renderer,
                                             const D2D_POINT_2F origin) noexcept;

        [[nodiscard]] static UINT32 _EstimateGlyphCount(const UINT32 textLength) noexcept;

    private:
        const ::Microsoft::WRL::ComPtr<IDWriteFactory2> _factory;

        // DirectWrite analyzer
        const ::Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1> _analyzer;

        // DirectWrite text format
        const ::Microsoft::WRL::ComPtr<IDWriteTextFormat2> _format;

        // DirectWrite font face
        const ::Microsoft::WRL::ComPtr<IDWriteFontFace5> _font;

        // The text we're analyzing and processing into a layout
        std::wstring _text;
        std::vector<UINT16> _textClusterColumns;
        size_t _width;

        // Properties of the text that might be relevant.
        std::wstring _localeName;
        ::Microsoft::WRL::ComPtr<IDWriteNumberSubstitution> _numberSubstitution;
        DWRITE_READING_DIRECTION _readingDirection;

        // Text analysis results
        std::vector<LinkedRun> _runs;
        std::vector<DWRITE_LINE_BREAKPOINT> _breakpoints;

        // Text analysis interim status variable (to assist the Analyzer Sink in operations involving _runs)
        UINT32 _runIndex;

        // Glyph shaping results
        std::vector<DWRITE_GLYPH_OFFSET> _glyphOffsets;
        std::vector<UINT16> _glyphClusters;
        std::vector<UINT16> _glyphIndices;
        std::vector<float> _glyphAdvances;
    };
}

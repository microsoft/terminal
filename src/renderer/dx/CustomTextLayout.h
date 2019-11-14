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

        CustomTextLayout(gsl::not_null<IDWriteFactory1*> const factory,
                         gsl::not_null<IDWriteTextAnalyzer1*> const analyzer,
                         gsl::not_null<IDWriteTextFormat*> const format,
                         gsl::not_null<IDWriteFontFace1*> const font,
                         const std::basic_string_view<::Microsoft::Console::Render::Cluster> clusters,
                         size_t const width);

        [[nodiscard]] HRESULT STDMETHODCALLTYPE GetColumns(_Out_ UINT32* columns);

        // IDWriteTextLayout methods (but we don't actually want to implement them all, so just this one matching the existing interface)
        [[nodiscard]] HRESULT STDMETHODCALLTYPE Draw(_In_opt_ void* clientDrawingContext,
                                                     _In_ IDWriteTextRenderer* renderer,
                                                     FLOAT originX,
                                                     FLOAT originY) noexcept;

        // IDWriteTextAnalysisSource methods
        [[nodiscard]] HRESULT STDMETHODCALLTYPE GetTextAtPosition(UINT32 textPosition,
                                                                  _Outptr_result_buffer_(*textLength) WCHAR const** textString,
                                                                  _Out_ UINT32* textLength) override;
        [[nodiscard]] HRESULT STDMETHODCALLTYPE GetTextBeforePosition(UINT32 textPosition,
                                                                      _Outptr_result_buffer_(*textLength) WCHAR const** textString,
                                                                      _Out_ UINT32* textLength) noexcept override;
        [[nodiscard]] DWRITE_READING_DIRECTION STDMETHODCALLTYPE GetParagraphReadingDirection() noexcept override;
        [[nodiscard]] HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 textPosition,
                                                              _Out_ UINT32* textLength,
                                                              _Outptr_result_z_ WCHAR const** localeName) noexcept override;
        [[nodiscard]] HRESULT STDMETHODCALLTYPE GetNumberSubstitution(UINT32 textPosition,
                                                                      _Out_ UINT32* textLength,
                                                                      _COM_Outptr_ IDWriteNumberSubstitution** numberSubstitution) noexcept override;

        // IDWriteTextAnalysisSink methods
        [[nodiscard]] HRESULT STDMETHODCALLTYPE SetScriptAnalysis(UINT32 textPosition,
                                                                  UINT32 textLength,
                                                                  _In_ DWRITE_SCRIPT_ANALYSIS const* scriptAnalysis) override;
        [[nodiscard]] HRESULT STDMETHODCALLTYPE SetLineBreakpoints(UINT32 textPosition,
                                                                   UINT32 textLength,
                                                                   _In_reads_(textLength) DWRITE_LINE_BREAKPOINT const* lineBreakpoints) override;
        [[nodiscard]] HRESULT STDMETHODCALLTYPE SetBidiLevel(UINT32 textPosition,
                                                             UINT32 textLength,
                                                             UINT8 explicitLevel,
                                                             UINT8 resolvedLevel) override;
        [[nodiscard]] HRESULT STDMETHODCALLTYPE SetNumberSubstitution(UINT32 textPosition,
                                                                      UINT32 textLength,
                                                                      _In_ IDWriteNumberSubstitution* numberSubstitution) override;

    protected:
        // A single contiguous run of characters containing the same analysis results.
        struct Run
        {
            Run() noexcept :
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
            ::Microsoft::WRL::ComPtr<IDWriteFontFace1> fontFace;
            FLOAT fontScale;

            inline bool ContainsTextPosition(UINT32 desiredTextPosition) const noexcept
            {
                return desiredTextPosition >= textStart && desiredTextPosition < textStart + textLength;
            }

            inline bool operator==(UINT32 desiredTextPosition) const noexcept
            {
                // Search by text position using std::find
                return ContainsTextPosition(desiredTextPosition);
            }
        };

        // Single text analysis run, which points to the next run.
        struct LinkedRun : Run
        {
            LinkedRun() noexcept :
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

        [[nodiscard]] static constexpr UINT32 _EstimateGlyphCount(const UINT32 textLength) noexcept;

    private:
        const ::Microsoft::WRL::ComPtr<IDWriteFactory1> _factory;

        // DirectWrite analyzer
        const ::Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1> _analyzer;

        // DirectWrite text format
        const ::Microsoft::WRL::ComPtr<IDWriteTextFormat> _format;

        // DirectWrite font face
        const ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _font;

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

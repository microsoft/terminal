// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <dwrite.h>
#include <d2d1.h>

#include <wrl.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "BoxDrawingEffect.h"
#include "DxFontRenderData.h"
#include "../inc/Cluster.hpp"

namespace Microsoft::Console::Render
{
    class CustomTextLayout : public ::Microsoft::WRL::RuntimeClass<::Microsoft::WRL::RuntimeClassFlags<::Microsoft::WRL::ClassicCom | ::Microsoft::WRL::InhibitFtmBase>, IDWriteTextAnalysisSource, IDWriteTextAnalysisSink>
    {
    public:
        // Based on the Windows 7 SDK sample at https://github.com/pauldotknopf/WindowsSDK7-Samples/tree/master/multimedia/DirectWrite/CustomLayout

        CustomTextLayout(const gsl::not_null<DxFontRenderData*> fontRenderData);

        [[nodiscard]] HRESULT STDMETHODCALLTYPE AppendClusters(const std::span<const ::Microsoft::Console::Render::Cluster> clusters);

        [[nodiscard]] HRESULT STDMETHODCALLTYPE Reset() noexcept;

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
                                                              _Outptr_result_z_ const WCHAR** localeName) noexcept override;
        [[nodiscard]] HRESULT STDMETHODCALLTYPE GetNumberSubstitution(UINT32 textPosition,
                                                                      _Out_ UINT32* textLength,
                                                                      _COM_Outptr_ IDWriteNumberSubstitution** numberSubstitution) noexcept override;

        // IDWriteTextAnalysisSink methods
        [[nodiscard]] HRESULT STDMETHODCALLTYPE SetScriptAnalysis(UINT32 textPosition,
                                                                  UINT32 textLength,
                                                                  _In_ const DWRITE_SCRIPT_ANALYSIS* scriptAnalysis) override;
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
                fontScale{ 1.0 },
                drawingEffect{ nullptr }
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
            ::Microsoft::WRL::ComPtr<IUnknown> drawingEffect;

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
        [[nodiscard]] LinkedRun& _GetCurrentRun();
        void _SetCurrentRun(const UINT32 textPosition);
        void _SplitCurrentRun(const UINT32 splitPosition);
        void _OrderRuns();

        [[nodiscard]] HRESULT STDMETHODCALLTYPE _AnalyzeFontFallback(IDWriteTextAnalysisSource* const source, UINT32 textPosition, UINT32 textLength);
        [[nodiscard]] HRESULT STDMETHODCALLTYPE _SetMappedFontFace(UINT32 textPosition, UINT32 textLength, const ::Microsoft::WRL::ComPtr<IDWriteFontFace>& fontFace, FLOAT const scale);

        [[nodiscard]] HRESULT STDMETHODCALLTYPE _AnalyzeBoxDrawing(const gsl::not_null<IDWriteTextAnalysisSource*> source, UINT32 textPosition, UINT32 textLength);
        [[nodiscard]] HRESULT STDMETHODCALLTYPE _SetBoxEffect(UINT32 textPosition, UINT32 textLength);

        [[nodiscard]] HRESULT _AnalyzeTextComplexity() noexcept;
        [[nodiscard]] HRESULT _AnalyzeRuns() noexcept;
        [[nodiscard]] HRESULT _ShapeGlyphRuns() noexcept;
        [[nodiscard]] HRESULT _ShapeGlyphRun(const UINT32 runIndex, UINT32& glyphStart) noexcept;
        [[nodiscard]] HRESULT _CorrectGlyphRuns() noexcept;
        [[nodiscard]] HRESULT _CorrectGlyphRun(const UINT32 runIndex) noexcept;
        [[nodiscard]] HRESULT STDMETHODCALLTYPE _CorrectBoxDrawing() noexcept;
        [[nodiscard]] HRESULT _DrawGlyphRuns(_In_opt_ void* clientDrawingContext,
                                             IDWriteTextRenderer* renderer,
                                             const D2D_POINT_2F origin) noexcept;
        [[nodiscard]] HRESULT _DrawGlyphRun(_In_opt_ void* clientDrawingContext,
                                            gsl::not_null<IDWriteTextRenderer*> renderer,
                                            D2D_POINT_2F& mutableOrigin,
                                            const Run& run) noexcept;

        [[nodiscard]] static constexpr UINT32 _EstimateGlyphCount(const UINT32 textLength) noexcept;

    private:
        // DirectWrite font render data
        DxFontRenderData* _fontRenderData;

        // DirectWrite text formats
        IDWriteTextFormat* _formatInUse;

        // DirectWrite font faces
        IDWriteFontFace1* _fontInUse;

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

        // Whether the entire text is determined to be simple and does not require full script shaping.
        bool _isEntireTextSimple;

        std::vector<DWRITE_GLYPH_OFFSET> _glyphOffsets;

        // Clusters are complicated. They're in respect to each individual run.
        // The offsets listed here are in respect to the _text string, but from the beginning index of
        // each run.
        // That means if we have two runs, we will see 0 1 2 3 4 0 1 2 3 4 5 6 7... in this clusters count.
        std::vector<UINT16> _glyphClusters;

        // This appears to be the index of the glyph inside each font.
        std::vector<UINT16> _glyphIndices;

        // This is for calculating glyph advances when the entire text is simple.
        std::vector<INT32> _glyphDesignUnitAdvances;

        std::vector<float> _glyphAdvances;

        struct ScaleCorrection
        {
            UINT32 textIndex;
            UINT32 textLength;
            float scale;
        };

        // These are used to further break the runs apart and adjust the font size so glyphs fit inside the cells.
        std::vector<ScaleCorrection> _glyphScaleCorrections;

#ifdef UNIT_TESTING
    public:
        CustomTextLayout() = default;

        friend class CustomTextLayoutTests;
#endif
    };
}

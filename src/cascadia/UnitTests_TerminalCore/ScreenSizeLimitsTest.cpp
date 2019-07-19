// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <WexTestClass.h>

#include "DefaultSettings.h"
#include "../cascadia/TerminalCore/Terminal.hpp"
#include "../renderer/inc/DummyRenderTarget.hpp"
#include "consoletaeftemplates.hpp"

#include "winrt/Microsoft.Terminal.Settings.h"

using namespace winrt::Microsoft::Terminal::Settings;
using namespace Microsoft::Terminal::Core;

namespace TerminalCoreUnitTests
{
    class MockTermSettings : public winrt::implements<MockTermSettings, ICoreSettings>
    {
    public:
        MockTermSettings(int32_t historySize, int32_t initialRows, int32_t initialCols) :
            _historySize(historySize),
            _initialRows(initialRows),
            _initialCols(initialCols)
        {
        }

        // property getters - all implemented
        int32_t HistorySize() { return _historySize; }
        int32_t InitialRows() { return _initialRows; }
        int32_t InitialCols() { return _initialCols; }
        uint32_t DefaultForeground() { return COLOR_WHITE; }
        uint32_t DefaultBackground() { return COLOR_BLACK; }
        bool SnapOnInput() { return false; }
        uint32_t CursorColor() { return COLOR_WHITE; }
        CursorStyle CursorShape() const noexcept { return CursorStyle::Vintage; }
        uint32_t CursorHeight() { return 42UL; }
        winrt::hstring WordDelimiters() { return winrt::to_hstring(DEFAULT_WORD_DELIMITERS.c_str()); }

        // other implemented methods
        uint32_t GetColorTableEntry(int32_t) const { return 123; }

        // property setters - all unimplemented
        void HistorySize(int32_t) {}
        void InitialRows(int32_t) {}
        void InitialCols(int32_t) {}
        void DefaultForeground(uint32_t) {}
        void DefaultBackground(uint32_t) {}
        void SnapOnInput(bool) {}
        void CursorColor(uint32_t) {}
        void CursorShape(CursorStyle const&) noexcept {}
        void CursorHeight(uint32_t) {}
        void WordDelimiters(winrt::hstring) {}

        // other unimplemented methods
        void SetColorTableEntry(int32_t /* index */, uint32_t /* value */) {}

    private:
        int32_t _historySize;
        int32_t _initialRows;
        int32_t _initialCols;
    };

#define WCS(x) WCSHELPER(x)
#define WCSHELPER(x) L#x

    class ScreenSizeLimitsTest
    {
        TEST_CLASS(ScreenSizeLimitsTest);

        TEST_METHOD(ScreenWidthAndHeightAreClampedToBounds)
        {
            DummyRenderTarget emptyRenderTarget;

            // Negative values for initial visible row count or column count
            // are clamped to 1. Too-large positive values are clamped to SHRT_MAX.
            auto negativeColumnsSettings = winrt::make<MockTermSettings>(10000, 9999999, -1234);
            Terminal negativeColumnsTerminal;
            negativeColumnsTerminal.CreateFromSettings(negativeColumnsSettings, emptyRenderTarget);
            COORD actualDimensions = negativeColumnsTerminal.GetViewport().Dimensions();
            VERIFY_ARE_EQUAL(actualDimensions.Y, SHRT_MAX, L"Row count clamped to SHRT_MAX == " WCS(SHRT_MAX));
            VERIFY_ARE_EQUAL(actualDimensions.X, 1, L"Column count clamped to 1");

            // Zero values are clamped to 1 as well.
            auto zeroRowsSettings = winrt::make<MockTermSettings>(10000, 0, 9999999);
            Terminal zeroRowsTerminal;
            zeroRowsTerminal.CreateFromSettings(zeroRowsSettings, emptyRenderTarget);
            actualDimensions = zeroRowsTerminal.GetViewport().Dimensions();
            VERIFY_ARE_EQUAL(actualDimensions.Y, 1, L"Row count clamped to 1");
            VERIFY_ARE_EQUAL(actualDimensions.X, SHRT_MAX, L"Column count clamped to SHRT_MAX == " WCS(SHRT_MAX));
        }

        TEST_METHOD(ScrollbackHistorySizeIsClampedToBounds)
        {
            // What is actually clamped is the number of rows in the internal history buffer,
            // which is the *sum* of the history size plus the number of rows
            // actually visible on screen at the moment.

            const unsigned int visibleRowCount = 100;
            DummyRenderTarget emptyRenderTarget;

            // Zero history size is acceptable.
            auto noHistorySettings = winrt::make<MockTermSettings>(0, visibleRowCount, 100);
            Terminal noHistoryTerminal;
            noHistoryTerminal.CreateFromSettings(noHistorySettings, emptyRenderTarget);
            VERIFY_ARE_EQUAL(noHistoryTerminal.GetTextBuffer().TotalRowCount(), visibleRowCount, L"History size of 0 is accepted");

            // Negative history sizes are clamped to zero.
            auto negativeHistorySizeSettings = winrt::make<MockTermSettings>(-100, visibleRowCount, 100);
            Terminal negativeHistorySizeTerminal;
            negativeHistorySizeTerminal.CreateFromSettings(negativeHistorySizeSettings, emptyRenderTarget);
            VERIFY_ARE_EQUAL(negativeHistorySizeTerminal.GetTextBuffer().TotalRowCount(), visibleRowCount, L"Negative history size is clamped to 0");

            // History size + initial visible rows == SHRT_MAX is acceptable.
            auto maxHistorySizeSettings = winrt::make<MockTermSettings>(SHRT_MAX - visibleRowCount, visibleRowCount, 100);
            Terminal maxHistorySizeTerminal;
            maxHistorySizeTerminal.CreateFromSettings(maxHistorySizeSettings, emptyRenderTarget);
            VERIFY_ARE_EQUAL(maxHistorySizeTerminal.GetTextBuffer().TotalRowCount(), static_cast<unsigned int>(SHRT_MAX), L"History size == SHRT_MAX - initial row count is accepted");

            // History size + initial visible rows == SHRT_MAX + 1 will be clamped slightly.
            auto justTooBigHistorySizeSettings = winrt::make<MockTermSettings>(SHRT_MAX - visibleRowCount + 1, visibleRowCount, 100);
            Terminal justTooBigHistorySizeTerminal;
            justTooBigHistorySizeTerminal.CreateFromSettings(justTooBigHistorySizeSettings, emptyRenderTarget);
            VERIFY_ARE_EQUAL(justTooBigHistorySizeTerminal.GetTextBuffer().TotalRowCount(), static_cast<unsigned int>(SHRT_MAX), L"History size == 1 + SHRT_MAX - initial row count is clamped to SHRT_MAX - initial row count");

            // Ridiculously large history sizes are also clamped.
            auto farTooBigHistorySizeSettings = winrt::make<MockTermSettings>(99999999, visibleRowCount, 100);
            Terminal farTooBigHistorySizeTerminal;
            farTooBigHistorySizeTerminal.CreateFromSettings(farTooBigHistorySizeSettings, emptyRenderTarget);
            VERIFY_ARE_EQUAL(farTooBigHistorySizeTerminal.GetTextBuffer().TotalRowCount(), static_cast<unsigned int>(SHRT_MAX), L"History size that is far too large is clamped to SHRT_MAX - initial row count");
        }
    };
}

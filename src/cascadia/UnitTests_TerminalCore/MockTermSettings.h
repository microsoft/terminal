#pragma once

#include "precomp.h"
#include <WexTestClass.h>

#include "DefaultSettings.h"

#include "winrt/Microsoft.Terminal.Settings.h"

using namespace winrt::Microsoft::Terminal::Settings;

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
        bool CopyOnSelect() { return _copyOnSelect; }
        winrt::hstring StartingTitle() { return _startingTitle; }
        bool SuppressApplicationTitle() { return _suppressApplicationTitle; }
        uint32_t SelectionBackground() { return COLOR_WHITE; }

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
        void CopyOnSelect(bool copyOnSelect) { _copyOnSelect = copyOnSelect; }
        void StartingTitle(winrt::hstring const& value) { _startingTitle = value; }
        void SuppressApplicationTitle(bool suppressApplicationTitle) { _suppressApplicationTitle = suppressApplicationTitle; }
        void SelectionBackground(uint32_t) {}

        // other unimplemented methods
        void SetColorTableEntry(int32_t /* index */, uint32_t /* value */) {}

    private:
        int32_t _historySize;
        int32_t _initialRows;
        int32_t _initialCols;
        bool _copyOnSelect{ false };
        bool _suppressApplicationTitle{ false };
        winrt::hstring _startingTitle;
    };
}

#pragma once

#include "pch.h"
#include <WexTestClass.h>

#include "DefaultSettings.h"

#include <winrt/Microsoft.Terminal.Core.h>
#include "../inc/cppwinrt_utils.h"

using namespace winrt::Microsoft::Terminal::Core;

namespace TerminalCoreUnitTests
{
    class MockTermSettings : public winrt::implements<MockTermSettings, ICoreSettings, ICoreAppearance>
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
        til::color DefaultForeground() { return COLOR_WHITE; }
        til::color DefaultBackground() { return COLOR_BLACK; }
        bool SnapOnInput() { return false; }
        bool AltGrAliasing() { return true; }
        til::color CursorColor() { return COLOR_WHITE; }
        CursorStyle CursorShape() const noexcept { return CursorStyle::Vintage; }
        uint32_t CursorHeight() { return 42UL; }
        winrt::hstring WordDelimiters() { return winrt::hstring(DEFAULT_WORD_DELIMITERS); }
        bool CopyOnSelect() { return _copyOnSelect; }
        bool FocusFollowMouse() { return _focusFollowMouse; }
        winrt::hstring StartingTitle() { return _startingTitle; }
        bool SuppressApplicationTitle() { return _suppressApplicationTitle; }
        til::color SelectionBackground() { return COLOR_WHITE; }
        bool ForceVTInput() { return false; }
        ICoreAppearance UnfocusedAppearance() { return {}; };
        winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Core::Color> TabColor() { return nullptr; }
        winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Core::Color> StartingTabColor() { return nullptr; }
        bool TrimBlockSelection() { return false; }
        bool DetectURLs() { return true; }

        // other implemented methods
        til::color GetColorTableEntry(int32_t) const { return 123; }

        // property setters - all unimplemented
        void HistorySize(int32_t) {}
        void InitialRows(int32_t) {}
        void InitialCols(int32_t) {}
        void DefaultForeground(til::color) {}
        void DefaultBackground(til::color) {}
        void SnapOnInput(bool) {}
        void AltGrAliasing(bool) {}
        void CursorColor(til::color) {}
        void CursorShape(CursorStyle const&) noexcept {}
        void CursorHeight(uint32_t) {}
        void WordDelimiters(winrt::hstring) {}
        void CopyOnSelect(bool copyOnSelect) { _copyOnSelect = copyOnSelect; }
        void FocusFollowMouse(bool focusFollowMouse) { _focusFollowMouse = focusFollowMouse; }
        void StartingTitle(winrt::hstring const& value) { _startingTitle = value; }
        void SuppressApplicationTitle(bool suppressApplicationTitle) { _suppressApplicationTitle = suppressApplicationTitle; }
        void SelectionBackground(til::color) {}
        void ForceVTInput(bool) {}
        void UnfocusedAppearance(ICoreAppearance) {}
        void TabColor(const IInspectable&) {}
        void StartingTabColor(const IInspectable&) {}
        void TrimBlockSelection(bool) {}
        void DetectURLs(bool) {}

    private:
        int32_t _historySize;
        int32_t _initialRows;
        int32_t _initialCols;
        bool _copyOnSelect{ false };
        bool _focusFollowMouse{ false };
        bool _suppressApplicationTitle{ false };
        winrt::hstring _startingTitle;
    };
}

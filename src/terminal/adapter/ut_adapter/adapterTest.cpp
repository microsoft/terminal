// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <wextestclass.h>
#include "..\..\inc\consoletaeftemplates.hpp"

#include "adaptDispatch.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace Microsoft
{
    namespace Console
    {
        namespace VirtualTerminal
        {
            class AdapterTest;
            class ConAdapterTestGetSet;
        };
    };
};

enum class CursorY
{
    TOP,
    BOTTOM,
    YCENTER
};

enum class CursorX
{
    LEFT,
    RIGHT,
    XCENTER
};

enum class CursorDirection : size_t
{
    UP = 0,
    DOWN = 1,
    RIGHT = 2,
    LEFT = 3,
    NEXTLINE = 4,
    PREVLINE = 5
};

enum class AbsolutePosition : size_t
{
    CursorHorizontal = 0,
    VerticalLine = 1,
};

using namespace Microsoft::Console::VirtualTerminal;

class TestGetSet final : public ConGetSet
{
public:
    bool GetConsoleScreenBufferInfoEx(CONSOLE_SCREEN_BUFFER_INFOEX& sbiex) const override
    {
        Log::Comment(L"GetConsoleScreenBufferInfoEx MOCK returning data...");

        if (_getConsoleScreenBufferInfoExResult)
        {
            sbiex.dwSize = _bufferSize;
            sbiex.srWindow = _viewport;
            sbiex.dwCursorPosition = _cursorPos;
            sbiex.wAttributes = _attribute;
        }

        return _getConsoleScreenBufferInfoExResult;
    }
    bool SetConsoleScreenBufferInfoEx(const CONSOLE_SCREEN_BUFFER_INFOEX& sbiex) override
    {
        Log::Comment(L"SetConsoleScreenBufferInfoEx MOCK returning data...");

        if (_setConsoleScreenBufferInfoExResult)
        {
            VERIFY_ARE_EQUAL(_expectedCursorPos, sbiex.dwCursorPosition);
            VERIFY_ARE_EQUAL(_expectedScreenBufferSize, sbiex.dwSize);
            VERIFY_ARE_EQUAL(_expectedScreenBufferViewport, sbiex.srWindow);
            VERIFY_ARE_EQUAL(_expectedAttributes, sbiex.wAttributes);
        }
        return _setConsoleScreenBufferInfoExResult;
    }
    bool SetConsoleCursorPosition(const COORD position) override
    {
        Log::Comment(L"SetConsoleCursorPosition MOCK called...");

        if (_setConsoleCursorPositionResult)
        {
            VERIFY_ARE_EQUAL(_expectedCursorPos, position);
            _cursorPos = position;
        }

        return _setConsoleCursorPositionResult;
    }

    bool GetConsoleCursorInfo(CONSOLE_CURSOR_INFO& cursorInfo) const override
    {
        Log::Comment(L"GetConsoleCursorInfo MOCK called...");

        if (_getConsoleCursorInfoResult)
        {
            cursorInfo.dwSize = _cursorSize;
            cursorInfo.bVisible = _cursorVisible;
        }

        return _getConsoleCursorInfoResult;
    }

    bool SetConsoleCursorInfo(const CONSOLE_CURSOR_INFO& cursorInfo) override
    {
        Log::Comment(L"SetConsoleCursorInfo MOCK called...");

        if (_setConsoleCursorInfoResult)
        {
            VERIFY_ARE_EQUAL(_expectedCursorSize, cursorInfo.dwSize);
            VERIFY_ARE_EQUAL(_expectedCursorVisible, !!cursorInfo.bVisible);
        }

        return _setConsoleCursorInfoResult;
    }

    bool SetConsoleWindowInfo(const bool absolute, const SMALL_RECT& window) override
    {
        Log::Comment(L"SetConsoleWindowInfo MOCK called...");

        if (_setConsoleWindowInfoResult)
        {
            VERIFY_ARE_EQUAL(_expectedWindowAbsolute, absolute);
            VERIFY_ARE_EQUAL(_expectedConsoleWindow, window);
            _viewport = window;
        }

        return _setConsoleWindowInfoResult;
    }

    bool PrivateSetCursorKeysMode(const bool applicationMode) override
    {
        Log::Comment(L"PrivateSetCursorKeysMode MOCK called...");

        if (_privateSetCursorKeysModeResult)
        {
            VERIFY_ARE_EQUAL(_cursorKeysApplicationMode, applicationMode);
        }

        return _privateSetCursorKeysModeResult;
    }

    bool PrivateSetKeypadMode(const bool applicationMode) override
    {
        Log::Comment(L"PrivateSetKeypadMode MOCK called...");

        if (_privateSetKeypadModeResult)
        {
            VERIFY_ARE_EQUAL(_keypadApplicationMode, applicationMode);
        }

        return _privateSetKeypadModeResult;
    }

    bool PrivateSetScreenMode(const bool /*reverseMode*/) override
    {
        Log::Comment(L"PrivateSetScreenMode MOCK called...");

        return true;
    }

    bool PrivateSetAutoWrapMode(const bool /*wrapAtEOL*/) override
    {
        Log::Comment(L"PrivateSetAutoWrapMode MOCK called...");

        return false;
    }

    bool PrivateShowCursor(const bool show) override
    {
        Log::Comment(L"PrivateShowCursor MOCK called...");

        if (_privateShowCursorResult)
        {
            VERIFY_ARE_EQUAL(_expectedShowCursor, show);
        }

        return _privateShowCursorResult;
    }

    bool PrivateAllowCursorBlinking(const bool enable) override
    {
        Log::Comment(L"PrivateAllowCursorBlinking MOCK called...");

        if (_privateAllowCursorBlinkingResult)
        {
            VERIFY_ARE_EQUAL(_enable, enable);
        }

        return _privateAllowCursorBlinkingResult;
    }

    bool SetConsoleTextAttribute(const WORD attr) override
    {
        Log::Comment(L"SetConsoleTextAttribute MOCK called...");

        if (_setConsoleTextAttributeResult)
        {
            VERIFY_ARE_EQUAL(_expectedAttribute, attr);
            _attribute = attr;
            _usingRgbColor = false;
        }

        return _setConsoleTextAttributeResult;
    }

    bool PrivateIsVtInputEnabled() const override
    {
        return true;
    }

    bool PrivateSetLegacyAttributes(const WORD attr, const bool foreground, const bool background, const bool meta) override
    {
        Log::Comment(L"PrivateSetLegacyAttributes MOCK called...");
        if (_privateSetLegacyAttributesResult)
        {
            VERIFY_ARE_EQUAL(_expectedForeground, foreground);
            VERIFY_ARE_EQUAL(_expectedBackground, background);
            VERIFY_ARE_EQUAL(_expectedMeta, meta);
            if (foreground)
            {
                WI_UpdateFlagsInMask(_attribute, FG_ATTRS, attr);
            }
            if (background)
            {
                WI_UpdateFlagsInMask(_attribute, BG_ATTRS, attr);
            }
            if (meta)
            {
                WI_UpdateFlagsInMask(_attribute, META_ATTRS, attr);
            }

            VERIFY_ARE_EQUAL(_expectedAttribute, attr);

            _expectedForeground = _expectedBackground = _expectedMeta = false;
        }

        return _privateSetLegacyAttributesResult;
    }

    bool SetConsoleXtermTextAttribute(const int xtermTableEntry, const bool isForeground) override
    {
        Log::Comment(L"SetConsoleXtermTextAttribute MOCK called...");

        if (_setConsoleXtermTextAttributeResult)
        {
            VERIFY_ARE_EQUAL(_expectedIsForeground, isForeground);
            _isForeground = isForeground;
            VERIFY_ARE_EQUAL(_iExpectedXtermTableEntry, xtermTableEntry);
            _xtermTableEntry = xtermTableEntry;
            // if the table entry is less than 16, keep using the legacy attr
            _usingRgbColor = xtermTableEntry > 16;
            if (!_usingRgbColor)
            {
                //Convert the xterm index to the win index
                const auto red = (xtermTableEntry & 0x01) > 0;
                const auto green = (xtermTableEntry & 0x02) > 0;
                const auto blue = (xtermTableEntry & 0x04) > 0;
                const auto bright = (xtermTableEntry & 0x08) > 0;
                WORD winEntry = (red ? 0x4 : 0x0) | (green ? 0x2 : 0x0) | (blue ? 0x1 : 0x0) | (bright ? 0x8 : 0x0);
                _attribute = isForeground ? ((_attribute & 0xF0) | winEntry) : ((winEntry << 4) | (_attribute & 0x0F));
            }
        }

        return _setConsoleXtermTextAttributeResult;
    }

    bool SetConsoleRGBTextAttribute(const COLORREF rgbColor, const bool isForeground) override
    {
        Log::Comment(L"SetConsoleRGBTextAttribute MOCK called...");
        if (_setConsoleRGBTextAttributeResult)
        {
            VERIFY_ARE_EQUAL(_expectedIsForeground, isForeground);
            _isForeground = isForeground;
            VERIFY_ARE_EQUAL(_expectedColor, rgbColor);
            _rgbColor = rgbColor;
            _usingRgbColor = true;
        }

        return _setConsoleRGBTextAttributeResult;
    }

    bool PrivateBoldText(const bool isBold) override
    {
        Log::Comment(L"PrivateBoldText MOCK called...");
        if (_privateBoldTextResult)
        {
            VERIFY_ARE_EQUAL(_expectedIsBold, isBold);
            _isBold = isBold;
            _expectedIsBold = false;
        }
        return !!_privateBoldTextResult;
    }

    bool PrivateGetExtendedTextAttributes(ExtendedAttributes& /*attrs*/)
    {
        Log::Comment(L"PrivateGetExtendedTextAttributes MOCK called...");
        return true;
    }

    bool PrivateSetExtendedTextAttributes(const ExtendedAttributes /*attrs*/)
    {
        Log::Comment(L"PrivateSetExtendedTextAttributes MOCK called...");
        return true;
    }

    bool PrivateGetTextAttributes(TextAttribute& /*attrs*/) const
    {
        Log::Comment(L"PrivateGetTextAttributes MOCK called...");
        return true;
    }

    bool PrivateSetTextAttributes(const TextAttribute& /*attrs*/)
    {
        Log::Comment(L"PrivateSetTextAttributes MOCK called...");
        return true;
    }

    bool PrivateWriteConsoleInputW(std::deque<std::unique_ptr<IInputEvent>>& events,
                                   size_t& eventsWritten) override
    {
        Log::Comment(L"PrivateWriteConsoleInputW MOCK called...");

        if (_privateWriteConsoleInputWResult)
        {
            // move all the input events we were given into local storage so we can test against them
            Log::Comment(NoThrowString().Format(L"Moving %zu input events into local storage...", events.size()));

            _events.clear();
            _events.swap(events);
            eventsWritten = _events.size();
        }

        return _privateWriteConsoleInputWResult;
    }

    bool PrivatePrependConsoleInput(std::deque<std::unique_ptr<IInputEvent>>& events,
                                    size_t& eventsWritten) override
    {
        Log::Comment(L"PrivatePrependConsoleInput MOCK called...");

        if (_privatePrependConsoleInputResult)
        {
            // move all the input events we were given into local storage so we can test against them
            Log::Comment(NoThrowString().Format(L"Moving %zu input events into local storage...", events.size()));

            _events.clear();
            _events.swap(events);
            eventsWritten = _events.size();
        }

        return _privatePrependConsoleInputResult;
    }

    bool PrivateWriteConsoleControlInput(_In_ KeyEvent key) override
    {
        Log::Comment(L"PrivateWriteConsoleControlInput MOCK called...");

        if (_privateWriteConsoleControlInputResult)
        {
            VERIFY_ARE_EQUAL('C', key.GetVirtualKeyCode());
            VERIFY_ARE_EQUAL(0x3, key.GetCharData());
            VERIFY_ARE_EQUAL(true, key.IsCtrlPressed());
        }

        return _privateWriteConsoleControlInputResult;
    }

    bool PrivateSetScrollingRegion(const SMALL_RECT& scrollMargins) override
    {
        Log::Comment(L"PrivateSetScrollingRegion MOCK called...");

        if (_privateSetScrollingRegionResult)
        {
            VERIFY_ARE_EQUAL(_expectedScrollRegion, scrollMargins);
        }

        return _privateSetScrollingRegionResult;
    }

    bool PrivateWarningBell() override
    {
        Log::Comment(L"PrivateWarningBell MOCK called...");
        // We made it through the adapter, woo! Return true.
        return TRUE;
    }

    bool PrivateGetLineFeedMode() const override
    {
        Log::Comment(L"PrivateGetLineFeedMode MOCK called...");
        return _privateGetLineFeedModeResult;
    }

    bool PrivateLineFeed(const bool withReturn) override
    {
        Log::Comment(L"PrivateLineFeed MOCK called...");

        if (_privateLineFeedResult)
        {
            VERIFY_ARE_EQUAL(_expectedLineFeedWithReturn, withReturn);
        }

        return _privateLineFeedResult;
    }

    bool PrivateReverseLineFeed() override
    {
        Log::Comment(L"PrivateReverseLineFeed MOCK called...");
        // We made it through the adapter, woo! Return true.
        return TRUE;
    }

    bool SetConsoleTitleW(const std::wstring_view title)
    {
        Log::Comment(L"SetConsoleTitleW MOCK called...");

        if (_setConsoleTitleWResult)
        {
            // Put into WEX strings for rich logging when they don't compare.
            VERIFY_ARE_EQUAL(String(_expectedWindowTitle.data(), gsl::narrow<int>(_expectedWindowTitle.size())),
                             String(title.data(), gsl::narrow<int>(title.size())));
        }
        return TRUE;
    }

    bool PrivateUseAlternateScreenBuffer() override
    {
        Log::Comment(L"PrivateUseAlternateScreenBuffer MOCK called...");
        return true;
    }

    bool PrivateUseMainScreenBuffer() override
    {
        Log::Comment(L"PrivateUseMainScreenBuffer MOCK called...");
        return true;
    }

    bool PrivateHorizontalTabSet() override
    {
        Log::Comment(L"PrivateHorizontalTabSet MOCK called...");
        // We made it through the adapter, woo! Return true.
        return TRUE;
    }

    bool PrivateForwardTab(const size_t numTabs) override
    {
        Log::Comment(L"PrivateForwardTab MOCK called...");
        if (_privateForwardTabResult)
        {
            VERIFY_ARE_EQUAL(_expectedNumTabs, numTabs);
        }
        return TRUE;
    }

    bool PrivateBackwardsTab(const size_t numTabs) override
    {
        Log::Comment(L"PrivateBackwardsTab MOCK called...");
        if (_privateBackwardsTabResult)
        {
            VERIFY_ARE_EQUAL(_expectedNumTabs, numTabs);
        }
        return TRUE;
    }

    bool PrivateTabClear(const bool clearAll) override
    {
        Log::Comment(L"PrivateTabClear MOCK called...");
        if (_privateTabClearResult)
        {
            VERIFY_ARE_EQUAL(_expectedClearAll, clearAll);
        }
        return TRUE;
    }

    bool PrivateSetDefaultTabStops() override
    {
        Log::Comment(L"PrivateSetDefaultTabStops MOCK called...");
        return TRUE;
    }

    bool PrivateEnableVT200MouseMode(const bool enabled) override
    {
        Log::Comment(L"PrivateEnableVT200MouseMode MOCK called...");
        if (_privateEnableVT200MouseModeResult)
        {
            VERIFY_ARE_EQUAL(_expectedMouseEnabled, enabled);
        }
        return _privateEnableVT200MouseModeResult;
    }

    bool PrivateEnableUTF8ExtendedMouseMode(const bool enabled) override
    {
        Log::Comment(L"PrivateEnableUTF8ExtendedMouseMode MOCK called...");
        if (_privateEnableUTF8ExtendedMouseModeResult)
        {
            VERIFY_ARE_EQUAL(_expectedMouseEnabled, enabled);
        }
        return _privateEnableUTF8ExtendedMouseModeResult;
    }

    bool PrivateEnableSGRExtendedMouseMode(const bool enabled) override
    {
        Log::Comment(L"PrivateEnableSGRExtendedMouseMode MOCK called...");
        if (_privateEnableSGRExtendedMouseModeResult)
        {
            VERIFY_ARE_EQUAL(_expectedMouseEnabled, enabled);
        }
        return _privateEnableSGRExtendedMouseModeResult;
    }

    bool PrivateEnableButtonEventMouseMode(const bool enabled) override
    {
        Log::Comment(L"PrivateEnableButtonEventMouseMode MOCK called...");
        if (_privateEnableButtonEventMouseModeResult)
        {
            VERIFY_ARE_EQUAL(_expectedMouseEnabled, enabled);
        }
        return _privateEnableButtonEventMouseModeResult;
    }

    bool PrivateEnableAnyEventMouseMode(const bool enabled) override
    {
        Log::Comment(L"PrivateEnableAnyEventMouseMode MOCK called...");
        if (_privateEnableAnyEventMouseModeResult)
        {
            VERIFY_ARE_EQUAL(_expectedMouseEnabled, enabled);
        }
        return _privateEnableAnyEventMouseModeResult;
    }

    bool PrivateEnableAlternateScroll(const bool enabled) override
    {
        Log::Comment(L"PrivateEnableAlternateScroll MOCK called...");
        if (_privateEnableAlternateScrollResult)
        {
            VERIFY_ARE_EQUAL(_expectedAlternateScrollEnabled, enabled);
        }
        return _privateEnableAlternateScrollResult;
    }

    bool PrivateEraseAll() override
    {
        Log::Comment(L"PrivateEraseAll MOCK called...");
        return TRUE;
    }

    bool SetCursorStyle(const CursorType cursorType) override
    {
        Log::Comment(L"SetCursorStyle MOCK called...");
        if (_setCursorStyleResult)
        {
            VERIFY_ARE_EQUAL(_expectedCursorStyle, cursorType);
        }
        return _setCursorStyleResult;
    }

    bool SetCursorColor(const COLORREF cursorColor) override
    {
        Log::Comment(L"SetCursorColor MOCK called...");
        if (_setCursorColorResult)
        {
            VERIFY_ARE_EQUAL(_expectedCursorColor, cursorColor);
        }
        return _setCursorColorResult;
    }

    bool PrivateGetConsoleScreenBufferAttributes(WORD& attributes) override
    {
        Log::Comment(L"PrivateGetConsoleScreenBufferAttributes MOCK returning data...");

        if (_privateGetConsoleScreenBufferAttributesResult)
        {
            attributes = _attribute;
        }

        return _privateGetConsoleScreenBufferAttributesResult;
    }

    bool PrivateRefreshWindow() override
    {
        Log::Comment(L"PrivateRefreshWindow MOCK called...");
        // We made it through the adapter, woo! Return true.
        return TRUE;
    }

    bool PrivateSuppressResizeRepaint() override
    {
        Log::Comment(L"PrivateSuppressResizeRepaint MOCK called...");
        VERIFY_IS_TRUE(false, L"AdaptDispatch should never be calling this function.");
        return FALSE;
    }

    bool GetConsoleOutputCP(unsigned int& codepage) override
    {
        Log::Comment(L"GetConsoleOutputCP MOCK called...");
        if (_getConsoleOutputCPResult)
        {
            codepage = _expectedOutputCP;
        }
        return _getConsoleOutputCPResult;
    }

    bool IsConsolePty(bool& isPty) const override
    {
        Log::Comment(L"IsConsolePty MOCK called...");
        if (_isConsolePtyResult)
        {
            isPty = _isPty;
        }
        return _isConsolePtyResult;
    }

    bool DeleteLines(const size_t /*count*/) override
    {
        Log::Comment(L"DeleteLines MOCK called...");
        return TRUE;
    }

    bool InsertLines(const size_t /*count*/) override
    {
        Log::Comment(L"InsertLines MOCK called...");
        return TRUE;
    }

    bool PrivateSetDefaultAttributes(const bool foreground,
                                     const bool background) override
    {
        Log::Comment(L"PrivateSetDefaultAttributes MOCK called...");
        if (_privateSetDefaultAttributesResult)
        {
            VERIFY_ARE_EQUAL(_expectedForeground, foreground);
            VERIFY_ARE_EQUAL(_expectedBackground, background);
            if (foreground)
            {
                WI_UpdateFlagsInMask(_attribute, FG_ATTRS, s_defaultFill);
            }
            if (background)
            {
                WI_UpdateFlagsInMask(_attribute, BG_ATTRS, s_defaultFill);
            }

            _expectedForeground = _expectedBackground = false;
        }
        return _privateSetDefaultAttributesResult;
    }

    bool MoveToBottom() const override
    {
        Log::Comment(L"MoveToBottom MOCK called...");
        return _moveToBottomResult;
    }

    bool PrivateSetColorTableEntry(const short index, const COLORREF value) const noexcept override
    {
        Log::Comment(L"PrivateSetColorTableEntry MOCK called...");
        if (_privateSetColorTableEntryResult)
        {
            VERIFY_ARE_EQUAL(_expectedColorTableIndex, index);
            VERIFY_ARE_EQUAL(_expectedColorValue, value);
        }

        return _privateSetColorTableEntryResult;
    }

    bool PrivateSetDefaultForeground(const COLORREF value) const noexcept override
    {
        Log::Comment(L"PrivateSetDefaultForeground MOCK called...");
        if (_privateSetDefaultForegroundResult)
        {
            VERIFY_ARE_EQUAL(_expectedDefaultForegroundColorValue, value);
        }

        return _privateSetDefaultForegroundResult;
    }

    bool PrivateSetDefaultBackground(const COLORREF value) const noexcept override
    {
        Log::Comment(L"PrivateSetDefaultForeground MOCK called...");
        if (_privateSetDefaultBackgroundResult)
        {
            VERIFY_ARE_EQUAL(_expectedDefaultBackgroundColorValue, value);
        }

        return _privateSetDefaultBackgroundResult;
    }

    bool PrivateFillRegion(const COORD /*startPosition*/,
                           const size_t /*fillLength*/,
                           const wchar_t /*fillChar*/,
                           const bool /*standardFillAttrs*/) noexcept override
    {
        Log::Comment(L"PrivateFillRegion MOCK called...");

        return TRUE;
    }

    bool PrivateScrollRegion(const SMALL_RECT /*scrollRect*/,
                             const std::optional<SMALL_RECT> /*clipRect*/,
                             const COORD /*destinationOrigin*/,
                             const bool /*standardFillAttrs*/) noexcept override
    {
        Log::Comment(L"PrivateScrollRegion MOCK called...");

        return TRUE;
    }

    void PrepData()
    {
        PrepData(CursorDirection::UP); // if called like this, the cursor direction doesn't matter.
    }

    void PrepData(CursorDirection dir)
    {
        switch (dir)
        {
        case CursorDirection::UP:
            return PrepData(CursorX::LEFT, CursorY::TOP);
        case CursorDirection::DOWN:
            return PrepData(CursorX::LEFT, CursorY::BOTTOM);
        case CursorDirection::LEFT:
            return PrepData(CursorX::LEFT, CursorY::TOP);
        case CursorDirection::RIGHT:
            return PrepData(CursorX::RIGHT, CursorY::TOP);
        case CursorDirection::NEXTLINE:
            return PrepData(CursorX::LEFT, CursorY::BOTTOM);
        case CursorDirection::PREVLINE:
            return PrepData(CursorX::LEFT, CursorY::TOP);
        }
    }

    void PrepData(CursorX xact, CursorY yact)
    {
        Log::Comment(L"Resetting mock data state.");

        // APIs succeed by default
        _setConsoleCursorPositionResult = TRUE;
        _getConsoleScreenBufferInfoExResult = TRUE;
        _getConsoleCursorInfoResult = TRUE;
        _setConsoleCursorInfoResult = TRUE;
        _setConsoleTextAttributeResult = TRUE;
        _privateWriteConsoleInputWResult = TRUE;
        _privatePrependConsoleInputResult = TRUE;
        _privateWriteConsoleControlInputResult = TRUE;
        _setConsoleWindowInfoResult = TRUE;
        _privateGetConsoleScreenBufferAttributesResult = TRUE;
        _moveToBottomResult = true;

        _bufferSize.X = 100;
        _bufferSize.Y = 600;

        // Viewport sitting in the "middle" of the buffer somewhere (so all sides have excess buffer around them)
        _viewport.Top = 20;
        _viewport.Bottom = 49;
        _viewport.Left = 30;
        _viewport.Right = 59;

        // Call cursor positions separately
        PrepCursor(xact, yact);

        _cursorSize = 33;
        _expectedCursorSize = _cursorSize;

        _cursorVisible = TRUE;
        _expectedCursorVisible = _cursorVisible;

        // Attribute default is gray on black.
        _attribute = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
        _expectedAttribute = _attribute;
    }

    void PrepCursor(CursorX xact, CursorY yact)
    {
        Log::Comment(L"Adjusting cursor within viewport... Expected will match actual when done.");

        switch (xact)
        {
        case CursorX::LEFT:
            Log::Comment(L"Cursor set to left edge of buffer.");
            _cursorPos.X = 0;
            break;
        case CursorX::RIGHT:
            Log::Comment(L"Cursor set to right edge of buffer.");
            _cursorPos.X = _bufferSize.X - 1;
            break;
        case CursorX::XCENTER:
            Log::Comment(L"Cursor set to centered X of buffer.");
            _cursorPos.X = _bufferSize.X / 2;
            break;
        }

        switch (yact)
        {
        case CursorY::TOP:
            Log::Comment(L"Cursor set to top edge of viewport.");
            _cursorPos.Y = _viewport.Top;
            break;
        case CursorY::BOTTOM:
            Log::Comment(L"Cursor set to bottom edge of viewport.");
            _cursorPos.Y = _viewport.Bottom - 1;
            break;
        case CursorY::YCENTER:
            Log::Comment(L"Cursor set to centered Y of viewport.");
            _cursorPos.Y = _viewport.Top + ((_viewport.Bottom - _viewport.Top) / 2);
            break;
        }

        _expectedCursorPos = _cursorPos;
    }

    void ValidateInputEvent(_In_ PCWSTR pwszExpectedResponse)
    {
        size_t const cchResponse = wcslen(pwszExpectedResponse);
        size_t const eventCount = _events.size();

        VERIFY_ARE_EQUAL(cchResponse * 2, eventCount, L"We should receive TWO input records for every character in the expected string. Key down and key up.");

        for (size_t iInput = 0; iInput < eventCount; iInput++)
        {
            wchar_t const wch = pwszExpectedResponse[iInput / 2]; // the same portion of the string will be used twice. 0/2 = 0. 1/2 = 0. 2/2 = 1. 3/2 = 1. and so on.

            VERIFY_ARE_EQUAL(InputEventType::KeyEvent, _events[iInput]->EventType());

            const KeyEvent* const keyEvent = static_cast<const KeyEvent* const>(_events[iInput].get());

            // every even key is down. every odd key is up. DOWN = 0, UP = 1. DOWN = 2, UP = 3. and so on.
            VERIFY_ARE_EQUAL((bool)!(iInput % 2), keyEvent->IsKeyDown());
            VERIFY_ARE_EQUAL(0u, keyEvent->GetActiveModifierKeys());
            Log::Comment(NoThrowString().Format(L"Comparing '%c' with '%c'...", wch, keyEvent->GetCharData()));
            VERIFY_ARE_EQUAL(wch, keyEvent->GetCharData());
            VERIFY_ARE_EQUAL(1u, keyEvent->GetRepeatCount());
            VERIFY_ARE_EQUAL(0u, keyEvent->GetVirtualKeyCode());
            VERIFY_ARE_EQUAL(0u, keyEvent->GetVirtualScanCode());
        }
    }

    void _SetMarginsHelper(SMALL_RECT* rect, SHORT top, SHORT bottom)
    {
        rect->Top = top;
        rect->Bottom = bottom;
        //The rectangle is going to get converted from VT space to conhost space
        _expectedScrollRegion.Top = (top > 0) ? rect->Top - 1 : rect->Top;
        _expectedScrollRegion.Bottom = (bottom > 0) ? rect->Bottom - 1 : rect->Bottom;
    }

    ~TestGetSet()
    {
    }

    static const WCHAR s_wchErase = (WCHAR)0x20;
    static const WCHAR s_wchDefault = L'Z';
    static const WORD s_wAttrErase = FOREGROUND_BLUE | FOREGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY;
    static const WORD s_wDefaultAttribute = 0;
    static const WORD s_defaultFill = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED; // dark gray on black.

    std::deque<std::unique_ptr<IInputEvent>> _events;

    COORD _bufferSize = { 0, 0 };
    SMALL_RECT _viewport = { 0, 0, 0, 0 };
    SMALL_RECT _expectedConsoleWindow = { 0, 0, 0, 0 };
    COORD _cursorPos = { 0, 0 };
    SMALL_RECT _expectedScrollRegion = { 0, 0, 0, 0 };

    DWORD _cursorSize = 0;
    bool _cursorVisible = false;

    COORD _expectedCursorPos = { 0, 0 };
    DWORD _expectedCursorSize = 0;
    bool _expectedCursorVisible = false;

    WORD _attribute = 0;
    WORD _expectedAttribute = 0;
    int _xtermTableEntry = 0;
    int _iExpectedXtermTableEntry = 0;
    COLORREF _rgbColor = 0;
    COLORREF _expectedColor = 0;
    bool _isForeground = false;
    bool _expectedIsForeground = false;
    bool _usingRgbColor = false;
    bool _expectedForeground = false;
    bool _expectedBackground = false;
    bool _expectedMeta = false;
    unsigned int _expectedOutputCP = 0;
    bool _isPty = false;
    bool _privateBoldTextResult = false;
    bool _expectedIsBold = false;
    bool _isBold = false;

    bool _privateShowCursorResult = false;
    bool _expectedShowCursor = false;

    bool _getConsoleScreenBufferInfoExResult = false;
    bool _setConsoleCursorPositionResult = false;
    bool _getConsoleCursorInfoResult = false;
    bool _setConsoleCursorInfoResult = false;
    bool _setConsoleTextAttributeResult = false;
    bool _privateWriteConsoleInputWResult = false;
    bool _privatePrependConsoleInputResult = false;
    bool _privateWriteConsoleControlInputResult = false;

    bool _setConsoleWindowInfoResult = false;
    bool _expectedWindowAbsolute = false;
    bool _setConsoleScreenBufferInfoExResult = false;

    COORD _expectedScreenBufferSize = { 0, 0 };
    SMALL_RECT _expectedScreenBufferViewport{ 0, 0, 0, 0 };
    WORD _expectedAttributes = 0;
    bool _privateSetCursorKeysModeResult = false;
    bool _privateSetKeypadModeResult = false;
    bool _cursorKeysApplicationMode = false;
    bool _keypadApplicationMode = false;
    bool _privateAllowCursorBlinkingResult = false;
    bool _enable = false; // for cursor blinking
    bool _privateSetScrollingRegionResult = false;
    bool _privateGetLineFeedModeResult = false;
    bool _privateLineFeedResult = false;
    bool _expectedLineFeedWithReturn = false;
    bool _privateReverseLineFeedResult = false;

    bool _setConsoleTitleWResult = false;
    std::wstring_view _expectedWindowTitle{};
    bool _privateHorizontalTabSetResult = false;
    bool _privateForwardTabResult = false;
    bool _privateBackwardsTabResult = false;
    size_t _expectedNumTabs = 0;
    bool _privateTabClearResult = false;
    bool _expectedClearAll = false;
    bool _expectedMouseEnabled = false;
    bool _expectedAlternateScrollEnabled = false;
    bool _privateEnableVT200MouseModeResult = false;
    bool _privateEnableUTF8ExtendedMouseModeResult = false;
    bool _privateEnableSGRExtendedMouseModeResult = false;
    bool _privateEnableButtonEventMouseModeResult = false;
    bool _privateEnableAnyEventMouseModeResult = false;
    bool _privateEnableAlternateScrollResult = false;
    bool _setConsoleXtermTextAttributeResult = false;
    bool _setConsoleRGBTextAttributeResult = false;
    bool _privateSetLegacyAttributesResult = false;
    bool _privateGetConsoleScreenBufferAttributesResult = false;
    bool _setCursorStyleResult = false;
    CursorType _expectedCursorStyle;
    bool _setCursorColorResult = false;
    COLORREF _expectedCursorColor = 0;
    bool _getConsoleOutputCPResult = false;
    bool _isConsolePtyResult = false;
    bool _privateSetDefaultAttributesResult = false;
    bool _moveToBottomResult = false;

    bool _privateSetColorTableEntryResult = false;
    short _expectedColorTableIndex = -1;
    COLORREF _expectedColorValue = INVALID_COLOR;

    bool _privateSetDefaultForegroundResult = false;
    COLORREF _expectedDefaultForegroundColorValue = INVALID_COLOR;

    bool _privateSetDefaultBackgroundResult = false;
    COLORREF _expectedDefaultBackgroundColorValue = INVALID_COLOR;

private:
    HANDLE _hCon;
};

class DummyAdapter : public AdaptDefaults
{
    void Print(const wchar_t /*wch*/) override
    {
    }

    void PrintString(const std::wstring_view /*string*/) override
    {
    }

    void Execute(const wchar_t /*wch*/) override
    {
    }
};

class AdapterTest
{
public:
    TEST_CLASS(AdapterTest);

    TEST_METHOD_SETUP(SetupMethods)
    {
        bool fSuccess = true;

        auto api = std::make_unique<TestGetSet>();
        fSuccess = api.get() != nullptr;
        if (fSuccess)
        {
            auto adapter = std::make_unique<DummyAdapter>();

            // give AdaptDispatch ownership of _testGetSet
            _testGetSet = api.get(); // keep a copy for us but don't manage its lifetime anymore.
            _pDispatch = std::make_unique<AdaptDispatch>(std::move(api), std::move(adapter));
            fSuccess = _pDispatch != nullptr;
        }
        return fSuccess;
    }

    TEST_METHOD_CLEANUP(CleanupMethods)
    {
        _pDispatch.reset();
        _testGetSet = nullptr;
        return true;
    }

    TEST_METHOD(CursorMovementTest)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:uiDirection", L"{0, 1, 2, 3, 4, 5}") // These values align with the CursorDirection enum class to try all the directions.
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");

        // Used to switch between the various function options.
        typedef bool (AdaptDispatch::*CursorMoveFunc)(size_t);
        CursorMoveFunc moveFunc = nullptr;

        // Modify variables based on directionality of this test
        CursorDirection direction;
        size_t dir;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiDirection", dir));
        direction = (CursorDirection)dir;

        switch (direction)
        {
        case CursorDirection::UP:
            Log::Comment(L"Testing up direction.");
            moveFunc = &AdaptDispatch::CursorUp;
            break;
        case CursorDirection::DOWN:
            Log::Comment(L"Testing down direction.");
            moveFunc = &AdaptDispatch::CursorDown;
            break;
        case CursorDirection::RIGHT:
            Log::Comment(L"Testing right direction.");
            moveFunc = &AdaptDispatch::CursorForward;
            break;
        case CursorDirection::LEFT:
            Log::Comment(L"Testing left direction.");
            moveFunc = &AdaptDispatch::CursorBackward;
            break;
        case CursorDirection::NEXTLINE:
            Log::Comment(L"Testing next line direction.");
            moveFunc = &AdaptDispatch::CursorNextLine;
            break;
        case CursorDirection::PREVLINE:
            Log::Comment(L"Testing prev line direction.");
            moveFunc = &AdaptDispatch::CursorPrevLine;
            break;
        }

        if (moveFunc == nullptr)
        {
            VERIFY_FAIL();
            return;
        }

        // success cases
        // place cursor in top left. moving up is expected to go nowhere (it should get bounded by the viewport)
        Log::Comment(L"Test 1: Cursor doesn't move when placed in corner of viewport.");
        _testGetSet->PrepData(direction);

        VERIFY_IS_TRUE((_pDispatch.get()->*(moveFunc))(1));

        Log::Comment(L"Test 1b: Cursor moves to left of line with next/prev line command when cursor can't move higher/lower.");

        bool fDoTest1b = false;

        switch (direction)
        {
        case CursorDirection::NEXTLINE:
            _testGetSet->PrepData(CursorX::RIGHT, CursorY::BOTTOM);
            fDoTest1b = true;
            break;
        case CursorDirection::PREVLINE:
            _testGetSet->PrepData(CursorX::RIGHT, CursorY::TOP);
            fDoTest1b = true;
            break;
        }

        if (fDoTest1b)
        {
            _testGetSet->_expectedCursorPos.X = 0;
            VERIFY_IS_TRUE((_pDispatch.get()->*(moveFunc))(1));
        }
        else
        {
            Log::Comment(L"Test not applicable to direction selected. Skipping.");
        }

        // place cursor lower, move up 1.
        Log::Comment(L"Test 2: Cursor moves 1 in the correct direction from viewport.");
        _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);

        switch (direction)
        {
        case CursorDirection::UP:
            _testGetSet->_expectedCursorPos.Y--;
            break;
        case CursorDirection::DOWN:
            _testGetSet->_expectedCursorPos.Y++;
            break;
        case CursorDirection::RIGHT:
            _testGetSet->_expectedCursorPos.X++;
            break;
        case CursorDirection::LEFT:
            _testGetSet->_expectedCursorPos.X--;
            break;
        case CursorDirection::NEXTLINE:
            _testGetSet->_expectedCursorPos.Y++;
            _testGetSet->_expectedCursorPos.X = 0;
            break;
        case CursorDirection::PREVLINE:
            _testGetSet->_expectedCursorPos.Y--;
            _testGetSet->_expectedCursorPos.X = 0;
            break;
        }

        VERIFY_IS_TRUE((_pDispatch.get()->*(moveFunc))(1));

        // place cursor and move it up too far. It should get bounded by the viewport.
        Log::Comment(L"Test 3: Cursor moves and gets stuck at viewport when started away from edges and moved beyond edges.");
        _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);

        // Bottom and right viewports are -1 because those two sides are specified to be 1 outside the viewable area.

        switch (direction)
        {
        case CursorDirection::UP:
            _testGetSet->_expectedCursorPos.Y = _testGetSet->_viewport.Top;
            break;
        case CursorDirection::DOWN:
            _testGetSet->_expectedCursorPos.Y = _testGetSet->_viewport.Bottom - 1;
            break;
        case CursorDirection::RIGHT:
            _testGetSet->_expectedCursorPos.X = _testGetSet->_bufferSize.X - 1;
            break;
        case CursorDirection::LEFT:
            _testGetSet->_expectedCursorPos.X = 0;
            break;
        case CursorDirection::NEXTLINE:
            _testGetSet->_expectedCursorPos.X = 0;
            _testGetSet->_expectedCursorPos.Y = _testGetSet->_viewport.Bottom - 1;
            break;
        case CursorDirection::PREVLINE:
            _testGetSet->_expectedCursorPos.X = 0;
            _testGetSet->_expectedCursorPos.Y = _testGetSet->_viewport.Top;
            break;
        }

        VERIFY_IS_TRUE((_pDispatch.get()->*(moveFunc))(100));

        // error cases
        // SetConsoleCursorPosition throws failure. Parameters are otherwise normal.
        Log::Comment(L"Test 4: When SetConsoleCursorPosition throws a failure, call fails and cursor doesn't move.");
        _testGetSet->PrepData(direction);
        _testGetSet->_setConsoleCursorPositionResult = FALSE;

        VERIFY_IS_FALSE((_pDispatch.get()->*(moveFunc))(0));
        VERIFY_ARE_EQUAL(_testGetSet->_expectedCursorPos, _testGetSet->_cursorPos);

        // GetConsoleScreenBufferInfo throws failure. Parameters are otherwise normal.
        Log::Comment(L"Test 5: When GetConsoleScreenBufferInfo throws a failure, call fails and cursor doesn't move.");
        _testGetSet->PrepData(CursorX::LEFT, CursorY::TOP);
        _testGetSet->_getConsoleScreenBufferInfoExResult = FALSE;
        VERIFY_IS_FALSE((_pDispatch.get()->*(moveFunc))(0));
        VERIFY_ARE_EQUAL(_testGetSet->_expectedCursorPos, _testGetSet->_cursorPos);
    }

    TEST_METHOD(CursorPositionTest)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Place cursor within the viewport. Start from top left, move to middle.");
        _testGetSet->PrepData(CursorX::LEFT, CursorY::TOP);

        short sCol = (_testGetSet->_viewport.Right - _testGetSet->_viewport.Left) / 2;
        short sRow = (_testGetSet->_viewport.Bottom - _testGetSet->_viewport.Top) / 2;

        // The X coordinate is unaffected by the viewport.
        _testGetSet->_expectedCursorPos.X = sCol - 1;
        _testGetSet->_expectedCursorPos.Y = _testGetSet->_viewport.Top + (sRow - 1);

        VERIFY_IS_TRUE(_pDispatch.get()->CursorPosition(sRow, sCol));

        Log::Comment(L"Test 2: Move to 0, 0 (which is 1,1 in VT speak)");
        _testGetSet->PrepData(CursorX::RIGHT, CursorY::BOTTOM);

        // The X coordinate is unaffected by the viewport.
        _testGetSet->_expectedCursorPos.X = 0;
        _testGetSet->_expectedCursorPos.Y = _testGetSet->_viewport.Top;

        VERIFY_IS_TRUE(_pDispatch.get()->CursorPosition(1, 1));

        Log::Comment(L"Test 3: Move beyond rectangle (down/right too far). Should be bounded back in.");
        _testGetSet->PrepData(CursorX::LEFT, CursorY::TOP);

        sCol = (_testGetSet->_bufferSize.X) * 2;
        sRow = (_testGetSet->_viewport.Bottom - _testGetSet->_viewport.Top) * 2;

        _testGetSet->_expectedCursorPos.X = _testGetSet->_bufferSize.X - 1;
        _testGetSet->_expectedCursorPos.Y = _testGetSet->_viewport.Bottom - 1;

        VERIFY_IS_TRUE(_pDispatch.get()->CursorPosition(sRow, sCol));

        Log::Comment(L"Test 4: GetConsoleInfo API returns false. No move, return false.");
        _testGetSet->PrepData(CursorX::LEFT, CursorY::TOP);

        _testGetSet->_getConsoleScreenBufferInfoExResult = FALSE;

        VERIFY_IS_FALSE(_pDispatch.get()->CursorPosition(1, 1));

        Log::Comment(L"Test 5: SetCursor API returns false. No move, return false.");
        _testGetSet->PrepData(CursorX::LEFT, CursorY::TOP);

        _testGetSet->_setConsoleCursorPositionResult = FALSE;

        VERIFY_IS_FALSE(_pDispatch.get()->CursorPosition(1, 1));
    }

    TEST_METHOD(CursorSingleDimensionMoveTest)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:uiDirection", L"{0, 1}") // These values align with the CursorDirection enum class to try all the directions.
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");

        //// Used to switch between the various function options.
        typedef bool (AdaptDispatch::*CursorMoveFunc)(size_t);
        CursorMoveFunc moveFunc = nullptr;
        SHORT sRangeEnd = 0;
        SHORT sRangeStart = 0;
        SHORT* psCursorExpected = nullptr;

        // Modify variables based on directionality of this test
        AbsolutePosition direction;
        size_t dir;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiDirection", dir));
        direction = (AbsolutePosition)dir;
        _testGetSet->PrepData();

        switch (direction)
        {
        case AbsolutePosition::CursorHorizontal:
            Log::Comment(L"Testing cursor horizontal movement.");
            sRangeEnd = _testGetSet->_bufferSize.X;
            sRangeStart = 0;
            psCursorExpected = &_testGetSet->_expectedCursorPos.X;
            moveFunc = &AdaptDispatch::CursorHorizontalPositionAbsolute;
            break;
        case AbsolutePosition::VerticalLine:
            Log::Comment(L"Testing vertical line movement.");
            sRangeEnd = _testGetSet->_viewport.Bottom;
            sRangeStart = _testGetSet->_viewport.Top;
            psCursorExpected = &_testGetSet->_expectedCursorPos.Y;
            moveFunc = &AdaptDispatch::VerticalLinePositionAbsolute;
            break;
        }

        if (moveFunc == nullptr || psCursorExpected == nullptr)
        {
            VERIFY_FAIL();
            return;
        }

        Log::Comment(L"Test 1: Place cursor within the viewport. Start from top left, move to middle.");
        _testGetSet->PrepData(CursorX::LEFT, CursorY::TOP);

        short sVal = (sRangeEnd - sRangeStart) / 2;

        *psCursorExpected = sRangeStart + (sVal - 1);

        VERIFY_IS_TRUE((_pDispatch.get()->*(moveFunc))(sVal));

        Log::Comment(L"Test 2: Move to 0 (which is 1 in VT speak)");
        _testGetSet->PrepData(CursorX::RIGHT, CursorY::BOTTOM);

        *psCursorExpected = sRangeStart;
        sVal = 1;

        VERIFY_IS_TRUE((_pDispatch.get()->*(moveFunc))(sVal));

        Log::Comment(L"Test 3: Move beyond rectangle (down/right too far). Should be bounded back in.");
        _testGetSet->PrepData(CursorX::LEFT, CursorY::TOP);

        sVal = (sRangeEnd - sRangeStart) * 2;

        *psCursorExpected = sRangeEnd - 1;

        VERIFY_IS_TRUE((_pDispatch.get()->*(moveFunc))(sVal));

        Log::Comment(L"Test 4: GetConsoleInfo API returns false. No move, return false.");
        _testGetSet->PrepData(CursorX::LEFT, CursorY::TOP);

        _testGetSet->_getConsoleScreenBufferInfoExResult = FALSE;

        sVal = 1;

        VERIFY_IS_FALSE((_pDispatch.get()->*(moveFunc))(sVal));

        Log::Comment(L"Test 5: SetCursor API returns false. No move, return false.");
        _testGetSet->PrepData(CursorX::LEFT, CursorY::TOP);

        _testGetSet->_setConsoleCursorPositionResult = FALSE;

        sVal = 1;

        VERIFY_IS_FALSE((_pDispatch.get()->*(moveFunc))(sVal));
    }

    TEST_METHOD(CursorSaveRestoreTest)
    {
        Log::Comment(L"Starting test...");

        COORD coordExpected = { 0 };

        Log::Comment(L"Test 1: Restore with no saved data should move to top-left corner, the null/default position.");

        // Move cursor to top left and save off expected position.
        _testGetSet->PrepData(CursorX::LEFT, CursorY::TOP);
        coordExpected = _testGetSet->_expectedCursorPos;

        // Then move cursor to the middle and reset the expected to the top left.
        _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);
        _testGetSet->_expectedCursorPos = coordExpected;

        VERIFY_IS_TRUE(_pDispatch.get()->CursorRestoreState(), L"By default, restore to top left corner (0,0 offset from viewport).");

        Log::Comment(L"Test 2: Place cursor in center. Save. Move cursor to corner. Restore. Should come back to center.");
        _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);
        VERIFY_IS_TRUE(_pDispatch.get()->CursorSaveState(), L"Succeed at saving position.");

        Log::Comment(L"Backup expected cursor (in the middle). Move cursor to corner. Then re-set expected cursor to middle.");
        // save expected cursor position
        coordExpected = _testGetSet->_expectedCursorPos;

        // adjust cursor to corner
        _testGetSet->PrepData(CursorX::LEFT, CursorY::BOTTOM);

        // restore expected cursor position to center.
        _testGetSet->_expectedCursorPos = coordExpected;

        VERIFY_IS_TRUE(_pDispatch.get()->CursorRestoreState(), L"Restoring to corner should succeed. API call inside will test that cursor matched expected position.");
    }

    TEST_METHOD(CursorHideShowTest)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:fStartingVis", L"{TRUE, FALSE}")
            TEST_METHOD_PROPERTY(L"Data:fEndingVis", L"{TRUE, FALSE}")
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");

        // Modify variables based on permutations of this test.
        bool fStart;
        bool fEnd;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"fStartingVis", fStart));
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"fEndingVis", fEnd));

        Log::Comment(L"Test 1: Verify successful API call modifies visibility state.");
        _testGetSet->PrepData();
        _testGetSet->_cursorVisible = fStart;
        _testGetSet->_privateShowCursorResult = true;
        _testGetSet->_expectedShowCursor = fEnd;
        VERIFY_IS_TRUE(_pDispatch.get()->CursorVisibility(fEnd));

        Log::Comment(L"Test 3: When we fail to set updated cursor information, the dispatch should fail.");
        _testGetSet->PrepData();
        _testGetSet->_privateShowCursorResult = false;
        VERIFY_IS_FALSE(_pDispatch.get()->CursorVisibility(fEnd));
    }

    TEST_METHOD(GraphicsBaseTests)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Send no options.");

        _testGetSet->PrepData();

        DispatchTypes::GraphicsOptions rgOptions[16];
        size_t cOptions = 0;

        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));

        Log::Comment(L"Test 2: Gracefully fail when getting buffer information fails.");

        _testGetSet->PrepData();
        _testGetSet->_privateGetConsoleScreenBufferAttributesResult = FALSE;

        VERIFY_IS_FALSE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));

        Log::Comment(L"Test 3: Gracefully fail when setting attribute data fails.");

        _testGetSet->PrepData();
        _testGetSet->_setConsoleTextAttributeResult = FALSE;
        // Need at least one option in order for the call to be able to fail.
        rgOptions[0] = (DispatchTypes::GraphicsOptions)0;
        cOptions = 1;
        VERIFY_IS_FALSE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));
    }

    TEST_METHOD(GraphicsSingleTests)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:uiGraphicsOptions", L"{0, 1, 4, 7, 24, 27, 30, 31, 32, 33, 34, 35, 36, 37, 39, 40, 41, 42, 43, 44, 45, 46, 47, 49, 90, 91, 92, 93, 94, 95, 96, 97, 100, 101, 102, 103, 104, 105, 106, 107}") // corresponds to options in DispatchTypes::GraphicsOptions
        END_TEST_METHOD_PROPERTIES()

        Log::Comment(L"Starting test...");
        _testGetSet->PrepData();

        // Modify variables based on type of this test
        DispatchTypes::GraphicsOptions graphicsOption;
        size_t uiGraphicsOption;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiGraphicsOptions", uiGraphicsOption));
        graphicsOption = (DispatchTypes::GraphicsOptions)uiGraphicsOption;

        DispatchTypes::GraphicsOptions rgOptions[16];
        size_t cOptions = 1;
        rgOptions[0] = graphicsOption;

        _testGetSet->_privateSetLegacyAttributesResult = TRUE;

        switch (graphicsOption)
        {
        case DispatchTypes::GraphicsOptions::Off:
            Log::Comment(L"Testing graphics 'Off/Reset'");
            _testGetSet->_attribute = (WORD)~_testGetSet->s_defaultFill;
            _testGetSet->_expectedAttribute = 0;
            _testGetSet->_privateSetDefaultAttributesResult = true;
            _testGetSet->_expectedForeground = true;
            _testGetSet->_expectedBackground = true;
            _testGetSet->_expectedMeta = true;
            _testGetSet->_privateBoldTextResult = true;
            _testGetSet->_expectedIsBold = false;

            break;
        case DispatchTypes::GraphicsOptions::BoldBright:
            Log::Comment(L"Testing graphics 'Bold/Bright'");
            _testGetSet->_attribute = 0;
            _testGetSet->_expectedAttribute = FOREGROUND_INTENSITY;
            _testGetSet->_expectedForeground = true;
            _testGetSet->_privateBoldTextResult = true;
            _testGetSet->_expectedIsBold = true;
            break;
        case DispatchTypes::GraphicsOptions::Underline:
            Log::Comment(L"Testing graphics 'Underline'");
            _testGetSet->_attribute = 0;
            _testGetSet->_expectedAttribute = COMMON_LVB_UNDERSCORE;
            _testGetSet->_expectedMeta = true;
            break;
        case DispatchTypes::GraphicsOptions::Negative:
            Log::Comment(L"Testing graphics 'Negative'");
            _testGetSet->_attribute = 0;
            _testGetSet->_expectedAttribute = COMMON_LVB_REVERSE_VIDEO;
            _testGetSet->_expectedMeta = true;
            break;
        case DispatchTypes::GraphicsOptions::NoUnderline:
            Log::Comment(L"Testing graphics 'No Underline'");
            _testGetSet->_attribute = COMMON_LVB_UNDERSCORE;
            _testGetSet->_expectedAttribute = 0;
            _testGetSet->_expectedMeta = true;
            break;
        case DispatchTypes::GraphicsOptions::Positive:
            Log::Comment(L"Testing graphics 'Positive'");
            _testGetSet->_attribute = COMMON_LVB_REVERSE_VIDEO;
            _testGetSet->_expectedAttribute = 0;
            _testGetSet->_expectedMeta = true;
            break;
        case DispatchTypes::GraphicsOptions::ForegroundBlack:
            Log::Comment(L"Testing graphics 'Foreground Color Black'");
            _testGetSet->_attribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            _testGetSet->_expectedAttribute = 0;
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::ForegroundBlue:
            Log::Comment(L"Testing graphics 'Foreground Color Blue'");
            _testGetSet->_attribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            _testGetSet->_expectedAttribute = FOREGROUND_BLUE;
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::ForegroundGreen:
            Log::Comment(L"Testing graphics 'Foreground Color Green'");
            _testGetSet->_attribute = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            _testGetSet->_expectedAttribute = FOREGROUND_GREEN;
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::ForegroundCyan:
            Log::Comment(L"Testing graphics 'Foreground Color Cyan'");
            _testGetSet->_attribute = FOREGROUND_RED | FOREGROUND_INTENSITY;
            _testGetSet->_expectedAttribute = FOREGROUND_BLUE | FOREGROUND_GREEN;
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::ForegroundRed:
            Log::Comment(L"Testing graphics 'Foreground Color Red'");
            _testGetSet->_attribute = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            _testGetSet->_expectedAttribute = FOREGROUND_RED;
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::ForegroundMagenta:
            Log::Comment(L"Testing graphics 'Foreground Color Magenta'");
            _testGetSet->_attribute = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            _testGetSet->_expectedAttribute = FOREGROUND_BLUE | FOREGROUND_RED;
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::ForegroundYellow:
            Log::Comment(L"Testing graphics 'Foreground Color Yellow'");
            _testGetSet->_attribute = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            _testGetSet->_expectedAttribute = FOREGROUND_GREEN | FOREGROUND_RED;
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::ForegroundWhite:
            Log::Comment(L"Testing graphics 'Foreground Color White'");
            _testGetSet->_attribute = FOREGROUND_INTENSITY;
            _testGetSet->_expectedAttribute = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::ForegroundDefault:
            Log::Comment(L"Testing graphics 'Foreground Color Default'");
            _testGetSet->_privateSetDefaultAttributesResult = true;
            _testGetSet->_attribute = (WORD)~_testGetSet->s_wDefaultAttribute; // set the current attribute to the opposite of default so we can ensure all relevant bits flip.
            // To get expected value, take what we started with and change ONLY the background series of bits to what the Default says.
            _testGetSet->_expectedAttribute = _testGetSet->_attribute; // expect = starting
            _testGetSet->_expectedAttribute &= ~(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY); // turn off all bits related to the background
            _testGetSet->_expectedAttribute |= (_testGetSet->s_defaultFill & (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY)); // reapply ONLY background bits from the default attribute.
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::BackgroundBlack:
            Log::Comment(L"Testing graphics 'Background Color Black'");
            _testGetSet->_attribute = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY;
            _testGetSet->_expectedAttribute = 0;
            _testGetSet->_expectedBackground = true;
            break;
        case DispatchTypes::GraphicsOptions::BackgroundBlue:
            Log::Comment(L"Testing graphics 'Background Color Blue'");
            _testGetSet->_attribute = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_INTENSITY;
            _testGetSet->_expectedAttribute = BACKGROUND_BLUE;
            _testGetSet->_expectedBackground = true;
            break;
        case DispatchTypes::GraphicsOptions::BackgroundGreen:
            Log::Comment(L"Testing graphics 'Background Color Green'");
            _testGetSet->_attribute = BACKGROUND_RED | BACKGROUND_BLUE | BACKGROUND_INTENSITY;
            _testGetSet->_expectedAttribute = BACKGROUND_GREEN;
            _testGetSet->_expectedBackground = true;
            break;
        case DispatchTypes::GraphicsOptions::BackgroundCyan:
            Log::Comment(L"Testing graphics 'Background Color Cyan'");
            _testGetSet->_attribute = BACKGROUND_RED | BACKGROUND_INTENSITY;
            _testGetSet->_expectedAttribute = BACKGROUND_BLUE | BACKGROUND_GREEN;
            _testGetSet->_expectedBackground = true;
            break;
        case DispatchTypes::GraphicsOptions::BackgroundRed:
            Log::Comment(L"Testing graphics 'Background Color Red'");
            _testGetSet->_attribute = BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_INTENSITY;
            _testGetSet->_expectedAttribute = BACKGROUND_RED;
            _testGetSet->_expectedBackground = true;
            break;
        case DispatchTypes::GraphicsOptions::BackgroundMagenta:
            Log::Comment(L"Testing graphics 'Background Color Magenta'");
            _testGetSet->_attribute = BACKGROUND_GREEN | BACKGROUND_INTENSITY;
            _testGetSet->_expectedAttribute = BACKGROUND_BLUE | BACKGROUND_RED;
            _testGetSet->_expectedBackground = true;
            break;
        case DispatchTypes::GraphicsOptions::BackgroundYellow:
            Log::Comment(L"Testing graphics 'Background Color Yellow'");
            _testGetSet->_attribute = BACKGROUND_BLUE | BACKGROUND_INTENSITY;
            _testGetSet->_expectedAttribute = BACKGROUND_GREEN | BACKGROUND_RED;
            _testGetSet->_expectedBackground = true;
            break;
        case DispatchTypes::GraphicsOptions::BackgroundWhite:
            Log::Comment(L"Testing graphics 'Background Color White'");
            _testGetSet->_attribute = BACKGROUND_INTENSITY;
            _testGetSet->_expectedAttribute = BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED;
            _testGetSet->_expectedBackground = true;
            break;
        case DispatchTypes::GraphicsOptions::BackgroundDefault:
            Log::Comment(L"Testing graphics 'Background Color Default'");
            _testGetSet->_privateSetDefaultAttributesResult = true;
            _testGetSet->_attribute = (WORD)~_testGetSet->s_wDefaultAttribute; // set the current attribute to the opposite of default so we can ensure all relevant bits flip.
            // To get expected value, take what we started with and change ONLY the background series of bits to what the Default says.
            _testGetSet->_expectedAttribute = _testGetSet->_attribute; // expect = starting
            _testGetSet->_expectedAttribute &= ~(BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY); // turn off all bits related to the background
            _testGetSet->_expectedAttribute |= (_testGetSet->s_defaultFill & (BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY)); // reapply ONLY background bits from the default attribute.
            _testGetSet->_expectedBackground = true;
            break;
        case DispatchTypes::GraphicsOptions::BrightForegroundBlack:
            Log::Comment(L"Testing graphics 'Bright Foreground Color Black'");
            _testGetSet->_attribute = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            _testGetSet->_expectedAttribute = FOREGROUND_INTENSITY;
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::BrightForegroundBlue:
            Log::Comment(L"Testing graphics 'Bright Foreground Color Blue'");
            _testGetSet->_attribute = FOREGROUND_RED | FOREGROUND_GREEN;
            _testGetSet->_expectedAttribute = FOREGROUND_INTENSITY | FOREGROUND_BLUE;
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::BrightForegroundGreen:
            Log::Comment(L"Testing graphics 'Bright Foreground Color Green'");
            _testGetSet->_attribute = FOREGROUND_RED | FOREGROUND_BLUE;
            _testGetSet->_expectedAttribute = FOREGROUND_INTENSITY | FOREGROUND_GREEN;
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::BrightForegroundCyan:
            Log::Comment(L"Testing graphics 'Bright Foreground Color Cyan'");
            _testGetSet->_attribute = FOREGROUND_RED;
            _testGetSet->_expectedAttribute = FOREGROUND_INTENSITY | FOREGROUND_BLUE | FOREGROUND_GREEN;
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::BrightForegroundRed:
            Log::Comment(L"Testing graphics 'Bright Foreground Color Red'");
            _testGetSet->_attribute = FOREGROUND_BLUE | FOREGROUND_GREEN;
            _testGetSet->_expectedAttribute = FOREGROUND_INTENSITY | FOREGROUND_RED;
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::BrightForegroundMagenta:
            Log::Comment(L"Testing graphics 'Bright Foreground Color Magenta'");
            _testGetSet->_attribute = FOREGROUND_GREEN;
            _testGetSet->_expectedAttribute = FOREGROUND_INTENSITY | FOREGROUND_BLUE | FOREGROUND_RED;
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::BrightForegroundYellow:
            Log::Comment(L"Testing graphics 'Bright Foreground Color Yellow'");
            _testGetSet->_attribute = FOREGROUND_BLUE;
            _testGetSet->_expectedAttribute = FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_RED;
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::BrightForegroundWhite:
            Log::Comment(L"Testing graphics 'Bright Foreground Color White'");
            _testGetSet->_attribute = 0;
            _testGetSet->_expectedAttribute = FOREGROUND_INTENSITY | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
            _testGetSet->_expectedForeground = true;
            break;
        case DispatchTypes::GraphicsOptions::BrightBackgroundBlack:
            Log::Comment(L"Testing graphics 'Bright Background Color Black'");
            _testGetSet->_attribute = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
            _testGetSet->_expectedAttribute = BACKGROUND_INTENSITY;
            _testGetSet->_expectedBackground = true;
            break;
        case DispatchTypes::GraphicsOptions::BrightBackgroundBlue:
            Log::Comment(L"Testing graphics 'Bright Background Color Blue'");
            _testGetSet->_attribute = BACKGROUND_RED | BACKGROUND_GREEN;
            _testGetSet->_expectedAttribute = BACKGROUND_INTENSITY | BACKGROUND_BLUE;
            _testGetSet->_expectedBackground = true;
            break;
        case DispatchTypes::GraphicsOptions::BrightBackgroundGreen:
            Log::Comment(L"Testing graphics 'Bright Background Color Green'");
            _testGetSet->_attribute = BACKGROUND_RED | BACKGROUND_BLUE;
            _testGetSet->_expectedAttribute = BACKGROUND_INTENSITY | BACKGROUND_GREEN;
            _testGetSet->_expectedBackground = true;
            break;
        case DispatchTypes::GraphicsOptions::BrightBackgroundCyan:
            Log::Comment(L"Testing graphics 'Bright Background Color Cyan'");
            _testGetSet->_attribute = BACKGROUND_RED;
            _testGetSet->_expectedAttribute = BACKGROUND_INTENSITY | BACKGROUND_BLUE | BACKGROUND_GREEN;
            _testGetSet->_expectedBackground = true;
            break;
        case DispatchTypes::GraphicsOptions::BrightBackgroundRed:
            Log::Comment(L"Testing graphics 'Bright Background Color Red'");
            _testGetSet->_attribute = BACKGROUND_BLUE | BACKGROUND_GREEN;
            _testGetSet->_expectedAttribute = BACKGROUND_INTENSITY | BACKGROUND_RED;
            _testGetSet->_expectedBackground = true;
            break;
        case DispatchTypes::GraphicsOptions::BrightBackgroundMagenta:
            Log::Comment(L"Testing graphics 'Bright Background Color Magenta'");
            _testGetSet->_attribute = BACKGROUND_GREEN;
            _testGetSet->_expectedAttribute = BACKGROUND_INTENSITY | BACKGROUND_BLUE | BACKGROUND_RED;
            _testGetSet->_expectedBackground = true;
            break;
        case DispatchTypes::GraphicsOptions::BrightBackgroundYellow:
            Log::Comment(L"Testing graphics 'Bright Background Color Yellow'");
            _testGetSet->_attribute = BACKGROUND_BLUE;
            _testGetSet->_expectedAttribute = BACKGROUND_INTENSITY | BACKGROUND_GREEN | BACKGROUND_RED;
            _testGetSet->_expectedBackground = true;
            break;
        case DispatchTypes::GraphicsOptions::BrightBackgroundWhite:
            Log::Comment(L"Testing graphics 'Bright Background Color White'");
            _testGetSet->_attribute = 0;
            _testGetSet->_expectedAttribute = BACKGROUND_INTENSITY | BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED;
            _testGetSet->_expectedBackground = true;
            break;
        default:
            VERIFY_FAIL(L"Test not implemented yet!");
            break;
        }

        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));
    }

    TEST_METHOD(GraphicsPersistBrightnessTests)
    {
        Log::Comment(L"Starting test...");

        _testGetSet->PrepData(); // default color from here is gray on black, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED

        _testGetSet->_privateSetLegacyAttributesResult = TRUE;

        DispatchTypes::GraphicsOptions rgOptions[16];
        size_t cOptions = 1;

        Log::Comment(L"Test 1: Basic brightness test");
        Log::Comment(L"Reseting graphics options");
        rgOptions[0] = DispatchTypes::GraphicsOptions::Off;
        _testGetSet->_privateSetDefaultAttributesResult = true;
        _testGetSet->_expectedAttribute = 0;
        _testGetSet->_expectedForeground = true;
        _testGetSet->_expectedBackground = true;
        _testGetSet->_expectedMeta = true;
        _testGetSet->_privateBoldTextResult = true;
        _testGetSet->_expectedIsBold = false;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));

        Log::Comment(L"Testing graphics 'Foreground Color Blue'");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundBlue;
        _testGetSet->_expectedAttribute = FOREGROUND_BLUE;
        _testGetSet->_expectedForeground = true;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));

        Log::Comment(L"Enabling brightness");
        rgOptions[0] = DispatchTypes::GraphicsOptions::BoldBright;
        _testGetSet->_expectedAttribute = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        _testGetSet->_expectedForeground = true;
        _testGetSet->_privateBoldTextResult = true;
        _testGetSet->_expectedIsBold = true;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));
        VERIFY_IS_TRUE(_testGetSet->_isBold);

        Log::Comment(L"Testing graphics 'Foreground Color Green, with brightness'");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundGreen;
        _testGetSet->_expectedAttribute = FOREGROUND_GREEN;
        _testGetSet->_expectedForeground = true;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));
        VERIFY_IS_TRUE(WI_IsFlagSet(_testGetSet->_attribute, FOREGROUND_GREEN));
        VERIFY_IS_TRUE(_testGetSet->_isBold);

        Log::Comment(L"Test 2: Disable brightness, use a bright color, next normal call remains not bright");
        Log::Comment(L"Reseting graphics options");
        rgOptions[0] = DispatchTypes::GraphicsOptions::Off;
        _testGetSet->_privateSetDefaultAttributesResult = true;
        _testGetSet->_expectedAttribute = 0;
        _testGetSet->_expectedForeground = true;
        _testGetSet->_expectedBackground = true;
        _testGetSet->_expectedMeta = true;
        _testGetSet->_privateBoldTextResult = true;
        _testGetSet->_expectedIsBold = false;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));
        VERIFY_IS_TRUE(WI_IsFlagClear(_testGetSet->_attribute, FOREGROUND_INTENSITY));
        VERIFY_IS_FALSE(_testGetSet->_isBold);

        Log::Comment(L"Testing graphics 'Foreground Color Bright Blue'");
        rgOptions[0] = DispatchTypes::GraphicsOptions::BrightForegroundBlue;
        _testGetSet->_expectedAttribute = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        _testGetSet->_expectedForeground = true;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));
        VERIFY_IS_FALSE(_testGetSet->_isBold);

        Log::Comment(L"Testing graphics 'Foreground Color Blue', brightness of 9x series doesn't persist");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundBlue;
        _testGetSet->_expectedAttribute = FOREGROUND_BLUE;
        _testGetSet->_expectedForeground = true;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));
        VERIFY_IS_FALSE(_testGetSet->_isBold);

        Log::Comment(L"Test 3: Enable brightness, use a bright color, brightness persists to next normal call");
        Log::Comment(L"Reseting graphics options");
        rgOptions[0] = DispatchTypes::GraphicsOptions::Off;
        _testGetSet->_privateSetDefaultAttributesResult = true;
        _testGetSet->_expectedAttribute = 0;
        _testGetSet->_expectedForeground = true;
        _testGetSet->_expectedBackground = true;
        _testGetSet->_expectedMeta = true;
        _testGetSet->_privateBoldTextResult = true;
        _testGetSet->_expectedIsBold = false;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));
        VERIFY_IS_FALSE(_testGetSet->_isBold);

        Log::Comment(L"Testing graphics 'Foreground Color Blue'");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundBlue;
        _testGetSet->_expectedAttribute = FOREGROUND_BLUE;
        _testGetSet->_expectedForeground = true;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));
        VERIFY_IS_FALSE(_testGetSet->_isBold);

        Log::Comment(L"Enabling brightness");
        rgOptions[0] = DispatchTypes::GraphicsOptions::BoldBright;
        _testGetSet->_privateBoldTextResult = true;
        _testGetSet->_expectedIsBold = true;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));
        VERIFY_IS_TRUE(_testGetSet->_isBold);

        Log::Comment(L"Testing graphics 'Foreground Color Bright Blue'");
        rgOptions[0] = DispatchTypes::GraphicsOptions::BrightForegroundBlue;
        _testGetSet->_expectedAttribute = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        _testGetSet->_expectedForeground = true;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));
        VERIFY_IS_TRUE(_testGetSet->_isBold);

        Log::Comment(L"Testing graphics 'Foreground Color Blue, with brightness', brightness of 9x series doesn't affect brightness");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundBlue;
        _testGetSet->_expectedAttribute = FOREGROUND_BLUE;
        _testGetSet->_expectedForeground = true;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));
        VERIFY_IS_TRUE(_testGetSet->_isBold);

        Log::Comment(L"Testing graphics 'Foreground Color Green, with brightness'");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundGreen;
        _testGetSet->_expectedAttribute = FOREGROUND_GREEN;
        _testGetSet->_expectedForeground = true;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));
        VERIFY_IS_TRUE(_testGetSet->_isBold);
    }

    TEST_METHOD(DeviceStatusReportTests)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Verify failure when using bad status.");
        _testGetSet->PrepData();
        VERIFY_IS_FALSE(_pDispatch.get()->DeviceStatusReport((DispatchTypes::AnsiStatusType)-1));
    }

    TEST_METHOD(DeviceStatus_CursorPositionReportTests)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Verify normal cursor response position.");
        _testGetSet->PrepData(CursorX::XCENTER, CursorY::YCENTER);

        // start with the cursor position in the buffer.
        COORD coordCursorExpected = _testGetSet->_cursorPos;

        // to get to VT, we have to adjust it to its position relative to the viewport top.
        coordCursorExpected.Y -= _testGetSet->_viewport.Top;

        // Then note that VT is 1,1 based for the top left, so add 1. (The rest of the console uses 0,0 for array index bases.)
        coordCursorExpected.X++;
        coordCursorExpected.Y++;

        VERIFY_IS_TRUE(_pDispatch.get()->DeviceStatusReport(DispatchTypes::AnsiStatusType::CPR_CursorPositionReport));

        wchar_t pwszBuffer[50];

        swprintf_s(pwszBuffer, ARRAYSIZE(pwszBuffer), L"\x1b[%d;%dR", coordCursorExpected.Y, coordCursorExpected.X);
        _testGetSet->ValidateInputEvent(pwszBuffer);
    }

    TEST_METHOD(DeviceAttributesTests)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Verify normal response.");
        _testGetSet->PrepData();
        VERIFY_IS_TRUE(_pDispatch.get()->DeviceAttributes());

        PCWSTR pwszExpectedResponse = L"\x1b[?1;0c";
        _testGetSet->ValidateInputEvent(pwszExpectedResponse);

        Log::Comment(L"Test 2: Verify failure when WriteConsoleInput doesn't work.");
        _testGetSet->PrepData();
        _testGetSet->_privatePrependConsoleInputResult = FALSE;

        VERIFY_IS_FALSE(_pDispatch.get()->DeviceAttributes());
    }

    TEST_METHOD(CursorKeysModeTest)
    {
        Log::Comment(L"Starting test...");

        // success cases
        // set numeric mode = true
        Log::Comment(L"Test 1: application mode = false");
        _testGetSet->_privateSetCursorKeysModeResult = TRUE;
        _testGetSet->_cursorKeysApplicationMode = false;

        VERIFY_IS_TRUE(_pDispatch.get()->SetCursorKeysMode(false));

        // set numeric mode = false
        Log::Comment(L"Test 2: application mode = true");
        _testGetSet->_privateSetCursorKeysModeResult = TRUE;
        _testGetSet->_cursorKeysApplicationMode = true;

        VERIFY_IS_TRUE(_pDispatch.get()->SetCursorKeysMode(true));
    }

    TEST_METHOD(KeypadModeTest)
    {
        Log::Comment(L"Starting test...");

        // success cases
        // set numeric mode = true
        Log::Comment(L"Test 1: application mode = false");
        _testGetSet->_privateSetKeypadModeResult = TRUE;
        _testGetSet->_keypadApplicationMode = false;

        VERIFY_IS_TRUE(_pDispatch.get()->SetKeypadMode(false));

        // set numeric mode = false
        Log::Comment(L"Test 2: application mode = true");
        _testGetSet->_privateSetKeypadModeResult = TRUE;
        _testGetSet->_keypadApplicationMode = true;

        VERIFY_IS_TRUE(_pDispatch.get()->SetKeypadMode(true));
    }

    TEST_METHOD(AllowBlinkingTest)
    {
        Log::Comment(L"Starting test...");

        // success cases
        // set numeric mode = true
        Log::Comment(L"Test 1: enable blinking = true");
        _testGetSet->_privateAllowCursorBlinkingResult = TRUE;
        _testGetSet->_enable = true;

        VERIFY_IS_TRUE(_pDispatch.get()->EnableCursorBlinking(true));

        // set numeric mode = false
        Log::Comment(L"Test 2: enable blinking = false");
        _testGetSet->_privateAllowCursorBlinkingResult = TRUE;
        _testGetSet->_enable = false;

        VERIFY_IS_TRUE(_pDispatch.get()->EnableCursorBlinking(false));
    }

    TEST_METHOD(ScrollMarginsTest)
    {
        Log::Comment(L"Starting test...");

        SMALL_RECT srTestMargins = { 0 };
        _testGetSet->_bufferSize = { 100, 600 };
        _testGetSet->_viewport.Right = 8;
        _testGetSet->_viewport.Bottom = 8;
        _testGetSet->_getConsoleScreenBufferInfoExResult = TRUE;
        SHORT sScreenHeight = _testGetSet->_viewport.Bottom - _testGetSet->_viewport.Top;

        Log::Comment(L"Test 1: Verify having both values is valid.");
        _testGetSet->_SetMarginsHelper(&srTestMargins, 2, 6);
        _testGetSet->_privateSetScrollingRegionResult = TRUE;
        _testGetSet->_setConsoleCursorPositionResult = true;
        _testGetSet->_moveToBottomResult = true;
        VERIFY_IS_TRUE(_pDispatch.get()->SetTopBottomScrollingMargins(srTestMargins.Top, srTestMargins.Bottom));

        Log::Comment(L"Test 2: Verify having only top is valid.");

        _testGetSet->_SetMarginsHelper(&srTestMargins, 7, 0);
        _testGetSet->_expectedScrollRegion.Bottom = _testGetSet->_viewport.Bottom - 1; // We expect the bottom to be the bottom of the viewport, exclusive.
        _testGetSet->_privateSetScrollingRegionResult = TRUE;
        VERIFY_IS_TRUE(_pDispatch.get()->SetTopBottomScrollingMargins(srTestMargins.Top, srTestMargins.Bottom));

        Log::Comment(L"Test 3: Verify having only bottom is valid.");

        _testGetSet->_SetMarginsHelper(&srTestMargins, 0, 7);
        _testGetSet->_privateSetScrollingRegionResult = TRUE;
        VERIFY_IS_TRUE(_pDispatch.get()->SetTopBottomScrollingMargins(srTestMargins.Top, srTestMargins.Bottom));

        Log::Comment(L"Test 4: Verify having no values is valid.");

        _testGetSet->_SetMarginsHelper(&srTestMargins, 0, 0);
        _testGetSet->_privateSetScrollingRegionResult = TRUE;
        VERIFY_IS_TRUE(_pDispatch.get()->SetTopBottomScrollingMargins(srTestMargins.Top, srTestMargins.Bottom));

        Log::Comment(L"Test 5: Verify having both values, but bad bounds is invalid.");

        _testGetSet->_SetMarginsHelper(&srTestMargins, 7, 3);
        _testGetSet->_privateSetScrollingRegionResult = TRUE;
        VERIFY_IS_FALSE(_pDispatch.get()->SetTopBottomScrollingMargins(srTestMargins.Top, srTestMargins.Bottom));

        Log::Comment(L"Test 6: Verify setting margins to (0, height) clears them");
        // First set,
        _testGetSet->_privateSetScrollingRegionResult = TRUE;
        _testGetSet->_SetMarginsHelper(&srTestMargins, 2, 6);
        VERIFY_IS_TRUE(_pDispatch.get()->SetTopBottomScrollingMargins(srTestMargins.Top, srTestMargins.Bottom));
        // Then clear
        _testGetSet->_SetMarginsHelper(&srTestMargins, 0, sScreenHeight);
        _testGetSet->_expectedScrollRegion.Top = 0;
        _testGetSet->_expectedScrollRegion.Bottom = 0;
        VERIFY_IS_TRUE(_pDispatch.get()->SetTopBottomScrollingMargins(srTestMargins.Top, srTestMargins.Bottom));

        Log::Comment(L"Test 7: Verify setting margins to (1, height) clears them");
        // First set,
        _testGetSet->_privateSetScrollingRegionResult = TRUE;
        _testGetSet->_SetMarginsHelper(&srTestMargins, 2, 6);
        VERIFY_IS_TRUE(_pDispatch.get()->SetTopBottomScrollingMargins(srTestMargins.Top, srTestMargins.Bottom));
        // Then clear
        _testGetSet->_SetMarginsHelper(&srTestMargins, 1, sScreenHeight);
        _testGetSet->_expectedScrollRegion.Top = 0;
        _testGetSet->_expectedScrollRegion.Bottom = 0;
        VERIFY_IS_TRUE(_pDispatch.get()->SetTopBottomScrollingMargins(srTestMargins.Top, srTestMargins.Bottom));

        Log::Comment(L"Test 8: Verify setting margins to (1, 0) clears them");
        // First set,
        _testGetSet->_privateSetScrollingRegionResult = TRUE;
        _testGetSet->_SetMarginsHelper(&srTestMargins, 2, 6);
        VERIFY_IS_TRUE(_pDispatch.get()->SetTopBottomScrollingMargins(srTestMargins.Top, srTestMargins.Bottom));
        // Then clear
        _testGetSet->_SetMarginsHelper(&srTestMargins, 1, 0);
        _testGetSet->_expectedScrollRegion.Top = 0;
        _testGetSet->_expectedScrollRegion.Bottom = 0;
        VERIFY_IS_TRUE(_pDispatch.get()->SetTopBottomScrollingMargins(srTestMargins.Top, srTestMargins.Bottom));

        Log::Comment(L"Test 9: Verify having top and bottom margin the same is invalid.");

        _testGetSet->_SetMarginsHelper(&srTestMargins, 4, 4);
        _testGetSet->_privateSetScrollingRegionResult = TRUE;
        VERIFY_IS_FALSE(_pDispatch.get()->SetTopBottomScrollingMargins(srTestMargins.Top, srTestMargins.Bottom));

        Log::Comment(L"Test 10: Verify having top margin out of bounds is invalid.");

        _testGetSet->_SetMarginsHelper(&srTestMargins, sScreenHeight + 1, sScreenHeight + 10);
        _testGetSet->_privateSetScrollingRegionResult = TRUE;
        VERIFY_IS_FALSE(_pDispatch.get()->SetTopBottomScrollingMargins(srTestMargins.Top, srTestMargins.Bottom));

        Log::Comment(L"Test 11: Verify having bottom margin out of bounds is invalid.");

        _testGetSet->_SetMarginsHelper(&srTestMargins, 1, sScreenHeight + 1);
        _testGetSet->_privateSetScrollingRegionResult = TRUE;
        VERIFY_IS_FALSE(_pDispatch.get()->SetTopBottomScrollingMargins(srTestMargins.Top, srTestMargins.Bottom));
    }

    TEST_METHOD(LineFeedTest)
    {
        Log::Comment(L"Starting test...");

        // All test cases need the LineFeed call to succeed.
        _testGetSet->_privateLineFeedResult = TRUE;

        Log::Comment(L"Test 1: Line feed without carriage return.");
        _testGetSet->_expectedLineFeedWithReturn = false;
        VERIFY_IS_TRUE(_pDispatch.get()->LineFeed(DispatchTypes::LineFeedType::WithoutReturn));

        Log::Comment(L"Test 2: Line feed with carriage return.");
        _testGetSet->_expectedLineFeedWithReturn = true;
        VERIFY_IS_TRUE(_pDispatch.get()->LineFeed(DispatchTypes::LineFeedType::WithReturn));

        Log::Comment(L"Test 3: Line feed depends on mode, and mode reset.");
        _testGetSet->_privateGetLineFeedModeResult = false;
        _testGetSet->_expectedLineFeedWithReturn = false;
        VERIFY_IS_TRUE(_pDispatch.get()->LineFeed(DispatchTypes::LineFeedType::DependsOnMode));

        Log::Comment(L"Test 4: Line feed depends on mode, and mode set.");
        _testGetSet->_privateGetLineFeedModeResult = true;
        _testGetSet->_expectedLineFeedWithReturn = true;
        VERIFY_IS_TRUE(_pDispatch.get()->LineFeed(DispatchTypes::LineFeedType::DependsOnMode));
    }

    TEST_METHOD(TabSetClearTests)
    {
        Log::Comment(L"Starting test...");

        _testGetSet->_privateHorizontalTabSetResult = TRUE;
        VERIFY_IS_TRUE(_pDispatch.get()->HorizontalTabSet());

        _testGetSet->_expectedNumTabs = 16;

        _testGetSet->_privateForwardTabResult = TRUE;
        VERIFY_IS_TRUE(_pDispatch.get()->ForwardTab(16));

        _testGetSet->_privateBackwardsTabResult = TRUE;
        VERIFY_IS_TRUE(_pDispatch.get()->BackwardsTab(16));

        _testGetSet->_privateTabClearResult = TRUE;
        _testGetSet->_expectedClearAll = true;
        VERIFY_IS_TRUE(_pDispatch.get()->TabClear(DispatchTypes::TabClearType::ClearAllColumns));

        _testGetSet->_expectedClearAll = false;
        VERIFY_IS_TRUE(_pDispatch.get()->TabClear(DispatchTypes::TabClearType::ClearCurrentColumn));
    }

    TEST_METHOD(SetConsoleTitleTest)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: set title to be non-null");
        _testGetSet->_setConsoleTitleWResult = TRUE;
        _testGetSet->_expectedWindowTitle = L"Foo bar";

        VERIFY_IS_TRUE(_pDispatch.get()->SetWindowTitle(_testGetSet->_expectedWindowTitle));

        Log::Comment(L"Test 2: set title to be null");
        _testGetSet->_setConsoleTitleWResult = FALSE;
        _testGetSet->_expectedWindowTitle = {};

        VERIFY_IS_TRUE(_pDispatch.get()->SetWindowTitle({}));
    }

    TEST_METHOD(TestMouseModes)
    {
        Log::Comment(L"Starting test...");

        Log::Comment(L"Test 1: Test Default Mouse Mode");
        _testGetSet->_expectedMouseEnabled = true;
        _testGetSet->_privateEnableVT200MouseModeResult = TRUE;
        VERIFY_IS_TRUE(_pDispatch.get()->EnableVT200MouseMode(true));
        _testGetSet->_expectedMouseEnabled = false;
        VERIFY_IS_TRUE(_pDispatch.get()->EnableVT200MouseMode(false));

        Log::Comment(L"Test 2: Test UTF-8 Extended Mouse Mode");
        _testGetSet->_expectedMouseEnabled = true;
        _testGetSet->_privateEnableUTF8ExtendedMouseModeResult = TRUE;
        VERIFY_IS_TRUE(_pDispatch.get()->EnableUTF8ExtendedMouseMode(true));
        _testGetSet->_expectedMouseEnabled = false;
        VERIFY_IS_TRUE(_pDispatch.get()->EnableUTF8ExtendedMouseMode(false));

        Log::Comment(L"Test 3: Test SGR Extended Mouse Mode");
        _testGetSet->_expectedMouseEnabled = true;
        _testGetSet->_privateEnableSGRExtendedMouseModeResult = TRUE;
        VERIFY_IS_TRUE(_pDispatch.get()->EnableSGRExtendedMouseMode(true));
        _testGetSet->_expectedMouseEnabled = false;
        VERIFY_IS_TRUE(_pDispatch.get()->EnableSGRExtendedMouseMode(false));

        Log::Comment(L"Test 4: Test Button-Event Mouse Mode");
        _testGetSet->_expectedMouseEnabled = true;
        _testGetSet->_privateEnableButtonEventMouseModeResult = TRUE;
        VERIFY_IS_TRUE(_pDispatch.get()->EnableButtonEventMouseMode(true));
        _testGetSet->_expectedMouseEnabled = false;
        VERIFY_IS_TRUE(_pDispatch.get()->EnableButtonEventMouseMode(false));

        Log::Comment(L"Test 5: Test Any-Event Mouse Mode");
        _testGetSet->_expectedMouseEnabled = true;
        _testGetSet->_privateEnableAnyEventMouseModeResult = TRUE;
        VERIFY_IS_TRUE(_pDispatch.get()->EnableAnyEventMouseMode(true));
        _testGetSet->_expectedMouseEnabled = false;
        VERIFY_IS_TRUE(_pDispatch.get()->EnableAnyEventMouseMode(false));

        Log::Comment(L"Test 6: Test Alt Scroll Mouse Mode");
        _testGetSet->_expectedAlternateScrollEnabled = true;
        _testGetSet->_privateEnableAlternateScrollResult = TRUE;
        VERIFY_IS_TRUE(_pDispatch.get()->EnableAlternateScroll(true));
        _testGetSet->_expectedAlternateScrollEnabled = false;
        VERIFY_IS_TRUE(_pDispatch.get()->EnableAlternateScroll(false));
    }

    TEST_METHOD(Xterm256ColorTest)
    {
        Log::Comment(L"Starting test...");

        _testGetSet->PrepData(); // default color from here is gray on black, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED

        DispatchTypes::GraphicsOptions rgOptions[16];
        size_t cOptions = 3;

        _testGetSet->_setConsoleXtermTextAttributeResult = true;

        Log::Comment(L"Test 1: Change Foreground");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::BlinkOrXterm256Index;
        rgOptions[2] = (DispatchTypes::GraphicsOptions)2; // Green
        _testGetSet->_expectedAttribute = FOREGROUND_GREEN;
        _testGetSet->_iExpectedXtermTableEntry = 2;
        _testGetSet->_expectedIsForeground = true;
        _testGetSet->_usingRgbColor = false;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));

        Log::Comment(L"Test 2: Change Background");
        rgOptions[0] = DispatchTypes::GraphicsOptions::BackgroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::BlinkOrXterm256Index;
        rgOptions[2] = (DispatchTypes::GraphicsOptions)9; // Bright Red
        _testGetSet->_expectedAttribute = FOREGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY;
        _testGetSet->_iExpectedXtermTableEntry = 9;
        _testGetSet->_expectedIsForeground = false;
        _testGetSet->_usingRgbColor = false;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));

        Log::Comment(L"Test 3: Change Foreground to RGB color");
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::BlinkOrXterm256Index;
        rgOptions[2] = (DispatchTypes::GraphicsOptions)42; // Arbitrary Color
        _testGetSet->_iExpectedXtermTableEntry = 42;
        _testGetSet->_expectedIsForeground = true;
        _testGetSet->_usingRgbColor = true;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));

        Log::Comment(L"Test 4: Change Background to RGB color");
        rgOptions[0] = DispatchTypes::GraphicsOptions::BackgroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::BlinkOrXterm256Index;
        rgOptions[2] = (DispatchTypes::GraphicsOptions)142; // Arbitrary Color
        _testGetSet->_iExpectedXtermTableEntry = 142;
        _testGetSet->_expectedIsForeground = false;
        _testGetSet->_usingRgbColor = true;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));

        Log::Comment(L"Test 5: Change Foreground to Legacy Attr while BG is RGB color");
        // Unfortunately this test isn't all that good, because the adapterTest adapter isn't smart enough
        //   to have its own color table and translate the pre-existing RGB BG into a legacy BG.
        // Fortunately, the ft_api:RgbColorTests IS smart enough to test that.
        rgOptions[0] = DispatchTypes::GraphicsOptions::ForegroundExtended;
        rgOptions[1] = DispatchTypes::GraphicsOptions::BlinkOrXterm256Index;
        rgOptions[2] = (DispatchTypes::GraphicsOptions)9; // Bright Red
        _testGetSet->_expectedAttribute = FOREGROUND_RED | FOREGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_INTENSITY;
        _testGetSet->_iExpectedXtermTableEntry = 9;
        _testGetSet->_expectedIsForeground = true;
        _testGetSet->_usingRgbColor = false;
        VERIFY_IS_TRUE(_pDispatch.get()->SetGraphicsRendition({ rgOptions, cOptions }));
    }

    TEST_METHOD(SetColorTableValue)
    {
        _testGetSet->PrepData();

        _testGetSet->_privateSetColorTableEntryResult = true;
        const auto testColor = RGB(1, 2, 3);
        _testGetSet->_expectedColorValue = testColor;

        _testGetSet->_expectedColorTableIndex = 0; // Windows DARK_BLACK
        VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(0, testColor));

        _testGetSet->_expectedColorTableIndex = 4; // Windows DARK_RED
        VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(1, testColor));

        _testGetSet->_expectedColorTableIndex = 2; // Windows DARK_GREEN
        VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(2, testColor));

        _testGetSet->_expectedColorTableIndex = 6; // Windows DARK_YELLOW
        VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(3, testColor));

        _testGetSet->_expectedColorTableIndex = 1; // Windows DARK_BLUE
        VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(4, testColor));

        _testGetSet->_expectedColorTableIndex = 5; // Windows DARK_MAGENTA
        VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(5, testColor));

        _testGetSet->_expectedColorTableIndex = 3; // Windows DARK_CYAN
        VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(6, testColor));

        _testGetSet->_expectedColorTableIndex = 7; // Windows DARK_WHITE
        VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(7, testColor));

        _testGetSet->_expectedColorTableIndex = 8; // Windows BRIGHT_BLACK
        VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(8, testColor));

        _testGetSet->_expectedColorTableIndex = 12; // Windows BRIGHT_RED
        VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(9, testColor));

        _testGetSet->_expectedColorTableIndex = 10; // Windows BRIGHT_GREEN
        VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(10, testColor));

        _testGetSet->_expectedColorTableIndex = 14; // Windows BRIGHT_YELLOW
        VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(11, testColor));

        _testGetSet->_expectedColorTableIndex = 9; // Windows BRIGHT_BLUE
        VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(12, testColor));

        _testGetSet->_expectedColorTableIndex = 13; // Windows BRIGHT_MAGENTA
        VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(13, testColor));

        _testGetSet->_expectedColorTableIndex = 11; // Windows BRIGHT_CYAN
        VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(14, testColor));

        _testGetSet->_expectedColorTableIndex = 15; // Windows BRIGHT_WHITE
        VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(15, testColor));

        for (short i = 16; i < 256; i++)
        {
            _testGetSet->_expectedColorTableIndex = i;
            VERIFY_IS_TRUE(_pDispatch.get()->SetColorTableEntry(i, testColor));
        }

        // Test in pty mode - we should fail, but PrivateSetColorTableEntry should still be called
        _testGetSet->_isPty = true;
        _testGetSet->_isConsolePtyResult = true;

        _testGetSet->_expectedColorTableIndex = 15; // Windows BRIGHT_WHITE
        VERIFY_IS_FALSE(_pDispatch.get()->SetColorTableEntry(15, testColor));
    }

private:
    TestGetSet* _testGetSet; // non-ownership pointer
    std::unique_ptr<AdaptDispatch> _pDispatch;
};

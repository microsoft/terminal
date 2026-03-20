/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- screenInfo.hpp

Abstract:
- This module represents the structures and functions required
  for rendering one screen of the console host window.

Author(s):
- Michael Niksa (MiNiksa) 10-Apr-2014
- Paul Campbell (PaulCam) 10-Apr-2014

Revision History:
- From components of output.h/.c and resize.c by Therese Stowell (ThereseS) 1990-1991
--*/

#pragma once

#include "outputStream.hpp"
#include "../buffer/out/OutputCellRect.hpp"
#include "../buffer/out/textBuffer.hpp"
#include "../interactivity/inc/IWindowMetrics.hpp"
#include "../renderer/inc/FontInfo.hpp"
#include "../renderer/inc/FontInfoDesired.hpp"
#include "../server/ObjectHeader.h"

class ConversionAreaInfo; // forward decl window. circular reference

class SCREEN_INFORMATION : public ConsoleObjectHeader, public Microsoft::Console::IIoProvider
{
public:
    // This little helper works like wil::scope_exit but is slimmer
    // (= easier to optimize) and has a concrete type (= can declare).
    struct SnapOnScopeExit
    {
        ~SnapOnScopeExit()
        {
            if (self)
            {
                try
                {
                    self->_makeCursorVisible();
                }
                CATCH_LOG();
            }
        }

        SCREEN_INFORMATION* self;
    };

    struct ScrollBarState
    {
        til::size maxSize;
        til::rect viewport;
        bool isAltBuffer = false;
    };

    ~SCREEN_INFORMATION() override;

#pragma region IIoProvider
    SCREEN_INFORMATION& GetActiveOutputBuffer() override;
    const SCREEN_INFORMATION& GetActiveOutputBuffer() const override;
    InputBuffer* const GetActiveInputBuffer() const override;
#pragma endregion

    // NOTE: If your method has 200 chars worth of parameters, and it's hard to read
    // without wrapping, chance is you're doing it wrong. This is also true here.
    // Should have been a default constructor + setters / builder pattern.
    // GetScreenBufferInformation could return a struct. And so on.

    // Creation
    [[nodiscard]] static NTSTATUS CreateInstance(til::size windowSize, FontInfoDesired fontInfoDesired, FontInfo fontInfo, til::size screenBufferSize, TextAttribute defaultAttributes, TextAttribute popupAttributes, UINT cursorSize, SCREEN_INFORMATION** screen);
    static void s_InsertScreenBuffer(SCREEN_INFORMATION* screenInfo);
    static void s_RemoveScreenBuffer(SCREEN_INFORMATION* screenInfo);

    // Buffer
    TextBuffer& GetTextBuffer() noexcept;
    const TextBuffer& GetTextBuffer() const noexcept;
    bool IsActiveScreenBuffer() const;
    [[nodiscard]] NTSTATUS ResizeScreenBuffer(til::size newScreenSize, bool doScrollBarUpdate);
    [[nodiscard]] NTSTATUS ResizeWithReflow(til::size newScreenSize);
    [[nodiscard]] NTSTATUS ResizeTraditional(til::size newScreenSize);
    [[nodiscard]] NTSTATUS UseAlternateScreenBuffer(const TextAttribute& initAttributes);
    void UseMainScreenBuffer();
    SCREEN_INFORMATION& GetMainBuffer();
    const SCREEN_INFORMATION& GetMainBuffer() const;
    const SCREEN_INFORMATION* GetAltBuffer() const noexcept;
    SCREEN_INFORMATION& GetActiveBuffer();
    const SCREEN_INFORMATION& GetActiveBuffer() const;
    const TextAttribute& GetAttributes() const noexcept;
    const TextAttribute& GetPopupAttributes() const noexcept;
    void SetAttributes(const TextAttribute& attributes);
    void SetPopupAttributes(const TextAttribute& popupAttributes);
    void SetDefaultAttributes(const TextAttribute& attributes, const TextAttribute& popupAttributes);
    void ProcessResizeWindow(const til::rect* clientNew, const til::rect* clientOld);

    // Cursor
    [[nodiscard]] NTSTATUS SetCursorPosition(til::point Position);
    void MakeCurrentCursorVisible();
    void MakeCursorVisible(til::point position);
    void SnapOnInput(WORD vkey);
    SnapOnScopeExit SnapOnOutput() noexcept;
    void SetCursorInformation(ULONG size, bool visible) noexcept;
    void SetCursorType(CursorType type, bool setMain = false) noexcept;
    void SetCursorDBMode(bool doubleCursor);

    // I/O
    const Microsoft::Console::VirtualTerminal::StateMachine& GetStateMachine() const;
    Microsoft::Console::VirtualTerminal::StateMachine& GetStateMachine();
    TextBufferCellIterator GetCellDataAt(til::point at) const;
    TextBufferCellIterator GetCellLineDataAt(til::point at) const;
    TextBufferCellIterator GetCellDataAt(til::point at, Microsoft::Console::Types::Viewport limit) const;
    TextBufferTextIterator GetTextDataAt(til::point at) const;
    TextBufferTextIterator GetTextLineDataAt(til::point at) const;
    TextBufferTextIterator GetTextDataAt(til::point at, Microsoft::Console::Types::Viewport limit) const;
    OutputCellIterator Write(OutputCellIterator it);
    OutputCellIterator Write(OutputCellIterator it, til::point target, std::optional<bool> wrap = true);
    OutputCellIterator WriteRect(OutputCellIterator it, Microsoft::Console::Types::Viewport viewport);
    void WriteRect(const OutputCellRect& data, til::point location);
    void ClearTextData();

    // Rendering / Viewport
    FontInfo& GetCurrentFont() noexcept;
    const FontInfo& GetCurrentFont() const noexcept;
    FontInfoDesired& GetDesiredFont() noexcept;
    const FontInfoDesired& GetDesiredFont() const noexcept;
    til::size GetLegacyConhostFontCellSize() const noexcept;
    til::size GetScreenFontSize() const;
    void UpdateFont(FontInfoDesired newFont);
    void RefreshFontWithRenderer();
    [[nodiscard]] NTSTATUS SetViewportOrigin(bool absolute, til::point coordWindowOrigin, bool updateBottom);
    const Microsoft::Console::Types::Viewport& GetViewport() const noexcept;
    void SetViewport(const Microsoft::Console::Types::Viewport& newViewport, bool updateBottom);
    void SetViewportSize(const til::size* size);
    void UpdateBottom();
    Microsoft::Console::Types::Viewport GetVirtualViewport() const noexcept;
    Microsoft::Console::Types::Viewport GetVtPageArea() const noexcept;

    // Windowing
    til::size GetScrollBarSizesInCharacters() const;
    void UpdateScrollBars();
    ScrollBarState FetchScrollBarState();
    bool IsMaximizedBoth() const;
    bool IsMaximizedX() const;
    bool IsMaximizedY() const;
    bool PostUpdateWindowSize() const;

    // General Information
    void GetScreenBufferInformation(til::size* size, til::point* cursorPosition, til::inclusive_rect* window, PWORD attributes, til::size* maximumWindowSize, PWORD popupAttributes, LPCOLORREF colorTable) const;
    void GetRequiredConsoleSizeInPixels(til::size* requiredSize) const;
    til::size GetMinWindowSizeInCharacters(til::size fontSize = { 1, 1 }) const;
    til::size GetMaxWindowSizeInCharacters(til::size fontSize = { 1, 1 }) const;
    til::size GetLargestWindowSizeInCharacters(til::size fontSize = { 1, 1 }) const;

    // Helpers
    void ClipToScreenBuffer(til::inclusive_rect* clip) const;
    std::pair<til::point, til::point> GetWordBoundary(til::point position) const;
    Microsoft::Console::Types::Viewport GetBufferSize() const;
    Microsoft::Console::Types::Viewport GetTerminalBufferSize() const;
    bool SendNotifyBeep() const;
    bool ConptyCursorPositionMayBeWrong() const noexcept;
    void SetConptyCursorPositionMayBeWrong() noexcept;
    void ResetConptyCursorPositionMayBeWrong() noexcept;
    void WaitForConptyCursorPositionToBeSynchronized() noexcept;

    DWORD OutputMode = ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT;
    short WheelDelta = 0;
    short HWheelDelta = 0;
    SCREEN_INFORMATION* Next = nullptr;
    BYTE WriteConsoleDbcsLeadByte[2] = { 0, 0 };
    BYTE FillOutDbcsLeadChar = 0;
    UINT ScrollScale = 1;

private:
    SCREEN_INFORMATION(Microsoft::Console::Interactivity::IWindowMetrics* metrics, TextAttribute popupAttributes, FontInfoDesired fontInfoDesired, FontInfo fontInfo);

    // Construction
    [[nodiscard]] NTSTATUS _InitializeOutputStateMachine();
    void _FreeOutputStateMachine();

    // Buffer
    [[nodiscard]] HRESULT _AdjustScreenBufferHelper(const til::rect* clientNew, til::size bufferOld, til::size* clientNewCharacters);
    [[nodiscard]] HRESULT _AdjustScreenBuffer(const til::rect* clientNew);
    [[nodiscard]] NTSTATUS _CreateAltBuffer(const TextAttribute& initAttributes, SCREEN_INFORMATION** newScreenBuffer);
    bool _IsAltBuffer() const;
    bool _IsInPtyMode() const;
    bool _IsInVTMode() const;

    // Cursor
    void _makeCursorVisible();

    // Rendering / Viewport
    void _updateFont(FontInfoDesired newFont);
    void _CalculateViewportSize(const til::rect* clientArea, til::size* size);
    void _AdjustViewportSize(const til::rect* clientNew, const til::rect* clientOld, const til::size* size);
    void _InternalSetViewportSize(const til::size* size, bool resizeFromTop, bool resizeFromLeft);

    // Windowing
    static void s_CalculateScrollbarVisibility(const til::rect* clientArea, const til::size* bufferSize, const til::size* fontSize, bool* horizontalVisible, bool* verticalVisible);
    static void _handleDeferredResize(SCREEN_INFORMATION& siMain);

    Microsoft::Console::Interactivity::IWindowMetrics* _pConsoleWindowMetrics;
    std::unique_ptr<TextBuffer> _textBuffer{ nullptr };
    ConhostInternalGetSet _api{ *this };
    std::shared_ptr<Microsoft::Console::VirtualTerminal::StateMachine> _stateMachine;
    // Specifies which coordinates of the screen buffer are visible in the
    //      window client (the "viewport" into the buffer)
    Microsoft::Console::Types::Viewport _viewport;
    SCREEN_INFORMATION* _psiAlternateBuffer = nullptr; // The VT "Alternate" screen buffer.
    SCREEN_INFORMATION* _psiMainBuffer = nullptr; // A pointer to the main buffer, if this is the alternate buffer.
    til::rect _rcAltSavedClientNew;
    til::rect _rcAltSavedClientOld;
    bool _fAltWindowChanged = false;
    TextAttribute _PopupAttributes;
    FontInfo _currentFont;
    FontInfoDesired _desiredFont;
    // Tracks the last virtual position the viewport was at. This is not
    //  affected by the user scrolling the viewport, only when API calls cause
    //  the viewport to move (SetBufferInfo, WriteConsole, etc)
    til::CoordType _virtualBottom = 0;
    std::optional<til::size> _deferredPtyResize;
    std::atomic<bool> _conptyCursorPositionMayBeWrong = false;

#ifdef UNIT_TESTING
    friend class TextBufferIteratorTests;
    friend class ScreenBufferTests;
    friend class CommonState;
#endif
};

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
#include "../renderer/inc/FontInfoDesired.hpp"
#include "../server/ObjectHeader.h"

#include <wil/resource.h>

class ConversionAreaInfo; // forward decl window. circular reference

// Backs a CONSOLE_GRAPHICS_BUFFER screen buffer: a shared, pagefile-backed
// section holding a raw DIB (top-down, BI_RGB), mapped once into conhost's own
// process for rendering (Bits()) and once directly into the requesting client's
// process (ClientBits()) so the client can blit pixels with zero IPC per frame.
// A Mutant is duplicated into the client alongside it so both sides can
// synchronize access using only kernel objects, no round-trips through the
// console driver. This mirrors the mechanism NTVDM originally used for
// full-screen DOS graphics mode, generalized so any client can request one.
class GraphicsBuffer
{
public:
    GraphicsBuffer(wil::unique_handle hSection,
                   wil::unique_handle hClientProcess,
                   wil::unique_handle hMutex,
                   HANDLE hClientMutex,
                   PVOID bitMap,
                   PVOID clientBitMap,
                   size_t bitmapSize,
                   std::vector<BYTE> bitmapInfoStorage,
                   ULONG dibUsage) noexcept;
    ~GraphicsBuffer();

    GraphicsBuffer(const GraphicsBuffer&) = delete;
    GraphicsBuffer& operator=(const GraphicsBuffer&) = delete;

    const BITMAPINFO* BitmapInfo() const noexcept;
    PVOID Bits() const noexcept;
    PVOID ClientBits() const noexcept;
    HANDLE ClientMutex() const noexcept;
    size_t BitmapSize() const noexcept;

    // DIB_RGB_COLORS or DIB_PAL_COLORS, from the client's original create call
    // (CONSOLE_CREATESCREENBUFFER_MSG.Usage) - tells a render engine how to
    // interpret BitmapInfo()'s color table, and which iUsage to pass a GDI
    // blit function.
    ULONG DibUsage() const noexcept;

    // Set via ConsolepSetPalette (ApiDispatchers::ServerSetConsolePalette).
    // Stored as received - the client owns its lifetime, we never delete it.
    // The client is responsible for making the handle usable across
    // processes before sending it (GDI's CreatePalette() returns a handle
    // private to the creating process by default).
    void SetPalette(HPALETTE hPalette) noexcept;
    HPALETTE Palette() const noexcept;

private:
    wil::unique_handle _hSection;
    wil::unique_handle _hClientProcess;
    wil::unique_handle _hMutex;
    HANDLE _hClientMutex; // Owned by the client process; not ours to close.
    PVOID _bitMap; // Mapped into conhost's own process.
    PVOID _clientBitMap; // Mapped into the client's process.
    size_t _bitmapSize;
    std::vector<BYTE> _bitmapInfoStorage; // Holds a BITMAPINFOHEADER (+ optional color table).
    ULONG _dibUsage;
    HPALETTE _hPalette = nullptr; // Not owned - see SetPalette.
};

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
    [[nodiscard]] static NTSTATUS CreateInstance(til::size windowSize, FontInfo fontInfo, til::size screenBufferSize, TextAttribute defaultAttributes, TextAttribute popupAttributes, UINT cursorSize, SCREEN_INFORMATION** screen);
    static void s_InsertScreenBuffer(SCREEN_INFORMATION* screenInfo);
    static void s_RemoveScreenBuffer(SCREEN_INFORMATION* screenInfo);

    // Graphics (CONSOLE_GRAPHICS_BUFFER) buffers
    // NOTE: the buffer is still created via CreateInstance() with the desired
    // pixel dimensions passed as the window/screen buffer size - this method
    // only attaches the pixel storage on top of the resulting instance.
    void AttachGraphicsBuffer(std::unique_ptr<GraphicsBuffer> graphicsBuffer) noexcept;
    bool IsGraphicsBuffer() const noexcept;
    GraphicsBuffer* GetGraphicsBuffer() noexcept;
    const GraphicsBuffer* GetGraphicsBuffer() const noexcept;

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
    til::size GetScreenFontSize() const;
    void UpdateFont(const FontInfo* newFont);
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
    SCREEN_INFORMATION(Microsoft::Console::Interactivity::IWindowMetrics* metrics, TextAttribute popupAttributes, FontInfo fontInfo);

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
    void _CalculateViewportSize(const til::rect* clientArea, til::size* size);
    void _AdjustViewportSize(const til::rect* clientNew, const til::rect* clientOld, const til::size* size);
    void _InternalSetViewportSize(const til::size* size, bool resizeFromTop, bool resizeFromLeft);

    // Windowing
    static void s_CalculateScrollbarVisibility(const til::rect* clientArea, const til::size* bufferSize, const til::size* fontSize, bool* horizontalVisible, bool* verticalVisible);
    static void _handleDeferredResize(SCREEN_INFORMATION& siMain);

    Microsoft::Console::Interactivity::IWindowMetrics* _pConsoleWindowMetrics;
    std::unique_ptr<TextBuffer> _textBuffer{ nullptr };
    // Non-null only for a CONSOLE_GRAPHICS_BUFFER instance. The underlying
    // _textBuffer above is still allocated (sized to match the pixel
    // dimensions) so the rest of this class's invariants keep holding; it is
    // simply not what gets rendered for this buffer. See AttachGraphicsBuffer.
    std::unique_ptr<GraphicsBuffer> _graphicsBuffer{ nullptr };
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
    // The LSB indicates whether the cursor position may be wrong. 0 = correct, 1 = may be wrong.
    // The other 31 bit are a generation count to avoid TOCTOU issues in WaitForConptyCursorPositionToBeSynchronized.
    std::atomic<uint32_t> _conptyCursorPositionGeneration{ 0 };

#ifdef UNIT_TESTING
    friend class TextBufferIteratorTests;
    friend class ScreenBufferTests;
    friend class CommonState;
#endif
};

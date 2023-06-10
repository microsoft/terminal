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

#include "conapi.h"
#include "settings.hpp"
#include "outputStream.hpp"

#include "../buffer/out/OutputCellRect.hpp"
#include "../buffer/out/TextAttribute.hpp"
#include "../buffer/out/textBuffer.hpp"
#include "../buffer/out/textBufferCellIterator.hpp"
#include "../buffer/out/textBufferTextIterator.hpp"

#include "IIoProvider.hpp"
#include "outputStream.hpp"
#include "../terminal/adapter/adaptDispatch.hpp"
#include "../terminal/parser/stateMachine.hpp"
#include "../terminal/parser/OutputStateMachineEngine.hpp"

#include "../server/ObjectHeader.h"

#include "../interactivity/inc/IAccessibilityNotifier.hpp"
#include "../interactivity/inc/IConsoleWindow.hpp"
#include "../interactivity/inc/IWindowMetrics.hpp"

#include "../renderer/inc/FontInfo.hpp"
#include "../renderer/inc/FontInfoDesired.hpp"

#include "../types/inc/Viewport.hpp"
class ConversionAreaInfo; // forward decl window. circular reference

// fwdecl unittest classes
#ifdef UNIT_TESTING
namespace TerminalCoreUnitTests
{
    class ConptyRoundtripTests;
};
#endif

class SCREEN_INFORMATION : public ConsoleObjectHeader, public Microsoft::Console::IIoProvider
{
public:
    [[nodiscard]] static NTSTATUS CreateInstance(_In_ til::size coordWindowSize,
                                                 const FontInfo fontInfo,
                                                 _In_ til::size coordScreenBufferSize,
                                                 const TextAttribute defaultAttributes,
                                                 const TextAttribute popupAttributes,
                                                 const UINT uiCursorSize,
                                                 _Outptr_ SCREEN_INFORMATION** const ppScreen);

    ~SCREEN_INFORMATION();

    void GetScreenBufferInformation(_Out_ til::size* pcoordSize,
                                    _Out_ til::point* pcoordCursorPosition,
                                    _Out_ til::inclusive_rect* psrWindow,
                                    _Out_ PWORD pwAttributes,
                                    _Out_ til::size* pcoordMaximumWindowSize,
                                    _Out_ PWORD pwPopupAttributes,
                                    _Out_writes_(COLOR_TABLE_SIZE) LPCOLORREF lpColorTable) const;

    void GetRequiredConsoleSizeInPixels(_Out_ til::size* const pRequiredSize) const;

    void MakeCurrentCursorVisible();

    void ClipToScreenBuffer(_Inout_ til::inclusive_rect* const psrClip) const;

    til::size GetMinWindowSizeInCharacters(const til::size coordFontSize = { 1, 1 }) const;
    til::size GetMaxWindowSizeInCharacters(const til::size coordFontSize = { 1, 1 }) const;
    til::size GetLargestWindowSizeInCharacters(const til::size coordFontSize = { 1, 1 }) const;
    til::size GetScrollBarSizesInCharacters() const;

    Microsoft::Console::Types::Viewport GetBufferSize() const;
    Microsoft::Console::Types::Viewport GetTerminalBufferSize() const;

    til::size GetScreenFontSize() const;
    void UpdateFont(const FontInfo* const pfiNewFont);
    void RefreshFontWithRenderer();

    [[nodiscard]] NTSTATUS ResizeScreenBuffer(const til::size coordNewScreenSize, const bool fDoScrollBarUpdate);

    bool HasAccessibilityEventing() const noexcept;
    void NotifyAccessibilityEventing(const til::CoordType sStartX, const til::CoordType sStartY, const til::CoordType sEndX, const til::CoordType sEndY);

    void UpdateScrollBars();
    void InternalUpdateScrollBars();

    bool IsMaximizedBoth() const;
    bool IsMaximizedX() const;
    bool IsMaximizedY() const;

    const Microsoft::Console::Types::Viewport& GetViewport() const noexcept;
    void SetViewport(const Microsoft::Console::Types::Viewport& newViewport, const bool updateBottom);
    Microsoft::Console::Types::Viewport GetVirtualViewport() const noexcept;

    void ProcessResizeWindow(const til::rect* const prcClientNew, const til::rect* const prcClientOld);
    void SetViewportSize(const til::size* const pcoordSize);

    // Forwarders to Window if we're the active buffer.
    [[nodiscard]] NTSTATUS SetViewportOrigin(const bool fAbsolute, const til::point coordWindowOrigin, const bool updateBottom);

    bool SendNotifyBeep() const;
    bool PostUpdateWindowSize() const;

    // TODO: MSFT 9355062 these methods should probably be a part of construction/destruction. http://osgvsowi/9355062
    static void s_InsertScreenBuffer(_In_ SCREEN_INFORMATION* const pScreenInfo);
    static void s_RemoveScreenBuffer(_In_ SCREEN_INFORMATION* const pScreenInfo);

    OutputCellRect ReadRect(const Microsoft::Console::Types::Viewport location) const;

    TextBufferCellIterator GetCellDataAt(const til::point at) const;
    TextBufferCellIterator GetCellLineDataAt(const til::point at) const;
    TextBufferCellIterator GetCellDataAt(const til::point at, const Microsoft::Console::Types::Viewport limit) const;
    TextBufferTextIterator GetTextDataAt(const til::point at) const;
    TextBufferTextIterator GetTextLineDataAt(const til::point at) const;
    TextBufferTextIterator GetTextDataAt(const til::point at, const Microsoft::Console::Types::Viewport limit) const;

    OutputCellIterator Write(const OutputCellIterator it);

    OutputCellIterator Write(const OutputCellIterator it,
                             const til::point target,
                             const std::optional<bool> wrap = true);

    OutputCellIterator WriteRect(const OutputCellIterator it,
                                 const Microsoft::Console::Types::Viewport viewport);

    void WriteRect(const OutputCellRect& data,
                   const til::point location);

    void ClearTextData();

    std::pair<til::point, til::point> GetWordBoundary(const til::point position) const;

    TextBuffer& GetTextBuffer() noexcept;
    const TextBuffer& GetTextBuffer() const noexcept;

#pragma region IIoProvider
    SCREEN_INFORMATION& GetActiveOutputBuffer() override;
    const SCREEN_INFORMATION& GetActiveOutputBuffer() const override;
    InputBuffer* const GetActiveInputBuffer() const override;
#pragma endregion

    bool CursorIsDoubleWidth() const;

    DWORD OutputMode;
    WORD ResizingWindow; // > 0 if we should ignore WM_SIZE messages

    short WheelDelta;
    short HWheelDelta;

private:
    std::unique_ptr<TextBuffer> _textBuffer;

public:
    SCREEN_INFORMATION* Next;
    BYTE WriteConsoleDbcsLeadByte[2];
    BYTE FillOutDbcsLeadChar;

    // non ownership pointer
    ConversionAreaInfo* ConvScreenInfo;

    UINT ScrollScale;

    bool IsActiveScreenBuffer() const;

    const Microsoft::Console::VirtualTerminal::StateMachine& GetStateMachine() const;
    Microsoft::Console::VirtualTerminal::StateMachine& GetStateMachine();

    void SetCursorInformation(const ULONG Size,
                              const bool Visible) noexcept;

    void SetCursorType(const CursorType Type, const bool setMain = false) noexcept;

    void SetCursorDBMode(const bool DoubleCursor);
    [[nodiscard]] NTSTATUS SetCursorPosition(const til::point Position, const bool TurnOn);

    void MakeCursorVisible(const til::point CursorPosition);

    [[nodiscard]] NTSTATUS UseAlternateScreenBuffer(const TextAttribute& initAttributes);
    void UseMainScreenBuffer();

    SCREEN_INFORMATION& GetMainBuffer();
    const SCREEN_INFORMATION& GetMainBuffer() const;

    SCREEN_INFORMATION& GetActiveBuffer();
    const SCREEN_INFORMATION& GetActiveBuffer() const;

    TextAttribute GetAttributes() const;
    TextAttribute GetPopupAttributes() const;

    void SetAttributes(const TextAttribute& attributes);
    void SetPopupAttributes(const TextAttribute& popupAttributes);
    void SetDefaultAttributes(const TextAttribute& attributes,
                              const TextAttribute& popupAttributes);

    [[nodiscard]] HRESULT ClearBuffer();

    void SetTerminalConnection(_In_ Microsoft::Console::Render::VtEngine* const pTtyConnection);

    void UpdateBottom();

    FontInfo& GetCurrentFont() noexcept;
    const FontInfo& GetCurrentFont() const noexcept;

    FontInfoDesired& GetDesiredFont() noexcept;
    const FontInfoDesired& GetDesiredFont() const noexcept;

    void SetIgnoreLegacyEquivalentVTAttributes() noexcept;
    void ResetIgnoreLegacyEquivalentVTAttributes() noexcept;

private:
    SCREEN_INFORMATION(_In_ Microsoft::Console::Interactivity::IWindowMetrics* pMetrics,
                       _In_ Microsoft::Console::Interactivity::IAccessibilityNotifier* pNotifier,
                       const TextAttribute popupAttributes,
                       const FontInfo fontInfo);

    Microsoft::Console::Interactivity::IWindowMetrics* _pConsoleWindowMetrics;
    Microsoft::Console::Interactivity::IAccessibilityNotifier* _pAccessibilityNotifier;

    [[nodiscard]] HRESULT _AdjustScreenBufferHelper(const til::rect* const prcClientNew,
                                                    const til::size coordBufferOld,
                                                    _Out_ til::size* const pcoordClientNewCharacters);
    [[nodiscard]] HRESULT _AdjustScreenBuffer(const til::rect* const prcClientNew);
    void _CalculateViewportSize(const til::rect* const prcClientArea, _Out_ til::size* const pcoordSize);
    void _AdjustViewportSize(const til::rect* const prcClientNew, const til::rect* const prcClientOld, const til::size* const pcoordSize);
    void _InternalSetViewportSize(const til::size* pcoordSize, const bool fResizeFromTop, const bool fResizeFromLeft);

    static void s_CalculateScrollbarVisibility(const til::rect* const prcClientArea,
                                               const til::size* const pcoordBufferSize,
                                               const til::size* const pcoordFontSize,
                                               _Out_ bool* const pfIsHorizontalVisible,
                                               _Out_ bool* const pfIsVerticalVisible);

    [[nodiscard]] NTSTATUS ResizeWithReflow(const til::size coordnewScreenSize);
    [[nodiscard]] NTSTATUS ResizeTraditional(const til::size coordNewScreenSize);

    [[nodiscard]] NTSTATUS _InitializeOutputStateMachine();
    void _FreeOutputStateMachine();

    [[nodiscard]] NTSTATUS _CreateAltBuffer(const TextAttribute& initAttributes,
                                            _Out_ SCREEN_INFORMATION** const ppsiNewScreenBuffer);

    bool _IsAltBuffer() const;
    bool _IsInPtyMode() const;
    bool _IsInVTMode() const;

    ConhostInternalGetSet _api;

    std::shared_ptr<Microsoft::Console::VirtualTerminal::StateMachine> _stateMachine;

    // Specifies which coordinates of the screen buffer are visible in the
    //      window client (the "viewport" into the buffer)
    Microsoft::Console::Types::Viewport _viewport;

    SCREEN_INFORMATION* _psiAlternateBuffer; // The VT "Alternate" screen buffer.
    SCREEN_INFORMATION* _psiMainBuffer; // A pointer to the main buffer, if this is the alternate buffer.

    til::rect _rcAltSavedClientNew;
    til::rect _rcAltSavedClientOld;
    bool _fAltWindowChanged;

    TextAttribute _PopupAttributes;

    FontInfo _currentFont;
    FontInfoDesired _desiredFont;

    // Tracks the last virtual position the viewport was at. This is not
    //  affected by the user scrolling the viewport, only when API calls cause
    //  the viewport to move (SetBufferInfo, WriteConsole, etc)
    til::CoordType _virtualBottom;

    bool _ignoreLegacyEquivalentVTAttributes;

    std::optional<til::size> _deferredPtyResize{ std::nullopt };

    static void _handleDeferredResize(SCREEN_INFORMATION& siMain);

#ifdef UNIT_TESTING
    friend class TextBufferIteratorTests;
    friend class ScreenBufferTests;
    friend class CommonState;
    friend class ConptyOutputTests;
    friend class TerminalCoreUnitTests::ConptyRoundtripTests;
#endif
};

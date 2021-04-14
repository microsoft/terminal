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
#include "ScreenBufferRenderTarget.hpp"

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

#include "../inc/ITerminalOutputConnection.hpp"

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
    [[nodiscard]] static NTSTATUS CreateInstance(_In_ COORD coordWindowSize,
                                                 const FontInfo fontInfo,
                                                 _In_ COORD coordScreenBufferSize,
                                                 const TextAttribute defaultAttributes,
                                                 const TextAttribute popupAttributes,
                                                 const UINT uiCursorSize,
                                                 _Outptr_ SCREEN_INFORMATION** const ppScreen);

    ~SCREEN_INFORMATION();

    void GetScreenBufferInformation(_Out_ PCOORD pcoordSize,
                                    _Out_ PCOORD pcoordCursorPosition,
                                    _Out_ PSMALL_RECT psrWindow,
                                    _Out_ PWORD pwAttributes,
                                    _Out_ PCOORD pcoordMaximumWindowSize,
                                    _Out_ PWORD pwPopupAttributes,
                                    _Out_writes_(COLOR_TABLE_SIZE) LPCOLORREF lpColorTable) const;

    void GetRequiredConsoleSizeInPixels(_Out_ PSIZE const pRequiredSize) const;

    void MakeCurrentCursorVisible();

    void ClipToScreenBuffer(_Inout_ SMALL_RECT* const psrClip) const;

    COORD GetMinWindowSizeInCharacters(const COORD coordFontSize = { 1, 1 }) const;
    COORD GetMaxWindowSizeInCharacters(const COORD coordFontSize = { 1, 1 }) const;
    COORD GetLargestWindowSizeInCharacters(const COORD coordFontSize = { 1, 1 }) const;
    COORD GetScrollBarSizesInCharacters() const;

    Microsoft::Console::Types::Viewport GetBufferSize() const;
    Microsoft::Console::Types::Viewport GetTerminalBufferSize() const;

    COORD GetScreenFontSize() const;
    void UpdateFont(const FontInfo* const pfiNewFont);
    void RefreshFontWithRenderer();

    [[nodiscard]] NTSTATUS ResizeScreenBuffer(const COORD coordNewScreenSize, const bool fDoScrollBarUpdate);

    void NotifyAccessibilityEventing(const short sStartX, const short sStartY, const short sEndX, const short sEndY);

    void UpdateScrollBars();
    void InternalUpdateScrollBars();

    bool IsMaximizedBoth() const;
    bool IsMaximizedX() const;
    bool IsMaximizedY() const;

    const Microsoft::Console::Types::Viewport& GetViewport() const noexcept;
    void SetViewport(const Microsoft::Console::Types::Viewport& newViewport, const bool updateBottom);
    Microsoft::Console::Types::Viewport GetVirtualViewport() const noexcept;

    void ProcessResizeWindow(const RECT* const prcClientNew, const RECT* const prcClientOld);
    void SetViewportSize(const COORD* const pcoordSize);

    // Forwarders to Window if we're the active buffer.
    [[nodiscard]] NTSTATUS SetViewportOrigin(const bool fAbsolute, const COORD coordWindowOrigin, const bool updateBottom);

    bool SendNotifyBeep() const;
    bool PostUpdateWindowSize() const;

    // TODO: MSFT 9355062 these methods should probably be a part of construction/destruction. http://osgvsowi/9355062
    static void s_InsertScreenBuffer(_In_ SCREEN_INFORMATION* const pScreenInfo);
    static void s_RemoveScreenBuffer(_In_ SCREEN_INFORMATION* const pScreenInfo);

    OutputCellRect ReadRect(const Microsoft::Console::Types::Viewport location) const;

    TextBufferCellIterator GetCellDataAt(const COORD at) const;
    TextBufferCellIterator GetCellLineDataAt(const COORD at) const;
    TextBufferCellIterator GetCellDataAt(const COORD at, const Microsoft::Console::Types::Viewport limit) const;
    TextBufferTextIterator GetTextDataAt(const COORD at) const;
    TextBufferTextIterator GetTextLineDataAt(const COORD at) const;
    TextBufferTextIterator GetTextDataAt(const COORD at, const Microsoft::Console::Types::Viewport limit) const;

    OutputCellIterator Write(const OutputCellIterator it);

    OutputCellIterator Write(const OutputCellIterator it,
                             const COORD target,
                             const std::optional<bool> wrap = true);

    OutputCellIterator WriteRect(const OutputCellIterator it,
                                 const Microsoft::Console::Types::Viewport viewport);

    void WriteRect(const OutputCellRect& data,
                   const COORD location);

    void ClearTextData();

    std::pair<COORD, COORD> GetWordBoundary(const COORD position) const;

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

    void SetCursorColor(const unsigned int Color, const bool setMain = false) noexcept;

    void SetCursorType(const CursorType Type, const bool setMain = false) noexcept;

    void SetCursorDBMode(const bool DoubleCursor);
    [[nodiscard]] NTSTATUS SetCursorPosition(const COORD Position, const bool TurnOn);

    void MakeCursorVisible(const COORD CursorPosition, const bool updateBottom = true);

    Microsoft::Console::Types::Viewport GetRelativeScrollMargins() const;
    Microsoft::Console::Types::Viewport GetAbsoluteScrollMargins() const;
    void SetScrollMargins(const Microsoft::Console::Types::Viewport margins);
    bool AreMarginsSet() const noexcept;
    bool IsCursorInMargins(const COORD cursorPosition) const noexcept;

    [[nodiscard]] NTSTATUS UseAlternateScreenBuffer();
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

    [[nodiscard]] HRESULT VtEraseAll();

    void SetTerminalConnection(_In_ Microsoft::Console::ITerminalOutputConnection* const pTtyConnection);

    void UpdateBottom();
    void MoveToBottom();

    Microsoft::Console::Render::IRenderTarget& GetRenderTarget() noexcept;

    FontInfo& GetCurrentFont() noexcept;
    const FontInfo& GetCurrentFont() const noexcept;

    FontInfoDesired& GetDesiredFont() noexcept;
    const FontInfoDesired& GetDesiredFont() const noexcept;

    void InitializeCursorRowAttributes();

    void SetIgnoreLegacyEquivalentVTAttributes() noexcept;
    void ResetIgnoreLegacyEquivalentVTAttributes() noexcept;

private:
    SCREEN_INFORMATION(_In_ Microsoft::Console::Interactivity::IWindowMetrics* pMetrics,
                       _In_ Microsoft::Console::Interactivity::IAccessibilityNotifier* pNotifier,
                       const TextAttribute popupAttributes,
                       const FontInfo fontInfo);

    Microsoft::Console::Interactivity::IWindowMetrics* _pConsoleWindowMetrics;
    Microsoft::Console::Interactivity::IAccessibilityNotifier* _pAccessibilityNotifier;

    [[nodiscard]] HRESULT _AdjustScreenBufferHelper(const RECT* const prcClientNew,
                                                    const COORD coordBufferOld,
                                                    _Out_ COORD* const pcoordClientNewCharacters);
    [[nodiscard]] HRESULT _AdjustScreenBuffer(const RECT* const prcClientNew);
    void _CalculateViewportSize(const RECT* const prcClientArea, _Out_ COORD* const pcoordSize);
    void _AdjustViewportSize(const RECT* const prcClientNew, const RECT* const prcClientOld, const COORD* const pcoordSize);
    void _InternalSetViewportSize(const COORD* const pcoordSize, const bool fResizeFromTop, const bool fResizeFromLeft);

    static void s_CalculateScrollbarVisibility(const RECT* const prcClientArea,
                                               const COORD* const pcoordBufferSize,
                                               const COORD* const pcoordFontSize,
                                               _Out_ bool* const pfIsHorizontalVisible,
                                               _Out_ bool* const pfIsVerticalVisible);

    [[nodiscard]] NTSTATUS ResizeWithReflow(const COORD coordnewScreenSize);
    [[nodiscard]] NTSTATUS ResizeTraditional(const COORD coordNewScreenSize);

    [[nodiscard]] NTSTATUS _InitializeOutputStateMachine();
    void _FreeOutputStateMachine();

    [[nodiscard]] NTSTATUS _CreateAltBuffer(_Out_ SCREEN_INFORMATION** const ppsiNewScreenBuffer);

    bool _IsAltBuffer() const;
    bool _IsInPtyMode() const;
    bool _IsInVTMode() const;

    std::shared_ptr<Microsoft::Console::VirtualTerminal::StateMachine> _stateMachine;

    Microsoft::Console::Types::Viewport _scrollMargins; //The margins of the VT specified scroll region. Left and Right are currently unused, but could be in the future.

    // Specifies which coordinates of the screen buffer are visible in the
    //      window client (the "viewport" into the buffer)
    Microsoft::Console::Types::Viewport _viewport;

    SCREEN_INFORMATION* _psiAlternateBuffer; // The VT "Alternate" screen buffer.
    SCREEN_INFORMATION* _psiMainBuffer; // A pointer to the main buffer, if this is the alternate buffer.

    RECT _rcAltSavedClientNew;
    RECT _rcAltSavedClientOld;
    bool _fAltWindowChanged;

    TextAttribute _PopupAttributes;

    FontInfo _currentFont;
    FontInfoDesired _desiredFont;

    // Tracks the last virtual position the viewport was at. This is not
    //  affected by the user scrolling the viewport, only when API calls cause
    //  the viewport to move (SetBufferInfo, WriteConsole, etc)
    short _virtualBottom;

    ScreenBufferRenderTarget _renderTarget;

    bool _ignoreLegacyEquivalentVTAttributes;

#ifdef UNIT_TESTING
    friend class TextBufferIteratorTests;
    friend class ScreenBufferTests;
    friend class CommonState;
    friend class ConptyOutputTests;
    friend class TerminalCoreUnitTests::ConptyRoundtripTests;
#endif
};

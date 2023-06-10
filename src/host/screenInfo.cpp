// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "screenInfo.hpp"
#include "dbcs.h"
#include "output.h"
#include "_output.h"
#include "misc.h"
#include "handle.h"

#include <cmath>
#include "../interactivity/inc/ServiceLocator.hpp"
#include "../types/inc/Viewport.hpp"
#include "../types/inc/GlyphWidth.hpp"
#include "../terminal/parser/OutputStateMachineEngine.hpp"

#include "../types/inc/convert.hpp"

#pragma hdrstop

using namespace Microsoft::Console;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::VirtualTerminal;

#pragma region Construct_Destruct

SCREEN_INFORMATION::SCREEN_INFORMATION(
    _In_ IWindowMetrics* pMetrics,
    _In_ IAccessibilityNotifier* pNotifier,
    const TextAttribute popupAttributes,
    const FontInfo fontInfo) :
    OutputMode{ ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT },
    ResizingWindow{ 0 },
    WheelDelta{ 0 },
    HWheelDelta{ 0 },
    _textBuffer{ nullptr },
    Next{ nullptr },
    WriteConsoleDbcsLeadByte{ 0, 0 },
    FillOutDbcsLeadChar{ 0 },
    ConvScreenInfo{ nullptr },
    ScrollScale{ 1ul },
    _pConsoleWindowMetrics{ pMetrics },
    _pAccessibilityNotifier{ pNotifier },
    _api{ *this },
    _stateMachine{ nullptr },
    _viewport(Viewport::Empty()),
    _psiAlternateBuffer{ nullptr },
    _psiMainBuffer{ nullptr },
    _fAltWindowChanged{ false },
    _PopupAttributes{ popupAttributes },
    _virtualBottom{ 0 },
    _currentFont{ fontInfo },
    _desiredFont{ fontInfo },
    _ignoreLegacyEquivalentVTAttributes{ false }
{
    // Check if VT mode should be enabled by default. This can be true if
    // VirtualTerminalLevel is set to !=0 in the registry, or when conhost
    // is started in conpty mode.
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (gci.GetDefaultVirtTermLevel() != 0)
    {
        OutputMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    }
}

// Routine Description:
// - This routine frees the memory associated with a screen buffer.
// Arguments:
// - ScreenInfo - screen buffer data to free.
// Return Value:
// Note:
// - console handle table lock must be held when calling this routine
SCREEN_INFORMATION::~SCREEN_INFORMATION()
{
    _FreeOutputStateMachine();
}

// Routine Description:
// - This routine allocates and initializes the data associated with a screen buffer.
// Arguments:
// - ScreenInformation - the new screen buffer.
// - coordWindowSize - the initial size of screen buffer's window (in rows/columns)
// - nFont - the initial font to generate text with.
// - dwScreenBufferSize - the initial size of the screen buffer (in rows/columns).
// Return Value:
[[nodiscard]] NTSTATUS SCREEN_INFORMATION::CreateInstance(_In_ til::size coordWindowSize,
                                                          const FontInfo fontInfo,
                                                          _In_ til::size coordScreenBufferSize,
                                                          const TextAttribute defaultAttributes,
                                                          const TextAttribute popupAttributes,
                                                          const UINT uiCursorSize,
                                                          _Outptr_ SCREEN_INFORMATION** const ppScreen)
{
    *ppScreen = nullptr;

    try
    {
        auto pMetrics = ServiceLocator::LocateWindowMetrics();
        THROW_HR_IF_NULL(E_FAIL, pMetrics);

        const auto pNotifier = ServiceLocator::LocateAccessibilityNotifier();
        // It is possible for pNotifier to be null and that's OK.
        // For instance, the PTY doesn't need to send events. Just pass it along
        // and be sure that `SCREEN_INFORMATION` bypasses all event work if it's not there.
        const auto pScreen = new SCREEN_INFORMATION(pMetrics, pNotifier, popupAttributes, fontInfo);

        // Set up viewport
        pScreen->_viewport = Viewport::FromDimensions({ 0, 0 },
                                                      pScreen->_IsInPtyMode() ? coordScreenBufferSize : coordWindowSize);
        pScreen->UpdateBottom();

        // Set up text buffer
        pScreen->_textBuffer = std::make_unique<TextBuffer>(coordScreenBufferSize,
                                                            defaultAttributes,
                                                            uiCursorSize,
                                                            pScreen->IsActiveScreenBuffer(),
                                                            *ServiceLocator::LocateGlobals().pRender);

        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        pScreen->_textBuffer->GetCursor().SetType(gci.GetCursorType());

        const auto status = pScreen->_InitializeOutputStateMachine();

        if (SUCCEEDED_NTSTATUS(status))
        {
            *ppScreen = pScreen;
        }

        LOG_IF_NTSTATUS_FAILED(status);
        return status;
    }
    catch (...)
    {
        return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
    }
}

Viewport SCREEN_INFORMATION::GetBufferSize() const
{
    return _textBuffer->GetSize();
}

// Method Description:
// - Returns the "terminal" dimensions of this buffer. If we're in Terminal
//      Scrolling mode, this will return our Y dimension as only extending up to
//      the _virtualBottom. The height of the returned viewport would then be
//      (number of lines in scrollback) + (number of lines in viewport).
//   If we're not in terminal scrolling mode, this will return our normal buffer
//      size.
// Arguments:
// - <none>
// Return Value:
// - a viewport whose height is the height of the "terminal" portion of the
//      buffer in terminal scrolling mode, and is the height of the full buffer
//      in normal scrolling mode.
Viewport SCREEN_INFORMATION::GetTerminalBufferSize() const
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    auto v = _textBuffer->GetSize();
    if (gci.IsTerminalScrolling() && v.Height() > _virtualBottom)
    {
        v = Viewport::FromDimensions({ 0, 0 }, v.Width(), _virtualBottom + 1);
    }
    return v;
}

const StateMachine& SCREEN_INFORMATION::GetStateMachine() const
{
    return *_stateMachine;
}

StateMachine& SCREEN_INFORMATION::GetStateMachine()
{
    return *_stateMachine;
}

// Routine Description:
// - This routine inserts the screen buffer pointer into the console's list of screen buffers.
// Arguments:
// - ScreenInfo - Pointer to screen information structure.
// Return Value:
// Note:
// - The console lock must be held when calling this routine.
void SCREEN_INFORMATION::s_InsertScreenBuffer(_In_ SCREEN_INFORMATION* const pScreenInfo)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    FAIL_FAST_IF(!(gci.IsConsoleLocked()));

    pScreenInfo->Next = gci.ScreenBuffers;
    gci.ScreenBuffers = pScreenInfo;
}

// Routine Description:
// - This routine removes the screen buffer pointer from the console's list of screen buffers.
// Arguments:
// - ScreenInfo - Pointer to screen information structure.
// Return Value:
// Note:
// - The console lock must be held when calling this routine.
void SCREEN_INFORMATION::s_RemoveScreenBuffer(_In_ SCREEN_INFORMATION* const pScreenInfo)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (pScreenInfo == gci.ScreenBuffers)
    {
        gci.ScreenBuffers = pScreenInfo->Next;
    }
    else
    {
        auto* Cur = gci.ScreenBuffers;
        auto* Prev = Cur;
        while (Cur != nullptr)
        {
            if (pScreenInfo == Cur)
            {
                break;
            }

            Prev = Cur;
            Cur = Cur->Next;
        }

        FAIL_FAST_IF_NULL(Cur);
        Prev->Next = Cur->Next;
    }

    if (pScreenInfo == gci.pCurrentScreenBuffer &&
        gci.ScreenBuffers != gci.pCurrentScreenBuffer)
    {
        if (gci.ScreenBuffers != nullptr)
        {
            SetActiveScreenBuffer(*gci.ScreenBuffers);
        }
        else
        {
            gci.pCurrentScreenBuffer = nullptr;
        }
    }

    delete pScreenInfo;
}

#pragma endregion

#pragma region Output State Machine

[[nodiscard]] NTSTATUS SCREEN_INFORMATION::_InitializeOutputStateMachine()
{
    try
    {
        auto& g = ServiceLocator::LocateGlobals();
        auto& gci = g.getConsoleInformation();
        auto& renderer = *g.pRender;
        auto& renderSettings = gci.GetRenderSettings();
        auto& terminalInput = gci.GetActiveInputBuffer()->GetTerminalInput();
        auto adapter = std::make_unique<AdaptDispatch>(_api, renderer, renderSettings, terminalInput);
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(adapter));
        // Note that at this point in the setup, we haven't determined if we're
        //      in VtIo mode or not yet. We'll set the OutputStateMachine's
        //      TerminalConnection later, in VtIo::StartIfNeeded
        _stateMachine = std::make_shared<StateMachine>(std::move(engine));
    }
    catch (...)
    {
        // if any part of initialization failed, free the allocated ones.
        _FreeOutputStateMachine();

        return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
    }

    return STATUS_SUCCESS;
}

// If we're an alternate buffer, we want to give the GetSet back to our main
void SCREEN_INFORMATION::_FreeOutputStateMachine()
{
    if (_psiMainBuffer == nullptr) // If this is a main buffer
    {
        if (_psiAlternateBuffer != nullptr)
        {
            s_RemoveScreenBuffer(_psiAlternateBuffer);
        }

        _stateMachine.reset();
    }
}
#pragma endregion

#pragma region IIoProvider

// Method Description:
// - Return the active screen buffer of the console.
// Arguments:
// - <none>
// Return Value:
// - the active screen buffer of the console.
SCREEN_INFORMATION& SCREEN_INFORMATION::GetActiveOutputBuffer()
{
    return GetActiveBuffer();
}

const SCREEN_INFORMATION& SCREEN_INFORMATION::GetActiveOutputBuffer() const
{
    return GetActiveBuffer();
}

// Method Description:
// - Return the active input buffer of the console.
// Arguments:
// - <none>
// Return Value:
// - the active input buffer of the console.
InputBuffer* const SCREEN_INFORMATION::GetActiveInputBuffer() const
{
    return ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveInputBuffer();
}

#pragma endregion

#pragma region Get Data

bool SCREEN_INFORMATION::IsActiveScreenBuffer() const
{
    // the following macro returns TRUE if the given screen buffer is the active screen buffer.

    //#define ACTIVE_SCREEN_BUFFER(SCREEN_INFO) (gci.CurrentScreenBuffer == SCREEN_INFO)
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return (gci.pCurrentScreenBuffer == this);
}

// Routine Description:
// - This routine returns data about the screen buffer.
// Arguments:
// - Size - Pointer to location in which to store screen buffer size.
// - CursorPosition - Pointer to location in which to store the cursor position.
// - ScrollPosition - Pointer to location in which to store the scroll position.
// - Attributes - Pointer to location in which to store the default attributes.
// - CurrentWindowSize - Pointer to location in which to store current window size.
// - MaximumWindowSize - Pointer to location in which to store maximum window size.
// Return Value:
// - None
void SCREEN_INFORMATION::GetScreenBufferInformation(_Out_ til::size* pcoordSize,
                                                    _Out_ til::point* pcoordCursorPosition,
                                                    _Out_ til::inclusive_rect* psrWindow,
                                                    _Out_ PWORD pwAttributes,
                                                    _Out_ til::size* pcoordMaximumWindowSize,
                                                    _Out_ PWORD pwPopupAttributes,
                                                    _Out_writes_(COLOR_TABLE_SIZE) LPCOLORREF lpColorTable) const
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    *pcoordSize = GetBufferSize().Dimensions();

    *pcoordCursorPosition = _textBuffer->GetCursor().GetPosition();

    *psrWindow = _viewport.ToInclusive();

    *pwAttributes = GetAttributes().GetLegacyAttributes();
    *pwPopupAttributes = _PopupAttributes.GetLegacyAttributes();

    // the copy length must be constant for now to keep OACR happy with buffer overruns.
    for (size_t i = 0; i < COLOR_TABLE_SIZE; i++)
    {
        lpColorTable[i] = gci.GetLegacyColorTableEntry(i);
    }

    *pcoordMaximumWindowSize = GetMaxWindowSizeInCharacters();
}

// Routine Description:
// - Gets the smallest possible client area in characters. Takes the window client area and divides by the active font dimensions.
// Arguments:
// - coordFontSize - The font size to use for calculation if a screen buffer is not yet attached.
// Return Value:
// - til::size containing the width and height representing the minimum character grid that can be rendered in the window.
til::size SCREEN_INFORMATION::GetMinWindowSizeInCharacters(const til::size coordFontSize /*= { 1, 1 }*/) const
{
    FAIL_FAST_IF(coordFontSize.width == 0);
    FAIL_FAST_IF(coordFontSize.height == 0);

    // prepare rectangle
    const auto rcWindowInPixels = _pConsoleWindowMetrics->GetMinClientRectInPixels();

    // assign the pixel widths and heights to the final output
    til::size coordClientAreaSize;
    coordClientAreaSize.width = rcWindowInPixels.width();
    coordClientAreaSize.height = rcWindowInPixels.height();

    // now retrieve the font size and divide the pixel counts into character counts
    auto coordFont = coordFontSize; // by default, use the size we were given

    // If text info has been set up, instead retrieve its font size
    if (_textBuffer != nullptr)
    {
        coordFont = GetScreenFontSize();
    }

    FAIL_FAST_IF(coordFont.width == 0);
    FAIL_FAST_IF(coordFont.height == 0);

    coordClientAreaSize.width /= coordFont.width;
    coordClientAreaSize.height /= coordFont.height;

    return coordClientAreaSize;
}

// Routine Description:
// - Gets the maximum client area in characters that would fit on the current monitor or given the current buffer size.
//   Takes the monitor work area and divides by the active font dimensions then limits by buffer size.
// Arguments:
// - coordFontSize - The font size to use for calculation if a screen buffer is not yet attached.
// Return Value:
// - til::size containing the width and height representing the largest character
//      grid that can be rendered on the current monitor and/or from the current buffer size.
til::size SCREEN_INFORMATION::GetMaxWindowSizeInCharacters(const til::size coordFontSize /*= { 1, 1 }*/) const
{
    FAIL_FAST_IF(coordFontSize.width == 0);
    FAIL_FAST_IF(coordFontSize.height == 0);

    const auto coordScreenBufferSize = GetBufferSize().Dimensions();
    auto coordClientAreaSize = coordScreenBufferSize;

    //  Important re: headless consoles on onecore (for telnetd, etc.)
    // GetConsoleScreenBufferInfoEx hits this to get the max size of the display.
    // Because we're headless, we don't really care about the max size of the display.
    // In that case, we'll just return the buffer size as the "max" window size.
    if (!ServiceLocator::LocateGlobals().IsHeadless())
    {
        const auto coordWindowRestrictedSize = GetLargestWindowSizeInCharacters(coordFontSize);
        // If the buffer is smaller than what the max window would allow, then the max client area can only be as big as the
        // buffer we have.
        coordClientAreaSize.width = std::min(coordScreenBufferSize.width, coordWindowRestrictedSize.width);
        coordClientAreaSize.height = std::min(coordScreenBufferSize.height, coordWindowRestrictedSize.height);
    }

    return coordClientAreaSize;
}

// Routine Description:
// - Gets the largest possible client area in characters if the window were stretched as large as it could go.
// - Takes the window client area and divides by the active font dimensions.
// Arguments:
// - coordFontSize - The font size to use for calculation if a screen buffer is not yet attached.
// Return Value:
// - til::size containing the width and height representing the largest character
//      grid that can be rendered on the current monitor with the maximum size window.
til::size SCREEN_INFORMATION::GetLargestWindowSizeInCharacters(const til::size coordFontSize /*= { 1, 1 }*/) const
{
    FAIL_FAST_IF(coordFontSize.width == 0);
    FAIL_FAST_IF(coordFontSize.height == 0);

    const auto rcClientInPixels = _pConsoleWindowMetrics->GetMaxClientRectInPixels();

    // first assign the pixel widths and heights to the final output
    til::size coordClientAreaSize;
    coordClientAreaSize.width = rcClientInPixels.width();
    coordClientAreaSize.height = rcClientInPixels.height();

    // now retrieve the font size and divide the pixel counts into character counts
    auto coordFont = coordFontSize; // by default, use the size we were given

    // If renderer has been set up, instead retrieve its font size
    if (ServiceLocator::LocateGlobals().pRender != nullptr)
    {
        coordFont = GetScreenFontSize();
    }

    FAIL_FAST_IF(coordFont.width == 0);
    FAIL_FAST_IF(coordFont.height == 0);

    coordClientAreaSize.width /= coordFont.width;
    coordClientAreaSize.height /= coordFont.height;

    return coordClientAreaSize;
}

til::size SCREEN_INFORMATION::GetScrollBarSizesInCharacters() const
{
    auto coordFont = GetScreenFontSize();

    auto vScrollSize = ServiceLocator::LocateGlobals().sVerticalScrollSize;
    auto hScrollSize = ServiceLocator::LocateGlobals().sHorizontalScrollSize;

    til::size coordBarSizes;
    coordBarSizes.width = (vScrollSize / coordFont.width) + ((vScrollSize % coordFont.width) != 0 ? 1 : 0);
    coordBarSizes.height = (hScrollSize / coordFont.height) + ((hScrollSize % coordFont.height) != 0 ? 1 : 0);

    return coordBarSizes;
}

void SCREEN_INFORMATION::GetRequiredConsoleSizeInPixels(_Out_ til::size* const pRequiredSize) const
{
    const auto coordFontSize = GetCurrentFont().GetSize();

    // TODO: Assert valid size boundaries
    pRequiredSize->width = GetViewport().Width() * coordFontSize.width;
    pRequiredSize->height = GetViewport().Height() * coordFontSize.height;
}

til::size SCREEN_INFORMATION::GetScreenFontSize() const
{
    // If we have no renderer, then we don't really need any sort of pixel math. so the "font size" for the scale factor
    // (which is used almost everywhere around the code as * and / calls) should just be 1,1 so those operations will do
    // effectively nothing.
    til::size coordRet = { 1, 1 };
    if (ServiceLocator::LocateGlobals().pRender != nullptr)
    {
        coordRet = GetCurrentFont().GetSize();
    }

    // For sanity's sake, make sure not to leak 0 out as a possible value. These values are used in division operations.
    coordRet.width = std::max(coordRet.width, 1);
    coordRet.height = std::max(coordRet.height, 1);

    return coordRet;
}

#pragma endregion

#pragma region Set Data

void SCREEN_INFORMATION::RefreshFontWithRenderer()
{
    if (IsActiveScreenBuffer())
    {
        // Hand the handle to our internal structure to the font change trigger in case it updates it based on what's appropriate.
        if (ServiceLocator::LocateGlobals().pRender != nullptr)
        {
            ServiceLocator::LocateGlobals().pRender->TriggerFontChange(ServiceLocator::LocateGlobals().dpi,
                                                                       GetDesiredFont(),
                                                                       GetCurrentFont());

            NotifyGlyphWidthFontChanged();
        }
    }
}

void SCREEN_INFORMATION::UpdateFont(const FontInfo* const pfiNewFont)
{
    FontInfoDesired fiDesiredFont(*pfiNewFont);

    GetDesiredFont() = fiDesiredFont;

    RefreshFontWithRenderer();

    // If we're the active screen buffer...
    if (IsActiveScreenBuffer())
    {
        // If there is a window attached, let it know that it should try to update so the rows/columns are now accounting for the new font.
        const auto pWindow = ServiceLocator::LocateConsoleWindow();
        if (nullptr != pWindow)
        {
            auto coordViewport = GetViewport().Dimensions();
            pWindow->UpdateWindowSize(coordViewport);
        }
    }

    // If we're an alt buffer, also update our main buffer.
    if (_psiMainBuffer)
    {
        _psiMainBuffer->UpdateFont(pfiNewFont);
    }
}

// Routine Description:
// - Informs clients whether we have accessibility eventing so they can
//   save themselves the work of performing math or lookups before calling
//   `NotifyAccessibilityEventing`.
// Arguments:
// - <none>
// Return Value:
// - True if we have an accessibility listener. False otherwise.
bool SCREEN_INFORMATION::HasAccessibilityEventing() const noexcept
{
    return _pAccessibilityNotifier;
}

// NOTE: This method was historically used to notify accessibility apps AND
// to aggregate drawing metadata to determine whether or not to use PolyTextOut.
// After the Nov 2015 graphics refactor, the metadata drawing flag calculation is no longer necessary.
// This now only notifies accessibility apps of a change.
void SCREEN_INFORMATION::NotifyAccessibilityEventing(const til::CoordType sStartX,
                                                     const til::CoordType sStartY,
                                                     const til::CoordType sEndX,
                                                     const til::CoordType sEndY)
{
    if (!_pAccessibilityNotifier)
    {
        return;
    }

    // Fire off a winevent to let accessibility apps know what changed.
    if (IsActiveScreenBuffer())
    {
        const auto coordScreenBufferSize = GetBufferSize().Dimensions();
        FAIL_FAST_IF(!(sEndX < coordScreenBufferSize.width));

        if (sStartX == sEndX && sStartY == sEndY)
        {
            try
            {
                const auto cellData = GetCellDataAt({ sStartX, sStartY });
                const auto charAndAttr = MAKELONG(Utf16ToUcs2(cellData->Chars()),
                                                  cellData->TextAttr().GetLegacyAttributes());
                _pAccessibilityNotifier->NotifyConsoleUpdateSimpleEvent(MAKELONG(sStartX, sStartY),
                                                                        charAndAttr);
            }
            catch (...)
            {
                LOG_HR(wil::ResultFromCaughtException());
                return;
            }
        }
        else
        {
            _pAccessibilityNotifier->NotifyConsoleUpdateRegionEvent(MAKELONG(sStartX, sStartY),
                                                                    MAKELONG(sEndX, sEndY));
        }
        auto pConsoleWindow = ServiceLocator::LocateConsoleWindow();
        if (pConsoleWindow)
        {
            LOG_IF_FAILED(pConsoleWindow->SignalUia(UIA_Text_TextChangedEventId));
            // TODO MSFT 7960168 do we really need this event to not signal?
            //pConsoleWindow->SignalUia(UIA_LayoutInvalidatedEventId);
        }
    }
}

#pragma endregion

#pragma region UI_Refresh

VOID SCREEN_INFORMATION::UpdateScrollBars()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (!IsActiveScreenBuffer())
    {
        return;
    }

    if (gci.Flags & CONSOLE_UPDATING_SCROLL_BARS)
    {
        return;
    }

    gci.Flags |= CONSOLE_UPDATING_SCROLL_BARS;

    if (ServiceLocator::LocateConsoleWindow() != nullptr)
    {
        ServiceLocator::LocateConsoleWindow()->PostUpdateScrollBars();
    }
}

VOID SCREEN_INFORMATION::InternalUpdateScrollBars()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto pWindow = ServiceLocator::LocateConsoleWindow();

    WI_ClearFlag(gci.Flags, CONSOLE_UPDATING_SCROLL_BARS);

    if (!IsActiveScreenBuffer())
    {
        return;
    }

    ResizingWindow++;

    if (pWindow != nullptr)
    {
        const auto buffer = GetBufferSize();

        // If this is the main buffer, make sure we enable both of the scroll bars.
        //      The alt buffer likely disabled the scroll bars, this is the only
        //      way to re-enable it.
        if (!_IsAltBuffer())
        {
            pWindow->EnableBothScrollBars();
        }

        pWindow->UpdateScrollBar(true,
                                 _IsAltBuffer(),
                                 _viewport.Height(),
                                 gci.IsTerminalScrolling() ? _virtualBottom : buffer.BottomInclusive(),
                                 _viewport.Top());
        pWindow->UpdateScrollBar(false,
                                 _IsAltBuffer(),
                                 _viewport.Width(),
                                 buffer.RightInclusive(),
                                 _viewport.Left());
    }

    // Fire off an event to let accessibility apps know the layout has changed.
    if (_pAccessibilityNotifier)
    {
        _pAccessibilityNotifier->NotifyConsoleLayoutEvent();
    }

    ResizingWindow--;
}

// Routine Description:
// - Modifies the size of the current viewport to match the width/height of the request given.
// - This will act like a resize operation from the bottom right corner of the window.
// Arguments:
// - pcoordSize - Requested viewport width/heights in characters
// Return Value:
// - <none>
void SCREEN_INFORMATION::SetViewportSize(const til::size* const pcoordSize)
{
    // If this is the alt buffer or a VT I/O buffer:
    //      first resize ourselves to match the new viewport
    //      then also make sure that the main buffer gets the same call
    //      (if necessary)
    if (_IsInPtyMode())
    {
        LOG_IF_FAILED(ResizeScreenBuffer(*pcoordSize, TRUE));

        if (_psiMainBuffer)
        {
            _psiMainBuffer->_fAltWindowChanged = false;
            _psiMainBuffer->_deferredPtyResize = GetBufferSize().Dimensions();
        }
    }
    _InternalSetViewportSize(pcoordSize, false, false);
}

// Method Description:
// - Update the origin of the buffer's viewport. You can either move the
//      viewport with a delta relative to its current location, or set its
//      absolute origin. Either way leaves the dimensions of the viewport
//      unchanged. Also potentially updates our "virtual bottom", the last real
//      location of the viewport in the buffer.
//  Also notifies the window implementation to update its scrollbars.
// Arguments:
// - fAbsolute: If true, coordWindowOrigin is the absolute location of the origin of the new viewport.
//      If false, coordWindowOrigin is a delta to move the viewport relative to its current position.
// - coordWindowOrigin: Either the new absolute position of the origin of the
//      viewport, or a delta to add to the current viewport location.
// - updateBottom: If true, update our virtual bottom position. This should be
//      false if we're moving the viewport in response to the users scrolling up
//      and down in the buffer, but API calls should set this to true.
// Return Value:
// - STATUS_INVALID_PARAMETER if the new viewport would be outside the buffer,
//      else STATUS_SUCCESS
[[nodiscard]] NTSTATUS SCREEN_INFORMATION::SetViewportOrigin(const bool fAbsolute,
                                                             const til::point coordWindowOrigin,
                                                             const bool updateBottom)
{
    // calculate window size
    auto WindowSize = _viewport.Dimensions();

    til::inclusive_rect NewWindow;
    // if relative coordinates, figure out absolute coords.
    if (!fAbsolute)
    {
        if (coordWindowOrigin.x == 0 && coordWindowOrigin.y == 0)
        {
            return STATUS_SUCCESS;
        }
        NewWindow.left = _viewport.Left() + coordWindowOrigin.x;
        NewWindow.top = _viewport.Top() + coordWindowOrigin.y;
    }
    else
    {
        if (coordWindowOrigin == _viewport.Origin())
        {
            return STATUS_SUCCESS;
        }
        NewWindow.left = coordWindowOrigin.x;
        NewWindow.top = coordWindowOrigin.y;
    }
    NewWindow.right = NewWindow.left + WindowSize.width - 1;
    NewWindow.bottom = NewWindow.top + WindowSize.height - 1;

    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    // If we're in terminal scrolling mode, and we're trying to set the viewport
    //      below the logical viewport, without updating our virtual bottom
    //      (the logical viewport's position), dont.
    //  Instead move us to the bottom of the logical viewport.
    if (gci.IsTerminalScrolling() && !updateBottom && NewWindow.bottom > _virtualBottom)
    {
        const auto delta = _virtualBottom - NewWindow.bottom;
        NewWindow.top += delta;
        NewWindow.bottom += delta;
    }

    // see if new window origin would extend window beyond extent of screen buffer
    const auto coordScreenBufferSize = GetBufferSize().Dimensions();
    if (NewWindow.left < 0 ||
        NewWindow.top < 0 ||
        NewWindow.right < 0 ||
        NewWindow.bottom < 0 ||
        NewWindow.right >= coordScreenBufferSize.width ||
        NewWindow.bottom >= coordScreenBufferSize.height)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (IsActiveScreenBuffer() && ServiceLocator::LocateConsoleWindow() != nullptr)
    {
        // Tell the window that it needs to set itself to the new origin if we're the active buffer.
        ServiceLocator::LocateConsoleWindow()->ChangeViewport(NewWindow);
    }
    else
    {
        // Otherwise, just store the new position and go on.
        _viewport = Viewport::FromInclusive(NewWindow);
        Tracing::s_TraceWindowViewport(_viewport);
    }

    // Update our internal virtual bottom tracker if requested. This helps keep
    //      the viewport's logical position consistent from the perspective of a
    //      VT client application, even if the user scrolls the viewport with the mouse.
    //      We typically only want to this to move the virtual bottom down, though,
    //      otherwise it can end up "truncating" the buffer if the user is viewing
    //      the scrollback at the time the viewport origin is updated.
    if (updateBottom && _virtualBottom < _viewport.BottomInclusive())
    {
        UpdateBottom();
    }

    return STATUS_SUCCESS;
}

bool SCREEN_INFORMATION::SendNotifyBeep() const
{
    if (IsActiveScreenBuffer())
    {
        if (ServiceLocator::LocateConsoleWindow() != nullptr)
        {
            return ServiceLocator::LocateConsoleWindow()->SendNotifyBeep();
        }
    }

    return false;
}

bool SCREEN_INFORMATION::PostUpdateWindowSize() const
{
    if (IsActiveScreenBuffer())
    {
        if (ServiceLocator::LocateConsoleWindow() != nullptr)
        {
            return ServiceLocator::LocateConsoleWindow()->PostUpdateWindowSize();
        }
    }

    return false;
}

// Routine Description:
// - Modifies the screen buffer and viewport dimensions when the available client area rendering space changes.
// Arguments:
// - prcClientNew - Client rectangle in pixels after this update
// - prcClientOld - Client rectangle in pixels before this update
// Return Value:
// - <none>
void SCREEN_INFORMATION::ProcessResizeWindow(const til::rect* const prcClientNew,
                                             const til::rect* const prcClientOld)
{
    if (_IsAltBuffer())
    {
        // Stash away the size of the window, we'll need to do this to the main when we pop back
        //  We set this on the main, so that main->alt(resize)->alt keeps the resize
        _psiMainBuffer->_fAltWindowChanged = true;
        _psiMainBuffer->_rcAltSavedClientNew = *prcClientNew;
        _psiMainBuffer->_rcAltSavedClientOld = *prcClientOld;
    }

    // 1.a In some modes, the screen buffer size needs to change on window size,
    //      so do that first.
    //      _AdjustScreenBuffer might hide the commandline. If it does so, it'll
    //      return S_OK instead of S_FALSE. In that case, we'll need to re-show
    //      the commandline ourselves once the viewport size is updated.
    //      (See 1.b below)
    const auto adjustBufferSizeResult = _AdjustScreenBuffer(prcClientNew);
    LOG_IF_FAILED(adjustBufferSizeResult);

    // 2. Now calculate how large the new viewport should be
    til::size coordViewportSize;
    _CalculateViewportSize(prcClientNew, &coordViewportSize);

    // 3. And adjust the existing viewport to match the same dimensions.
    //      The old/new comparison is to figure out which side the window was resized from.
    _AdjustViewportSize(prcClientNew, prcClientOld, &coordViewportSize);

    // 1.b  If we did actually change the buffer size, then we need to show the
    //      commandline again. We hid it during _AdjustScreenBuffer, but we
    //      couldn't turn it back on until the Viewport was updated to the new
    //      size. See MSFT:19976291
    if (SUCCEEDED(adjustBufferSizeResult) && adjustBufferSizeResult != S_FALSE)
    {
        auto& commandLine = CommandLine::Instance();
        commandLine.Show();
    }

    // 4. Finally, update the scroll bars.
    UpdateScrollBars();

    FAIL_FAST_IF(!(_viewport.Top() >= 0));
    // TODO MSFT: 17663344 - Audit call sites for this precondition. Extremely tiny offscreen windows.
    //FAIL_FAST_IF(!(_viewport.IsValid()));
}

#pragma endregion

#pragma region Support_Calculation

// Routine Description:
// - This helper converts client pixel areas into the number of characters that could fit into the client window.
// - It requires the buffer size to figure out whether it needs to reserve space for the scroll bars (or not).
// Arguments:
// - prcClientNew - Client region of window in pixels
// - coordBufferOld - Size of backing buffer in characters
// - pcoordClientNewCharacters - The maximum number of characters X by Y that can be displayed in the window with the given backing buffer.
// Return Value:
// - S_OK if math was successful. Check with SUCCEEDED/FAILED macro.
[[nodiscard]] HRESULT SCREEN_INFORMATION::_AdjustScreenBufferHelper(const til::rect* const prcClientNew,
                                                                    const til::size coordBufferOld,
                                                                    _Out_ til::size* const pcoordClientNewCharacters)
{
    // Get the font size ready.
    const auto coordFontSize = GetScreenFontSize();

    // We cannot operate if the font size is 0. This shouldn't happen, but stop early if it does.
    RETURN_HR_IF(E_NOT_VALID_STATE, 0 == coordFontSize.width || 0 == coordFontSize.height);

    // Find out how much client space we have to work with in the new area.
    til::size sizeClientNewPixels;
    sizeClientNewPixels.width = prcClientNew->width();
    sizeClientNewPixels.height = prcClientNew->height();

    // Subtract out scroll bar space if scroll bars will be necessary.
    auto fIsHorizontalVisible = false;
    auto fIsVerticalVisible = false;
    s_CalculateScrollbarVisibility(prcClientNew, &coordBufferOld, &coordFontSize, &fIsHorizontalVisible, &fIsVerticalVisible);

    if (fIsHorizontalVisible)
    {
        sizeClientNewPixels.height -= ServiceLocator::LocateGlobals().sHorizontalScrollSize;
    }

    if (fIsVerticalVisible)
    {
        sizeClientNewPixels.width -= ServiceLocator::LocateGlobals().sVerticalScrollSize;
    }

    // Now with the scroll bars removed, calculate how many characters could fit into the new window area.
    *pcoordClientNewCharacters = sizeClientNewPixels / coordFontSize;

    // If the new client is too tiny, our viewport will be 1x1.
    pcoordClientNewCharacters->width = std::max(pcoordClientNewCharacters->width, 1);
    pcoordClientNewCharacters->height = std::max(pcoordClientNewCharacters->height, 1);
    return S_OK;
}

// Routine Description:
// - Modifies the size of the backing text buffer when the window changes to support "intuitive" resizing modes by grabbing the window edges.
// - This function will compensate for scroll bars.
// - Buffer size changes will happen internally to this function.
// Arguments:
// - prcClientNew - Client rectangle in pixels after this update
// Return Value:
// - appropriate HRESULT
[[nodiscard]] HRESULT SCREEN_INFORMATION::_AdjustScreenBuffer(const til::rect* const prcClientNew)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    // Prepare the buffer sizes.
    // We need the main's size here to maintain the right scrollbar visibility.
    const auto coordBufferSizeOld = _IsAltBuffer() ? _psiMainBuffer->GetBufferSize().Dimensions() : GetBufferSize().Dimensions();
    auto coordBufferSizeNew = coordBufferSizeOld;

    // First figure out how many characters we could fit into the new window given the old buffer size
    til::size coordClientNewCharacters;

    RETURN_IF_FAILED(_AdjustScreenBufferHelper(prcClientNew, coordBufferSizeOld, &coordClientNewCharacters));

    // If we're in wrap text mode, then we want to be fixed to the window size. So use the character calculation we just got
    // to fix the buffer and window width together.
    if (gci.GetWrapText())
    {
        coordBufferSizeNew.width = coordClientNewCharacters.width;
    }

    // Reanalyze scroll bars in case we fixed the edge together for word wrap.
    // Use the new buffer client size.
    RETURN_IF_FAILED(_AdjustScreenBufferHelper(prcClientNew, coordBufferSizeNew, &coordClientNewCharacters));

    // Now reanalyze the buffer size and grow if we can fit more characters into the window no matter the console mode.
    if (_IsInPtyMode())
    {
        // The alt buffer always wants to be exactly the size of the screen, never more or less.
        // This prevents scrollbars when you increase the alt buffer size, then decrease it.
        // Can't have a buffer dimension of 0 - that'll cause divide by zeros in the future.
        coordBufferSizeNew.width = std::max(coordClientNewCharacters.width, 1);
        coordBufferSizeNew.height = std::max(coordClientNewCharacters.height, 1);
    }
    else
    {
        if (coordClientNewCharacters.width > coordBufferSizeNew.width)
        {
            coordBufferSizeNew.width = coordClientNewCharacters.width;
        }
        if (coordClientNewCharacters.height > coordBufferSizeNew.height)
        {
            coordBufferSizeNew.height = coordClientNewCharacters.height;
        }
    }

    auto hr = S_FALSE;

    // Only attempt to modify the buffer if something changed. Expensive operation.
    if (coordBufferSizeOld != coordBufferSizeNew)
    {
        auto& commandLine = CommandLine::Instance();

        // TODO: Deleting and redrawing the command line during resizing can cause flickering. See: http://osgvsowi/658439
        // 1. Delete input string if necessary (see menu.c)
        commandLine.Hide(FALSE);

        const auto savedCursorVisibility = _textBuffer->GetCursor().IsVisible();
        _textBuffer->GetCursor().SetIsVisible(false);

        // 2. Call the resize screen buffer method (expensive) to redimension the backing buffer (and reflow)
        LOG_IF_FAILED(ResizeScreenBuffer(coordBufferSizeNew, FALSE));

        // MSFT:19976291 Don't re-show the commandline here. We need to wait for
        //      the viewport to also get resized before we can re-show the commandline.
        //      ProcessResizeWindow will call commandline.Show() for us.
        _textBuffer->GetCursor().SetIsVisible(savedCursorVisibility);

        // Return S_OK, to indicate we succeeded and actually did something.
        hr = S_OK;
    }

    return hr;
}

// Routine Description:
// - Calculates what width/height the viewport must have to consume all the available space in the given client area.
// - This compensates for scroll bars and will leave space in the client area for the bars if necessary.
// Arguments:
// - prcClientArea - The client rectangle in pixels of available rendering space.
// - pcoordSize - Filled with the width/height to which the viewport should be set.
// Return Value:
// - <none>
void SCREEN_INFORMATION::_CalculateViewportSize(const til::rect* const prcClientArea, _Out_ til::size* const pcoordSize)
{
    const auto coordBufferSize = GetBufferSize().Dimensions();
    const auto coordFontSize = GetScreenFontSize();

    til::size sizeClientPixels;
    sizeClientPixels.width = prcClientArea->width();
    sizeClientPixels.height = prcClientArea->height();

    bool fIsHorizontalVisible;
    bool fIsVerticalVisible;
    s_CalculateScrollbarVisibility(prcClientArea,
                                   &coordBufferSize,
                                   &coordFontSize,
                                   &fIsHorizontalVisible,
                                   &fIsVerticalVisible);

    if (fIsHorizontalVisible)
    {
        sizeClientPixels.height -= ServiceLocator::LocateGlobals().sHorizontalScrollSize;
    }

    if (fIsVerticalVisible)
    {
        sizeClientPixels.width -= ServiceLocator::LocateGlobals().sVerticalScrollSize;
    }

    pcoordSize->width = (til::CoordType)(sizeClientPixels.width / coordFontSize.width);
    pcoordSize->height = (til::CoordType)(sizeClientPixels.height / coordFontSize.height);
}

// Routine Description:
// - Modifies the size of the current viewport to match the width/height of the request given.
// - Must specify which corner to adjust from. Default to false/false to resize from the bottom right corner.
// Arguments:
// - pcoordSize - Requested viewport width/heights in characters
// - fResizeFromTop - If false, will trim/add to bottom of viewport first. If true, will trim/add to top.
// - fResizeFromBottom - If false, will trim/add to top of viewport first. If true, will trim/add to left.
// Return Value:
// - <none>
void SCREEN_INFORMATION::_InternalSetViewportSize(const til::size* const pcoordSize,
                                                  const bool fResizeFromTop,
                                                  const bool fResizeFromLeft)
{
    const auto DeltaX = pcoordSize->width - _viewport.Width();
    const auto DeltaY = pcoordSize->height - _viewport.Height();
    const auto coordScreenBufferSize = GetBufferSize().Dimensions();

    // do adjustments on a copy that's easily manipulated.
    auto srNewViewport = _viewport.ToInclusive();

    // Now we need to determine what our new Window size should
    // be. Note that Window here refers to the character/row window.
    if (fResizeFromLeft)
    {
        // we're being horizontally sized from the left border
        const auto sLeftProposed = srNewViewport.left - DeltaX;
        if (sLeftProposed >= 0)
        {
            // there's enough room in the backlog to just expand left
            srNewViewport.left -= DeltaX;
        }
        else
        {
            // if we're resizing horizontally, we want to show as much
            // content above as we can, but we can't show more
            // than the left of the window
            srNewViewport.left = 0;
            srNewViewport.right += abs(sLeftProposed);
        }
    }
    else
    {
        // we're being horizontally sized from the right border
        const auto sRightProposed = (srNewViewport.right + DeltaX);
        if (sRightProposed <= (coordScreenBufferSize.width - 1))
        {
            srNewViewport.right += DeltaX;
        }
        else
        {
            srNewViewport.right = (coordScreenBufferSize.width - 1);
            srNewViewport.left -= (sRightProposed - (coordScreenBufferSize.width - 1));
        }
    }

    if (fResizeFromTop)
    {
        const auto sTopProposed = (srNewViewport.top - DeltaY);
        // we're being vertically sized from the top border
        if (sTopProposed >= 0)
        {
            // Special case: Only modify the top position if we're not
            // on the 0th row of the buffer.

            // If we're on the 0th row, people expect it to stay stuck
            // to the top of the window, not to start collapsing down
            // and hiding the top rows.
            if (srNewViewport.top > 0)
            {
                // there's enough room in the backlog to just expand the top
                srNewViewport.top -= DeltaY;
            }
            else
            {
                // If we didn't adjust the top, we need to trim off
                // the number of rows from the bottom instead.
                // NOTE: It's += because DeltaY will be negative
                // already for this circumstance.
                FAIL_FAST_IF(!(DeltaY <= 0));
                srNewViewport.bottom += DeltaY;
            }
        }
        else
        {
            // if we're resizing vertically, we want to show as much
            // content above as we can, but we can't show more
            // than the top of the window
            srNewViewport.top = 0;
            srNewViewport.bottom += abs(sTopProposed);
        }
    }
    else
    {
        // we're being vertically sized from the bottom border
        const auto sBottomProposed = (srNewViewport.bottom + DeltaY);
        if (sBottomProposed <= (coordScreenBufferSize.height - 1))
        {
            // If the new bottom is supposed to be before the final line of the buffer
            // Check to ensure that we don't hide the prompt by collapsing the window.

            // The final valid end position will be the coordinates of
            // the last character displayed (including any characters
            // in the input line)
            til::point coordValidEnd;
            Selection::Instance().GetValidAreaBoundaries(nullptr, &coordValidEnd);

            // If the bottom of the window when adjusted would be
            // above the final line of valid text...
            if (srNewViewport.bottom + DeltaY < coordValidEnd.y)
            {
                // Adjust the top of the window instead of the bottom
                // (so the lines slide upward)
                srNewViewport.top -= DeltaY;
            }
            else
            {
                srNewViewport.bottom += DeltaY;
            }
        }
        else
        {
            srNewViewport.bottom = (coordScreenBufferSize.height - 1);
            srNewViewport.top -= (sBottomProposed - (coordScreenBufferSize.height - 1));
        }
    }

    // Ensure the viewport is valid.
    // We can't have a negative left or top.
    if (srNewViewport.left < 0)
    {
        srNewViewport.right -= srNewViewport.left;
        srNewViewport.left = 0;
    }

    if (srNewViewport.top < 0)
    {
        srNewViewport.bottom -= srNewViewport.top;
        srNewViewport.top = 0;
    }

    // Bottom and right cannot pass the final characters in the array.
    const auto offRightDelta = srNewViewport.right - (coordScreenBufferSize.width - 1);
    if (offRightDelta > 0) // the viewport was off the right of the buffer...
    {
        // ...so slide both left/right back into the buffer. This will prevent us
        // from having a negative width later.
        srNewViewport.right -= offRightDelta;
        srNewViewport.left = std::max(0, srNewViewport.left - offRightDelta);
    }
    const auto offBottomDelta = srNewViewport.bottom - (coordScreenBufferSize.height - 1);
    if (offBottomDelta > 0) // the viewport was off the right of the buffer...
    {
        // ...so slide both top/bottom back into the buffer. This will prevent us
        // from having a negative width later.
        srNewViewport.bottom -= offBottomDelta;
        srNewViewport.top = std::max(0, srNewViewport.top - offBottomDelta);
    }

    // In general we want to avoid moving the virtual bottom unless it's aligned with
    // the visible viewport, so we check whether the changes we're making would cause
    // the bottom of the visible viewport to intersect the virtual bottom at any point.
    // If so, we update the virtual bottom to match. We also update the virtual bottom
    // if it's less than the new viewport height minus 1, because that would otherwise
    // leave the virtual viewport extended past the top of the buffer.
    const auto newViewport = Viewport::FromInclusive(srNewViewport);
    if ((_virtualBottom >= _viewport.BottomInclusive() && _virtualBottom < newViewport.BottomInclusive()) ||
        (_virtualBottom <= _viewport.BottomInclusive() && _virtualBottom > newViewport.BottomInclusive()) ||
        _virtualBottom < newViewport.Height() - 1)
    {
        _virtualBottom = srNewViewport.bottom;
    }

    _viewport = newViewport;
    Tracing::s_TraceWindowViewport(_viewport);

    // In Conpty mode, call TriggerScroll here without params. By not providing
    // params, the renderer will make sure to update the VtEngine with the
    // updated viewport size. If we don't do this, the engine can get into a
    // torn state on this frame.
    //
    // Without this statement, the engine won't be told about the new view size
    // till the start of the next frame. If any other text gets output before
    // that frame starts, there's a very real chance that it'll cause errors as
    // the engine tries to invalidate those regions.
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (gci.IsInVtIoMode() && ServiceLocator::LocateGlobals().pRender)
    {
        ServiceLocator::LocateGlobals().pRender->TriggerScroll();
    }
}

// Routine Description:
// - Modifies the size of the current viewport to match the width/height of the request given.
// - Uses the old and new client areas to determine which side the window was resized from.
// Arguments:
// - prcClientNew - Client rectangle in pixels after this update
// - prcClientOld - Client rectangle in pixels before this update
// - pcoordSize - Requested viewport width/heights in characters
// Return Value:
// - <none>
void SCREEN_INFORMATION::_AdjustViewportSize(const til::rect* const prcClientNew,
                                             const til::rect* const prcClientOld,
                                             const til::size* const pcoordSize)
{
    // If the left is the only one that changed (and not the right
    // also), then adjust from the left. Otherwise if the right
    // changes or both changed, bias toward leaving the top-left
    // corner in place and resize from the bottom right.
    // --
    // Resizing from the bottom right is more expected by
    // users. Normally only one dimension (or one corner) will change
    // at a time if the user is moving it. However, if the window is
    // being dragged and forced to resize at a monitor boundary, all 4
    // will change. In this case especially, users expect the top left
    // to stay in place and the bottom right to adapt.
    const auto fResizeFromLeft = prcClientNew->left != prcClientOld->left &&
                                 prcClientNew->right == prcClientOld->right;
    const auto fResizeFromTop = prcClientNew->top != prcClientOld->top &&
                                prcClientNew->bottom == prcClientOld->bottom;

    const auto oldViewport = Viewport(_viewport);

    _InternalSetViewportSize(pcoordSize, fResizeFromTop, fResizeFromLeft);

    // MSFT 13194969, related to 12092729.
    // If we're in virtual terminal mode, and the viewport dimensions change,
    //      send a WindowBufferSizeEvent. If the client wants VT mode, then they
    //      probably want the viewport resizes, not just the screen buffer
    //      resizes. This does change the behavior of the API for v2 callers,
    //      but only callers who've requested VT mode. In 12092729, we enabled
    //      sending notifications from window resizes in cases where the buffer
    //      didn't resize, so this applies the same expansion to resizes using
    //      the window, not the API.
    if (IsInVirtualTerminalInputMode())
    {
        if ((_viewport.Width() != oldViewport.Width()) ||
            (_viewport.Height() != oldViewport.Height()))
        {
            ScreenBufferSizeChange(GetBufferSize().Dimensions());
        }
    }
}

// Routine Description:
// - From a window client area in pixels, a buffer size, and the font size, this will determine
//   whether scroll bars will need to be shown (and consume a portion of the client area) for the
//   given buffer to be rendered.
// Arguments:
// - prcClientArea - Client area in pixels of the available space for rendering
// - pcoordBufferSize - Buffer size in characters
// - pcoordFontSize - Font size in pixels per character
// - pfIsHorizontalVisible - Indicates whether the horizontal scroll
//   bar (consuming vertical space) will need to be visible
// - pfIsVerticalVisible - Indicates whether the vertical scroll bar
//   (consuming horizontal space) will need to be visible
// Return Value:
// - <none>
void SCREEN_INFORMATION::s_CalculateScrollbarVisibility(const til::rect* const prcClientArea,
                                                        const til::size* const pcoordBufferSize,
                                                        const til::size* const pcoordFontSize,
                                                        _Out_ bool* const pfIsHorizontalVisible,
                                                        _Out_ bool* const pfIsVerticalVisible)
{
    // Start with bars not visible as the initial state of the client area doesn't account for scroll bars.
    *pfIsHorizontalVisible = false;
    *pfIsVerticalVisible = false;

    // Set up the client area in pixels
    til::size sizeClientPixels;
    sizeClientPixels.width = prcClientArea->width();
    sizeClientPixels.height = prcClientArea->height();

    // Set up the buffer area in pixels by multiplying the size by the font size scale factor
    til::size sizeBufferPixels;
    sizeBufferPixels.width = pcoordBufferSize->width * pcoordFontSize->width;
    sizeBufferPixels.height = pcoordBufferSize->height * pcoordFontSize->height;

    // Now figure out whether we need one or both scroll bars.
    // Showing a scroll bar in one direction may necessitate showing
    // the scroll bar in the other (as it will consume client area
    // space).

    if (sizeBufferPixels.width > sizeClientPixels.width)
    {
        *pfIsHorizontalVisible = true;

        // If we have a horizontal bar, remove it from available
        // vertical space and check that remaining client area is
        // enough.
        sizeClientPixels.height -= ServiceLocator::LocateGlobals().sHorizontalScrollSize;

        if (sizeBufferPixels.height > sizeClientPixels.height)
        {
            *pfIsVerticalVisible = true;
        }
    }
    else if (sizeBufferPixels.height > sizeClientPixels.height)
    {
        *pfIsVerticalVisible = true;

        // If we have a vertical bar, remove it from available
        // horizontal space and check that remaining client area is
        // enough.
        sizeClientPixels.width -= ServiceLocator::LocateGlobals().sVerticalScrollSize;

        if (sizeBufferPixels.width > sizeClientPixels.width)
        {
            *pfIsHorizontalVisible = true;
        }
    }
}

bool SCREEN_INFORMATION::IsMaximizedBoth() const
{
    return IsMaximizedX() && IsMaximizedY();
}

bool SCREEN_INFORMATION::IsMaximizedX() const
{
    // If the viewport is displaying the entire size of the allocated buffer, it's maximized.
    return _viewport.Left() == 0 && (_viewport.Width() == GetBufferSize().Width());
}

bool SCREEN_INFORMATION::IsMaximizedY() const
{
    // If the viewport is displaying the entire size of the allocated buffer, it's maximized.
    return _viewport.Top() == 0 && (_viewport.Height() == GetBufferSize().Height());
}

#pragma endregion

// Routine Description:
// - This is a screen resize algorithm which will reflow the ends of lines based on the
//   line wrap state used for clipboard line-based copy.
// Arguments:
// - <in> Coordinates of the new screen size
// Return Value:
// - Success if successful. Invalid parameter if screen buffer size is unexpected. No memory if allocation failed.
[[nodiscard]] NTSTATUS SCREEN_INFORMATION::ResizeWithReflow(const til::size coordNewScreenSize)
{
    if ((USHORT)coordNewScreenSize.width >= SHORT_MAX || (USHORT)coordNewScreenSize.height >= SHORT_MAX)
    {
        RIPMSG2(RIP_WARNING, "Invalid screen buffer size (0x%x, 0x%x)", coordNewScreenSize.width, coordNewScreenSize.height);
        return STATUS_INVALID_PARAMETER;
    }

    // First allocate a new text buffer to take the place of the current one.
    std::unique_ptr<TextBuffer> newTextBuffer;

    // GH#3848 - Stash away the current attributes the old text buffer is using.
    // We'll initialize the new buffer with the default attributes, but after
    // the resize, we'll want to make sure that the new buffer's current
    // attributes (the ones used for printing new text) match the old buffer's.
    const auto oldPrimaryAttributes = _textBuffer->GetCurrentAttributes();
    try
    {
        newTextBuffer = std::make_unique<TextBuffer>(coordNewScreenSize,
                                                     TextAttribute{},
                                                     0, // temporarily set size to 0 so it won't render.
                                                     _textBuffer->IsActiveBuffer(),
                                                     _textBuffer->GetRenderer());
    }
    catch (...)
    {
        return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
    }

    // Save cursor's relative height versus the viewport
    const auto sCursorHeightInViewportBefore = _textBuffer->GetCursor().GetPosition().y - _viewport.Top();
    // Also save the distance to the virtual bottom so it can be restored after the resize
    const auto cursorDistanceFromBottom = _virtualBottom - _textBuffer->GetCursor().GetPosition().y;

    // skip any drawing updates that might occur until we swap _textBuffer with the new buffer or we exit early.
    newTextBuffer->GetCursor().StartDeferDrawing();
    _textBuffer->GetCursor().StartDeferDrawing();
    // we're capturing _textBuffer by reference here because when we exit, we want to EndDefer on the current active buffer.
    auto endDefer = wil::scope_exit([&]() noexcept { _textBuffer->GetCursor().EndDeferDrawing(); });

    auto hr = TextBuffer::Reflow(*_textBuffer.get(), *newTextBuffer.get(), std::nullopt, std::nullopt);

    if (SUCCEEDED(hr))
    {
        // Since the reflow doesn't preserve the virtual bottom, we try and
        // estimate where it ought to be by making it the same distance from
        // the cursor row as it was before the resize. However, we also need
        // to make sure it is far enough down to include the last non-space
        // row, and it shouldn't be less than the height of the viewport,
        // otherwise the top of the virtual viewport would end up negative.
        const auto cursorRow = newTextBuffer->GetCursor().GetPosition().y;
        const auto lastNonSpaceRow = newTextBuffer->GetLastNonSpaceCharacter().y;
        const auto estimatedBottom = cursorRow + cursorDistanceFromBottom;
        const auto viewportBottom = _viewport.Height() - 1;
        _virtualBottom = std::max({ lastNonSpaceRow, estimatedBottom, viewportBottom });

        // We can't let it extend past the bottom of the buffer either.
        _virtualBottom = std::min(_virtualBottom, newTextBuffer->GetSize().BottomInclusive());

        // Adjust the viewport so the cursor doesn't wildly fly off up or down.
        const auto sCursorHeightInViewportAfter = cursorRow - _viewport.Top();
        til::point coordCursorHeightDiff;
        coordCursorHeightDiff.y = sCursorHeightInViewportAfter - sCursorHeightInViewportBefore;
        LOG_IF_FAILED(SetViewportOrigin(false, coordCursorHeightDiff, false));

        newTextBuffer->SetCurrentAttributes(oldPrimaryAttributes);

        _textBuffer.swap(newTextBuffer);
    }

    return NTSTATUS_FROM_HRESULT(hr);
}

//
// Routine Description:
// - This is the legacy screen resize with minimal changes
// Arguments:
// - coordNewScreenSize - new size of screen.
// Return Value:
// - Success if successful. Invalid parameter if screen buffer size is unexpected. No memory if allocation failed.
[[nodiscard]] NTSTATUS SCREEN_INFORMATION::ResizeTraditional(const til::size coordNewScreenSize)
{
    _textBuffer->GetCursor().StartDeferDrawing();
    auto endDefer = wil::scope_exit([&]() noexcept { _textBuffer->GetCursor().EndDeferDrawing(); });
    return NTSTATUS_FROM_HRESULT(_textBuffer->ResizeTraditional(coordNewScreenSize));
}

//
// Routine Description:
// - This routine resizes the screen buffer.
// Arguments:
// - NewScreenSize - new size of screen in characters
// - DoScrollBarUpdate - indicates whether to update scroll bars at the end
// Return Value:
// - Success if successful. Invalid parameter if screen buffer size is unexpected. No memory if allocation failed.
[[nodiscard]] NTSTATUS SCREEN_INFORMATION::ResizeScreenBuffer(const til::size coordNewScreenSize,
                                                              const bool fDoScrollBarUpdate)
{
    // If the size hasn't actually changed, do nothing.
    if (coordNewScreenSize == GetBufferSize().Dimensions())
    {
        return STATUS_SUCCESS;
    }

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto status = STATUS_SUCCESS;

    // If we're in conpty mode, suppress any immediate painting we might do
    // during the resize.
    if (gci.IsInVtIoMode())
    {
        gci.GetVtIo()->BeginResize();
    }
    auto endResize = wil::scope_exit([&] {
        if (gci.IsInVtIoMode())
        {
            gci.GetVtIo()->EndResize();
        }
    });

    // cancel any active selection before resizing or it will not necessarily line up with the new buffer positions
    Selection::Instance().ClearSelection();

    // cancel any popups before resizing or they will not necessarily line up with new buffer positions
    CommandLine::Instance().EndAllPopups();

    const auto fWrapText = gci.GetWrapText();
    // GH#3493: Don't reflow the alt buffer.
    //
    // VTE only rewraps the contents of the (normal screen + its scrollback
    // buffer) on a resize event. It doesn't rewrap the contents of the
    // alternate screen. The alternate screen is used by applications which
    // repaint it after a resize event. So it doesn't really matter. However, in
    // that short time window, after resizing the terminal but before the
    // application catches up, this prevents vertical lines
    if (fWrapText && !_IsAltBuffer())
    {
        status = ResizeWithReflow(coordNewScreenSize);
    }
    else
    {
        status = NTSTATUS_FROM_HRESULT(ResizeTraditional(coordNewScreenSize));
    }

    if (SUCCEEDED_NTSTATUS(status))
    {
        if (HasAccessibilityEventing())
        {
            NotifyAccessibilityEventing(0, 0, coordNewScreenSize.width - 1, coordNewScreenSize.height - 1);
        }

        if ((!ConvScreenInfo))
        {
            if (FAILED(ConsoleImeResizeCompStrScreenBuffer(coordNewScreenSize)))
            {
                // If something went wrong, just bail out.
                return STATUS_INVALID_HANDLE;
            }
        }

        // Fire off an event to let accessibility apps know the layout has changed.
        if (_pAccessibilityNotifier && IsActiveScreenBuffer())
        {
            _pAccessibilityNotifier->NotifyConsoleLayoutEvent();
        }

        if (fDoScrollBarUpdate)
        {
            UpdateScrollBars();
        }
        ScreenBufferSizeChange(coordNewScreenSize);
    }

    return status;
}

// Routine Description:
// - Given a rectangle containing screen buffer coordinates (character-level positioning, not pixel)
//   This method will trim the rectangle to ensure it is within the buffer.
//   For example, if the rectangle given has a right position of 85, but the current screen buffer
//   is only reaching from 0-79, then the right position will be set to 79.
// Arguments:
// - psrRect - Pointer to rectangle holding data to be trimmed
// Return Value:
// - <none>
void SCREEN_INFORMATION::ClipToScreenBuffer(_Inout_ til::inclusive_rect* const psrClip) const
{
    const auto bufferSize = GetBufferSize();

    psrClip->left = std::max(psrClip->left, bufferSize.Left());
    psrClip->top = std::max(psrClip->top, bufferSize.Top());
    psrClip->right = std::min(psrClip->right, bufferSize.RightInclusive());
    psrClip->bottom = std::min(psrClip->bottom, bufferSize.BottomInclusive());
}

void SCREEN_INFORMATION::MakeCurrentCursorVisible()
{
    MakeCursorVisible(_textBuffer->GetCursor().GetPosition());
}

// Routine Description:
// - This routine sets the cursor size and visibility both in the data
//      structures and on the screen. Also updates the cursor information of
//      this buffer's main buffer, if this buffer is an alt buffer.
// Arguments:
// - Size - cursor size
// - Visible - cursor visibility
// Return Value:
// - None
void SCREEN_INFORMATION::SetCursorInformation(const ULONG Size,
                                              const bool Visible) noexcept
{
    auto& cursor = _textBuffer->GetCursor();
    const auto originalSize = cursor.GetSize();

    cursor.SetSize(Size);
    cursor.SetIsVisible(Visible);

    // If we are just trying to change the visibility, we don't want to reset
    // the cursor type. We only need to force it to the Legacy style if the
    // size is actually being changed.
    if (Size != originalSize)
    {
        cursor.SetType(CursorType::Legacy);
    }

    // If we're an alt buffer, also update our main buffer.
    // Users of the API expect both to be set - this can't be set by VT
    if (_psiMainBuffer)
    {
        _psiMainBuffer->SetCursorInformation(Size, Visible);
    }
}

// Routine Description:
// - This routine sets the cursor shape both in the data
//      structures and on the screen. Also updates the cursor information of
//      this buffer's main buffer, if this buffer is an alt buffer.
// Arguments:
// - Type - The new shape to set the cursor to
// - setMain - If true, propagate change to main buffer as well.
// Return Value:
// - None
void SCREEN_INFORMATION::SetCursorType(const CursorType Type, const bool setMain) noexcept
{
    auto& cursor = _textBuffer->GetCursor();

    cursor.SetType(Type);

    // If we're an alt buffer, DON'T propagate this setting up to the main buffer.
    // We don't want to pollute that buffer with this state,
    // UNLESS we're getting called from the propsheet, then we DO want to update this.
    if (_psiMainBuffer && setMain)
    {
        _psiMainBuffer->SetCursorType(Type);
    }
}

// Routine Description:
// - This routine sets a flag saying whether the cursor should be displayed
//   with its default size or it should be modified to indicate the
//   insert/overtype mode has changed.
// Arguments:
// - ScreenInfo - pointer to screen info structure.
// - DoubleCursor - should we indicated non-normal mode
// Return Value:
// - None
void SCREEN_INFORMATION::SetCursorDBMode(const bool DoubleCursor)
{
    auto& cursor = _textBuffer->GetCursor();

    if ((cursor.IsDouble() != DoubleCursor))
    {
        cursor.SetIsDouble(DoubleCursor);
    }

    // If we're an alt buffer, also update our main buffer.
    if (_psiMainBuffer)
    {
        _psiMainBuffer->SetCursorDBMode(DoubleCursor);
    }
}

// Routine Description:
// - This routine sets the cursor position in the data structures and on the screen.
// Arguments:
// - ScreenInfo - pointer to screen info structure.
// - Position - new position of cursor
// - TurnOn - true if cursor should be left on, false if should be left off
// Return Value:
// - Status
[[nodiscard]] NTSTATUS SCREEN_INFORMATION::SetCursorPosition(const til::point Position, const bool TurnOn)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& cursor = _textBuffer->GetCursor();

    //
    // Ensure that the cursor position is within the constraints of the screen
    // buffer.
    //
    const auto coordScreenBufferSize = GetBufferSize().Dimensions();
    if (Position.x >= coordScreenBufferSize.width || Position.y >= coordScreenBufferSize.height || Position.x < 0 || Position.y < 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // In GH#5291, we experimented with manually breaking the line on all cursor
    // movements here. As we print lines into the buffer, we mark lines as
    // wrapped when we print the last cell of the row, not the first cell of the
    // subsequent row (the row the first line wrapped onto).
    //
    // Logically, we thought that manually breaking lines when we move the
    // cursor was a good idea. We however, did not have the time to fully
    // validate that this was the correct answer, and a simpler solution for the
    // bug on hand was found. Furthermore, we thought it would be a more
    // comprehensive solution to only mark lines as wrapped when we print the
    // first cell of the second row, which would require some WriteCharsLegacy
    // work.

    cursor.SetPosition(Position);

    // If the cursor has moved below the virtual bottom, the bottom should be updated.
    if (Position.y > _virtualBottom)
    {
        _virtualBottom = Position.y;
    }

    // if we have the focus, adjust the cursor state
    if (gci.Flags & CONSOLE_HAS_FOCUS)
    {
        if (TurnOn)
        {
            cursor.SetDelay(false);
            cursor.SetIsOn(true);
        }
        else
        {
            cursor.SetDelay(true);
        }
        cursor.SetHasMoved(true);
    }

    return STATUS_SUCCESS;
}

void SCREEN_INFORMATION::MakeCursorVisible(const til::point CursorPosition)
{
    til::point WindowOrigin;

    if (CursorPosition.x > _viewport.RightInclusive())
    {
        WindowOrigin.x = CursorPosition.x - _viewport.RightInclusive();
    }
    else if (CursorPosition.x < _viewport.Left())
    {
        WindowOrigin.x = CursorPosition.x - _viewport.Left();
    }
    else
    {
        WindowOrigin.x = 0;
    }

    if (CursorPosition.y > _viewport.BottomInclusive())
    {
        WindowOrigin.y = CursorPosition.y - _viewport.BottomInclusive();
    }
    else if (CursorPosition.y < _viewport.Top())
    {
        WindowOrigin.y = CursorPosition.y - _viewport.Top();
    }
    else
    {
        WindowOrigin.y = 0;
    }

    if (WindowOrigin.x != 0 || WindowOrigin.y != 0)
    {
        LOG_IF_FAILED(SetViewportOrigin(false, WindowOrigin, false));
    }
}

// Routine Description:
// - Retrieves the active buffer of this buffer. If this buffer has an
//     alternate buffer, this is the alternate buffer. Otherwise, it is this buffer.
// Parameters:
// - None
// Return value:
// - a reference to this buffer's active buffer.
SCREEN_INFORMATION& SCREEN_INFORMATION::GetActiveBuffer()
{
    return const_cast<SCREEN_INFORMATION&>(static_cast<const SCREEN_INFORMATION* const>(this)->GetActiveBuffer());
}

const SCREEN_INFORMATION& SCREEN_INFORMATION::GetActiveBuffer() const
{
    if (_psiAlternateBuffer != nullptr)
    {
        return *_psiAlternateBuffer;
    }
    return *this;
}

// Routine Description:
// - Retrieves the main buffer of this buffer. If this buffer has an
//     alternate buffer, this is the main buffer. Otherwise, it is this buffer's main buffer.
//     The main buffer is not necessarily the active buffer.
// Parameters:
// - None
// Return value:
// - a reference to this buffer's main buffer.
SCREEN_INFORMATION& SCREEN_INFORMATION::GetMainBuffer()
{
    return const_cast<SCREEN_INFORMATION&>(static_cast<const SCREEN_INFORMATION* const>(this)->GetMainBuffer());
}

const SCREEN_INFORMATION& SCREEN_INFORMATION::GetMainBuffer() const
{
    if (_psiMainBuffer != nullptr)
    {
        return *_psiMainBuffer;
    }
    return *this;
}

// Routine Description:
// - Instantiates a new buffer to be used as an alternate buffer. This buffer
//     does not have a driver handle associated with it and shares a state
//     machine with the main buffer it belongs to.
// TODO: MSFT:19817348 Don't create alt screenbuffer's via an out SCREEN_INFORMATION**
// Parameters:
// - initAttributes - the attributes the buffer is initialized with.
// - ppsiNewScreenBuffer - a pointer to receive the newly created buffer.
// Return value:
// - STATUS_SUCCESS if handled successfully. Otherwise, an appropriate status code indicating the error.
[[nodiscard]] NTSTATUS SCREEN_INFORMATION::_CreateAltBuffer(const TextAttribute& initAttributes, _Out_ SCREEN_INFORMATION** const ppsiNewScreenBuffer)
{
    // Create new screen buffer.
    auto WindowSize = _viewport.Dimensions();

    const auto& existingFont = GetCurrentFont();

    auto Status = SCREEN_INFORMATION::CreateInstance(WindowSize,
                                                     existingFont,
                                                     WindowSize,
                                                     initAttributes,
                                                     GetPopupAttributes(),
                                                     Cursor::CURSOR_SMALL_SIZE,
                                                     ppsiNewScreenBuffer);
    if (SUCCEEDED_NTSTATUS(Status))
    {
        // Update the alt buffer's cursor style, visibility, and position to match our own.
        auto& myCursor = GetTextBuffer().GetCursor();
        auto* const createdBuffer = *ppsiNewScreenBuffer;
        auto& altCursor = createdBuffer->GetTextBuffer().GetCursor();
        altCursor.SetStyle(myCursor.GetSize(), myCursor.GetType());
        altCursor.SetIsVisible(myCursor.IsVisible());
        altCursor.SetBlinkingAllowed(myCursor.IsBlinkingAllowed());
        // The new position should match the viewport-relative position of the main buffer.
        auto altCursorPos = myCursor.GetPosition();
        altCursorPos.y -= GetVirtualViewport().Top();
        altCursor.SetPosition(altCursorPos);
        // The alt buffer's output mode should match the main buffer.
        createdBuffer->OutputMode = OutputMode;

        s_InsertScreenBuffer(createdBuffer);

        // delete the alt buffer's state machine. We don't want it.
        createdBuffer->_FreeOutputStateMachine(); // this has to be done before we give it a main buffer
        // we'll attach the GetSet, etc once we successfully make this buffer the active buffer.

        // Set up the new buffers references to our current state machine, dispatcher, getset, etc.
        createdBuffer->_stateMachine = _stateMachine;
    }
    return Status;
}

// Function Description:
// - Handle deferred resizes that may have happened while the alt buffer was
//   active. Both resizes on the HWND itself (_fAltWindowChanged), and resizes
//   to the viewport of the alt buffer (which in turn should resize the buffer),
//   are handled here.
// Arguments:
// - siMain: the main buffer whose resize was deferred.
// Return Value:
// - <none>
void SCREEN_INFORMATION::_handleDeferredResize(SCREEN_INFORMATION& siMain)
{
    if (siMain._fAltWindowChanged)
    {
        siMain.ProcessResizeWindow(&(siMain._rcAltSavedClientNew), &(siMain._rcAltSavedClientOld));
        siMain._fAltWindowChanged = false;
    }
    else if (siMain._deferredPtyResize.has_value())
    {
        const auto newViewSize = siMain._deferredPtyResize.value();
        // Tricky! We want to make sure to resize the actual main buffer
        // here, not just change the size of the viewport. When they resized
        // the alt buffer, the dimensions of the alt buffer changed. We
        // should make sure the main buffer reflects similar changes.
        //
        // To do this, we have to emulate bits of
        // ConhostInternalGetSet::ResizeWindow. We can't call that
        // straight-up, because the main buffer isn't active yet.
        const auto oldScreenBufferSize = siMain.GetBufferSize().Dimensions();
        auto newBufferSize = oldScreenBufferSize;

        // Always resize the width of the console
        newBufferSize.width = newViewSize.width;

        // Only set the new buffer's height if the new size will be TALLER than the current size (e.g., resizing a 80x32 buffer to be 80x124).
        if (newViewSize.height > oldScreenBufferSize.height)
        {
            newBufferSize.height = newViewSize.height;
        }

        // From ApiRoutines::SetConsoleScreenBufferInfoExImpl. We don't need
        // the whole call to SetConsoleScreenBufferInfoEx here, that's
        // too much work.
        if (newBufferSize != oldScreenBufferSize)
        {
            auto& commandLine = CommandLine::Instance();
            commandLine.Hide(FALSE);
            LOG_IF_FAILED(siMain.ResizeScreenBuffer(newBufferSize, TRUE));
            commandLine.Show();
        }

        // Not that the buffer is smaller, actually make sure to resize our
        // viewport to match.
        siMain.SetViewportSize(&newViewSize);

        // Clear out the resize.
        siMain._deferredPtyResize = std::nullopt;
    }
}

// Routine Description:
// - Creates an "alternate" screen buffer for this buffer. In virtual terminals, there exists both a "main"
//     screen buffer and an alternate. ASBSET creates a new alternate, and switches to it. If there is an already
//     existing alternate, it is discarded. This allows applications to retain one HANDLE, and switch which buffer it points to seamlessly.
// Parameters:
// - initAttributes - the attributes the buffer is initialized with.
// Return value:
// - STATUS_SUCCESS if handled successfully. Otherwise, an appropriate status code indicating the error.
[[nodiscard]] NTSTATUS SCREEN_INFORMATION::UseAlternateScreenBuffer(const TextAttribute& initAttributes)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& siMain = GetMainBuffer();
    // If we're in an alt that resized, resize the main before making the new alt
    _handleDeferredResize(siMain);

    SCREEN_INFORMATION* psiNewAltBuffer;
    auto Status = _CreateAltBuffer(initAttributes, &psiNewAltBuffer);
    if (SUCCEEDED_NTSTATUS(Status))
    {
        // if this is already an alternate buffer, we want to make the new
        // buffer the alt on our main buffer, not on ourself, because there
        // can only ever be one main and one alternate.
        const auto psiOldAltBuffer = siMain._psiAlternateBuffer;

        psiNewAltBuffer->_psiMainBuffer = &siMain;
        siMain._psiAlternateBuffer = psiNewAltBuffer;

        if (psiOldAltBuffer != nullptr)
        {
            s_RemoveScreenBuffer(psiOldAltBuffer); // this will also delete the old alt buffer
        }

        // GH#381: When we switch into the alt buffer:
        //  * flush the current frame, to clear out anything that we prepared for this buffer.
        //  * Emit a ?1049h/l to the remote side, to let them know that we've switched buffers.
        if (gci.IsInVtIoMode() && ServiceLocator::LocateGlobals().pRender)
        {
            ServiceLocator::LocateGlobals().pRender->TriggerFlush(false);
            LOG_IF_FAILED(gci.GetVtIo()->SwitchScreenBuffer(true));
        }

        ::SetActiveScreenBuffer(*psiNewAltBuffer);

        // Kind of a hack until we have proper signal channels: If the client app wants window size events, send one for
        // the new alt buffer's size (this is so WSL can update the TTY size when the MainSB.viewportWidth <
        // MainSB.bufferWidth (which can happen with wrap text disabled))
        ScreenBufferSizeChange(psiNewAltBuffer->GetBufferSize().Dimensions());

        // Tell the VT MouseInput handler that we're in the Alt buffer now
        gci.GetActiveInputBuffer()->GetTerminalInput().UseAlternateScreenBuffer();
    }
    return Status;
}

// Routine Description:
// - Restores the active buffer to be this buffer's main buffer. If this is the main buffer, then nothing happens.
// Parameters:
// - None
// Return value:
// - STATUS_SUCCESS if handled successfully. Otherwise, an appropriate status code indicating the error.
void SCREEN_INFORMATION::UseMainScreenBuffer()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto psiMain = _psiMainBuffer;
    if (psiMain != nullptr)
    {
        _handleDeferredResize(*psiMain);

        // GH#381: When we switch into the main buffer:
        //  * flush the current frame, to clear out anything that we prepared for this buffer.
        //  * Emit a ?1049h/l to the remote side, to let them know that we've switched buffers.
        if (gci.IsInVtIoMode() && ServiceLocator::LocateGlobals().pRender)
        {
            ServiceLocator::LocateGlobals().pRender->TriggerFlush(false);
            LOG_IF_FAILED(gci.GetVtIo()->SwitchScreenBuffer(false));
        }

        ::SetActiveScreenBuffer(*psiMain);
        psiMain->UpdateScrollBars(); // The alt had disabled scrollbars, re-enable them

        // send a _coordScreenBufferSizeChangeEvent for the new Sb viewport
        ScreenBufferSizeChange(psiMain->GetBufferSize().Dimensions());

        auto psiAlt = psiMain->_psiAlternateBuffer;
        psiMain->_psiAlternateBuffer = nullptr;

        // Copy the alt buffer's cursor style and visibility back to the main buffer.
        const auto& altCursor = psiAlt->GetTextBuffer().GetCursor();
        auto& mainCursor = psiMain->GetTextBuffer().GetCursor();
        mainCursor.SetStyle(altCursor.GetSize(), altCursor.GetType());
        mainCursor.SetIsVisible(altCursor.IsVisible());
        mainCursor.SetBlinkingAllowed(altCursor.IsBlinkingAllowed());

        // Copy the alt buffer's output mode back to the main buffer.
        psiMain->OutputMode = psiAlt->OutputMode;

        s_RemoveScreenBuffer(psiAlt); // this will also delete the alt buffer
        // deleting the alt buffer will give the GetSet back to its main

        // Tell the VT MouseInput handler that we're in the main buffer now
        gci.GetActiveInputBuffer()->GetTerminalInput().UseMainScreenBuffer();
    }
}

// Routine Description:
// - Helper indicating if the buffer has a main buffer, meaning that this is an alternate buffer.
// Parameters:
// - None
// Return value:
// - true iff this buffer has a main buffer.
bool SCREEN_INFORMATION::_IsAltBuffer() const
{
    return _psiMainBuffer != nullptr;
}

// Routine Description:
// - Helper indicating if the buffer is acting as a pty - with the screenbuffer
//      clamped to the viewport size. This can be the case either when we're in
//      VT I/O mode, or when this buffer is an alt buffer.
// Parameters:
// - None
// Return value:
// - true iff this buffer has a main buffer.
bool SCREEN_INFORMATION::_IsInPtyMode() const
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return _IsAltBuffer() || gci.IsInVtIoMode();
}

// Routine Description:
// - returns true if this buffer is in Virtual Terminal Output mode.
// Parameters:
// - None
// Return Value:
// - true iff this buffer is in Virtual Terminal Output mode.
bool SCREEN_INFORMATION::_IsInVTMode() const
{
    return WI_IsFlagSet(OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

// Routine Description:
// - Returns the value of the attributes
// Parameters:
// <none>
// Return value:
// - This screen buffer's attributes
TextAttribute SCREEN_INFORMATION::GetAttributes() const
{
    return _textBuffer->GetCurrentAttributes();
}

// Routine Description:
// - Returns the value of the popup attributes
// Parameters:
// <none>
// Return value:
// - This screen buffer's popup attributes
TextAttribute SCREEN_INFORMATION::GetPopupAttributes() const
{
    return _PopupAttributes;
}

// Routine Description:
// - Sets the value of the attributes on this screen buffer. Also propagates
//     the change down to the fill of the text buffer attached to this screen buffer.
// Parameters:
// - attributes - The new value of the attributes to use.
// Return value:
// <none>
void SCREEN_INFORMATION::SetAttributes(const TextAttribute& attributes)
{
    if (_ignoreLegacyEquivalentVTAttributes)
    {
        // See the comment on StripErroneousVT16VersionsOfLegacyDefaults for more info.
        _textBuffer->SetCurrentAttributes(TextAttribute::StripErroneousVT16VersionsOfLegacyDefaults(attributes));
        return;
    }

    _textBuffer->SetCurrentAttributes(attributes);

    // If we're an alt buffer, DON'T propagate this setting up to the main buffer.
    // We don't want to pollute that buffer with this state.
}

// Method Description:
// - Sets the value of the popup attributes on this screen buffer.
// Parameters:
// - popupAttributes - The new value of the popup attributes to use.
// Return value:
// <none>
void SCREEN_INFORMATION::SetPopupAttributes(const TextAttribute& popupAttributes)
{
    _PopupAttributes = popupAttributes;

    // If we're an alt buffer, DON'T propagate this setting up to the main buffer.
    // We don't want to pollute that buffer with this state.
}

// Method Description:
// - Sets the value of the attributes on this screen buffer. Also propagates
//     the change down to the fill of the attached text buffer.
// - Additionally updates any popups to match the new color scheme.
// - Also updates the defaults of the main buffer. This method is called by the
//      propsheet menu when you set the colors via the propsheet. In that
//      workflow, we want the main buffer's colors changed as well as our own.
// Parameters:
// - attributes - The new value of the attributes to use.
// - popupAttributes - The new value of the popup attributes to use.
// Return value:
// <none>
// Notes:
// This code is merged from the old global function SetScreenColors
void SCREEN_INFORMATION::SetDefaultAttributes(const TextAttribute& attributes,
                                              const TextAttribute& popupAttributes)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    const auto oldPrimaryAttributes = GetAttributes();
    const auto oldPopupAttributes = GetPopupAttributes();

    // Quick return if we don't need to do anything.
    if (oldPrimaryAttributes == attributes && oldPopupAttributes == popupAttributes)
    {
        return;
    }

    SetAttributes(attributes);
    SetPopupAttributes(popupAttributes);

    // Force repaint of entire viewport, unless we're in conpty mode. In that
    // case, we don't really need to force a redraw of the entire screen just
    // because the text attributes changed.
    if (!(gci.IsInVtIoMode()))
    {
        _textBuffer->TriggerRedrawAll();
    }

    gci.ConsoleIme.RefreshAreaAttributes();

    // If we're an alt buffer, also update our main buffer.
    if (_psiMainBuffer)
    {
        _psiMainBuffer->SetDefaultAttributes(attributes, popupAttributes);
    }
}

// Method Description:
// - Returns an inclusive rectangle that describes the bounds of the buffer viewport.
// Arguments:
// - <none>
// Return Value:
// - the viewport bounds as an inclusive rect.
const Viewport& SCREEN_INFORMATION::GetViewport() const noexcept
{
    return _viewport;
}

// Routine Description:
// - This routine updates the size of the rectangle representing the viewport into the text buffer.
// - It is specified in character count within the buffer.
// - It will be corrected to not exceed the limits of the current screen buffer dimensions.
// Arguments:
// newViewport: The new viewport to use. If it's out of bounds in the negative
//      direction it will be shifted to positive coordinates. If it's bigger
//      that the screen buffer, it will be clamped to the size of the buffer.
// updateBottom: if true, update our virtual bottom. This should be false when
//      called from UX interactions, such as scrolling with the mouse wheel,
//      and true when called from API endpoints, such as SetConsoleWindowInfo
// Return Value:
// - None
void SCREEN_INFORMATION::SetViewport(const Viewport& newViewport,
                                     const bool updateBottom)
{
    // make sure there's something to do
    if (newViewport == _viewport)
    {
        return;
    }

    // do adjustments on a copy that's easily manipulated.
    const til::rect viewportRect{ newViewport.ToInclusive() };
    const til::size coordScreenBufferSize{ GetBufferSize().Dimensions() };

    // MSFT-33471786, GH#13193:
    // newViewport may reside anywhere outside of the valid coordScreenBufferSize.
    // For instance it might be scrolled down more than our text buffer allows to be scrolled.
    const auto cx = gsl::narrow_cast<SHORT>(std::clamp(viewportRect.width(), 1, coordScreenBufferSize.width));
    const auto cy = gsl::narrow_cast<SHORT>(std::clamp(viewportRect.height(), 1, coordScreenBufferSize.height));
    const auto x = gsl::narrow_cast<SHORT>(std::clamp(viewportRect.left, 0, coordScreenBufferSize.width - cx));
    const auto y = gsl::narrow_cast<SHORT>(std::clamp(viewportRect.top, 0, coordScreenBufferSize.height - cy));

    _viewport = Viewport::FromExclusive({ x, y, x + cx, y + cy });

    if (updateBottom)
    {
        UpdateBottom();
    }

    Tracing::s_TraceWindowViewport(_viewport);
}

// Method Description:
// - Clear the entire contents of the viewport, except for the cursor's row,
//   which is moved to the top line of the viewport.
// - This is used exclusively by ConPTY to support GH#1193, GH#1882. This allows
//   a terminal to clear the contents of the ConPTY buffer, which is important
//   if the user would like to be able to clear the terminal-side buffer.
// Arguments:
// - <none>
// Return Value:
// - S_OK
[[nodiscard]] HRESULT SCREEN_INFORMATION::ClearBuffer()
{
    // Rotate the buffer to bring the cursor row to the top of the viewport.
    const auto cursorPos = _textBuffer->GetCursor().GetPosition();
    for (auto i = 0; i < cursorPos.y; i++)
    {
        _textBuffer->IncrementCircularBuffer();
    }

    // Erase everything below that point.
    RETURN_IF_FAILED(SetCursorPosition({ 0, 1 }, false));
    auto& engine = reinterpret_cast<OutputStateMachineEngine&>(_stateMachine->Engine());
    engine.Dispatch().EraseInDisplay(DispatchTypes::EraseType::ToEnd);

    // Restore the original cursor x offset, but now on the first row.
    RETURN_IF_FAILED(SetCursorPosition({ cursorPos.x, 0 }, false));

    _textBuffer->TriggerRedrawAll();

    return S_OK;
}

// Method Description:
// - Sets up the Output state machine to be in pty mode. Sequences it doesn't
//      understand will be written to the pTtyConnection passed in here.
// Arguments:
// - pTtyConnection: This is a TerminalOutputConnection that we can write the
//      sequence we didn't understand to.
// Return Value:
// - <none>
void SCREEN_INFORMATION::SetTerminalConnection(_In_ VtEngine* const pTtyConnection)
{
    auto& engine = reinterpret_cast<OutputStateMachineEngine&>(_stateMachine->Engine());
    if (pTtyConnection)
    {
        engine.SetTerminalConnection(pTtyConnection,
                                     [&stateMachine = *_stateMachine]() -> bool {
                                         ServiceLocator::LocateGlobals().pRender->NotifyPaintFrame();
                                         return stateMachine.FlushToTerminal();
                                     });
    }
    else
    {
        engine.SetTerminalConnection(nullptr,
                                     nullptr);
    }
}

// Routine Description:
// - This routine copies a rectangular region from the screen buffer. no clipping is done.
// Arguments:
// - viewport - rectangle in source buffer to copy
// Return Value:
// - output cell rectangle copy of screen buffer data
// Note:
// - will throw exception on error.
OutputCellRect SCREEN_INFORMATION::ReadRect(const Viewport viewport) const
{
    // If the viewport given doesn't fit inside this screen, it's not a valid argument.
    THROW_HR_IF(E_INVALIDARG, !GetBufferSize().IsInBounds(viewport));

    OutputCellRect result(viewport.Height(), viewport.Width());
    const OutputCell paddingCell{ std::wstring_view{ &UNICODE_SPACE, 1 }, {}, GetAttributes() };
    for (til::CoordType rowIndex = 0, height = viewport.Height(); rowIndex < height; ++rowIndex)
    {
        auto location = viewport.Origin();
        location.y += rowIndex;

        auto data = GetCellLineDataAt(location);
        const auto span = result.GetRow(rowIndex);
        auto it = span.begin();

        // Copy row data while there still is data and we haven't run out of rect to store it into.
        while (data && it < span.end())
        {
            *it++ = *data++;
        }

        // Pad out any remaining space.
        while (it < span.end())
        {
            *it++ = paddingCell;
        }

        // if we're clipping a dbcs char then don't include it, add a space instead
        if (span.begin()->DbcsAttr() == DbcsAttribute::Trailing)
        {
            *span.begin() = paddingCell;
        }
        if (span.rbegin()->DbcsAttr() == DbcsAttribute::Leading)
        {
            *span.rbegin() = paddingCell;
        }
    }

    return result;
}

// Routine Description:
// - Writes cells to the output buffer at the cursor position.
// Arguments:
// - it - Iterator representing output cell data to write.
// Return Value:
// - the iterator at its final position
// Note:
// - will throw exception on error.
OutputCellIterator SCREEN_INFORMATION::Write(const OutputCellIterator it)
{
    return _textBuffer->Write(it);
}

// Routine Description:
// - Writes cells to the output buffer.
// Arguments:
// - it - Iterator representing output cell data to write.
// - target - The position to start writing at
// - wrap - change the wrap flag if we hit the end of the row while writing and there's still more data
// Return Value:
// - the iterator at its final position
// Note:
// - will throw exception on error.
OutputCellIterator SCREEN_INFORMATION::Write(const OutputCellIterator it,
                                             const til::point target,
                                             const std::optional<bool> wrap)
{
    // NOTE: if wrap = true/false, we want to set the line's wrap to true/false (respectively) if we reach the end of the line
    return _textBuffer->Write(it, target, wrap);
}

// Routine Description:
// - This routine writes a rectangular region into the screen buffer.
// Arguments:
// - it - Iterator to the data to insert
// - viewport - rectangular region for insertion
// Return Value:
// - the iterator at its final position
// Note:
// - will throw exception on error.
OutputCellIterator SCREEN_INFORMATION::WriteRect(const OutputCellIterator it,
                                                 const Viewport viewport)
{
    THROW_HR_IF(E_INVALIDARG, viewport.Height() <= 0);
    THROW_HR_IF(E_INVALIDARG, viewport.Width() <= 0);

    auto iter = it;
    for (auto i = viewport.Top(); i < viewport.BottomExclusive(); i++)
    {
        iter = _textBuffer->WriteLine(iter, { viewport.Left(), i }, false, viewport.RightInclusive());
    }

    return iter;
}

// Routine Description:
// - This routine writes a rectangular region into the screen buffer.
// Arguments:
// - data - rectangular data in memory buffer
// - location - origin point (top left corner) of where to write rectangular data
// Return Value:
// - <none>
// Note:
// - will throw exception on error.
void SCREEN_INFORMATION::WriteRect(const OutputCellRect& data,
                                   const til::point location)
{
    for (til::CoordType i = 0; i < data.Height(); i++)
    {
        const auto iter = data.GetRowIter(i);

        til::point point;
        point.x = location.x;
        point.y = location.y + i;

        _textBuffer->WriteLine(iter, point);
    }
}

// Routine Description:
// - Clears out the entire text buffer with the default character and
//   the current default attribute applied to this screen.
void SCREEN_INFORMATION::ClearTextData()
{
    // Clear the text buffer.
    _textBuffer->Reset();
}

// Routine Description:
// - finds the boundaries of the word at the given position on the screen
// Arguments:
// - position - location on the screen to get the word boundary for
// Return Value:
// - word boundary positions
std::pair<til::point, til::point> SCREEN_INFORMATION::GetWordBoundary(const til::point position) const
{
    // The position argument is in screen coordinates, but we need the
    // equivalent buffer position, taking line rendition into account.
    auto clampedPosition = _textBuffer->ScreenToBufferPosition(position);
    GetBufferSize().Clamp(clampedPosition);

    auto start{ clampedPosition };
    auto end{ clampedPosition };

    // find the start of the word
    auto startIt = GetTextLineDataAt(clampedPosition);
    while (startIt)
    {
        --startIt;
        if (!startIt || IsWordDelim(*startIt))
        {
            break;
        }
        --start.x;
    }

    // find the end of the word
    auto endIt = GetTextLineDataAt(clampedPosition);
    while (endIt)
    {
        if (IsWordDelim(*endIt))
        {
            break;
        }
        ++endIt;
        ++end.x;
    }

    // trim leading zeros if we need to
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (gci.GetTrimLeadingZeros())
    {
        // Trim the leading zeros: 000fe12 -> fe12, except 0x and 0n.
        // Useful for debugging

        // Get iterator from the start of the selection
        auto trimIt = GetTextLineDataAt(start);

        // Advance to the second character to check if it's an x or n.
        trimIt++;

        // Only process if it's a single character. If it's a complicated run, then it's not an x or n.
        if (trimIt->size() == 1)
        {
            // Get the single character
            const auto wch = trimIt->front();

            // If the string is long enough to have stuff after the 0x/0n and it doesn't have one...
            if (end.x > start.x + 2 &&
                wch != L'x' &&
                wch != L'X' &&
                wch != L'n')
            {
                trimIt--; // Back up to the first character again

                // Now loop through and advance the selection forward each time
                // we find a single character '0' to Trim off the leading zeroes.
                while (trimIt->size() == 1 &&
                       trimIt->front() == L'0' &&
                       start.x < end.x - 1)
                {
                    start.x++;
                    trimIt++;
                }
            }
        }
    }

    // The calculated range is in buffer coordinates, but the caller is
    // expecting screen offsets, so we have to convert these back again.
    start = _textBuffer->BufferToScreenPosition(start);
    end = _textBuffer->BufferToScreenPosition(end);

    return { start, end };
}

TextBuffer& SCREEN_INFORMATION::GetTextBuffer() noexcept
{
    return *_textBuffer;
}

const TextBuffer& SCREEN_INFORMATION::GetTextBuffer() const noexcept
{
    return *_textBuffer;
}

TextBufferTextIterator SCREEN_INFORMATION::GetTextDataAt(const til::point at) const
{
    return _textBuffer->GetTextDataAt(at);
}

TextBufferCellIterator SCREEN_INFORMATION::GetCellDataAt(const til::point at) const
{
    return _textBuffer->GetCellDataAt(at);
}

TextBufferTextIterator SCREEN_INFORMATION::GetTextLineDataAt(const til::point at) const
{
    return _textBuffer->GetTextLineDataAt(at);
}

TextBufferCellIterator SCREEN_INFORMATION::GetCellLineDataAt(const til::point at) const
{
    return _textBuffer->GetCellLineDataAt(at);
}

TextBufferTextIterator SCREEN_INFORMATION::GetTextDataAt(const til::point at, const Viewport limit) const
{
    return _textBuffer->GetTextDataAt(at, limit);
}

TextBufferCellIterator SCREEN_INFORMATION::GetCellDataAt(const til::point at, const Viewport limit) const
{
    return _textBuffer->GetCellDataAt(at, limit);
}

// Method Description:
// - Updates our internal "virtual bottom" tracker with wherever the viewport
//      currently is.
// - <none>
// Return Value:
// - <none>
void SCREEN_INFORMATION::UpdateBottom()
{
    _virtualBottom = _viewport.BottomInclusive();
}

// Method Description:
// - Returns the "virtual" Viewport - the viewport with its bottom at
//      `_virtualBottom`. For VT operations, this is essentially the mutable
//      section of the buffer.
// Arguments:
// - <none>
// Return Value:
// - the virtual terminal viewport
Viewport SCREEN_INFORMATION::GetVirtualViewport() const noexcept
{
    const auto newTop = _virtualBottom - _viewport.Height() + 1;
    return Viewport::FromDimensions({ _viewport.Left(), newTop }, _viewport.Dimensions());
}

// Method Description:
// - Returns true if the character at the cursor's current position is wide.
//   See IsGlyphFullWidth
// Arguments:
// - <none>
// Return Value:
// - true if the character at the cursor's current position is wide
bool SCREEN_INFORMATION::CursorIsDoubleWidth() const
{
    const auto& buffer = GetTextBuffer();
    const auto position = buffer.GetCursor().GetPosition();
    return buffer.GetRowByOffset(position.y).DbcsAttrAt(position.x) != DbcsAttribute::Single;
}

// Method Description:
// - Gets the current font of the screen buffer.
// Arguments:
// - <none>
// Return Value:
// - A FontInfo describing our current font.
FontInfo& SCREEN_INFORMATION::GetCurrentFont() noexcept
{
    return _currentFont;
}

// Method Description:
// See the non-const version of this function.
const FontInfo& SCREEN_INFORMATION::GetCurrentFont() const noexcept
{
    return _currentFont;
}

// Method Description:
// - Gets the desired font of the screen buffer. If we try loading this font and
//      have to fallback to another, then GetCurrentFont()!=GetDesiredFont().
//      We store this separately, so that if we need to reload the font, we can
//      try again with our preferred font info (in the desired font info) instead
//      of re-using the looked up value from before.
// Arguments:
// - <none>
// Return Value:
// - A FontInfo describing our desired font.
FontInfoDesired& SCREEN_INFORMATION::GetDesiredFont() noexcept
{
    return _desiredFont;
}

// Method Description:
// See the non-const version of this function.
const FontInfoDesired& SCREEN_INFORMATION::GetDesiredFont() const noexcept
{
    return _desiredFont;
}

// Routine Description:
// - Engages the legacy VT handling quirk; see TextAttribute::StripErroneousVT16VersionsOfLegacyDefaults
void SCREEN_INFORMATION::SetIgnoreLegacyEquivalentVTAttributes() noexcept
{
    _ignoreLegacyEquivalentVTAttributes = true;
}

// Routine Description:
// - Disengages the legacy VT handling quirk; see TextAttribute::StripErroneousVT16VersionsOfLegacyDefaults
void SCREEN_INFORMATION::ResetIgnoreLegacyEquivalentVTAttributes() noexcept
{
    _ignoreLegacyEquivalentVTAttributes = false;
}

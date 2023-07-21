// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "vtrenderer.hpp"
#include "../../inc/conattrs.hpp"
#include "../../host/VtIo.hpp"

// For _vcprintf
#include <conio.h>
#include <cstdarg>

#pragma hdrstop

using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

constexpr til::point VtEngine::INVALID_COORDS = { -1, -1 };

// Routine Description:
// - Creates a new VT-based rendering engine
// - NOTE: Will throw if initialization failure. Caller must catch.
// Arguments:
// - <none>
// Return Value:
// - An instance of a Renderer.
VtEngine::VtEngine(_In_ wil::unique_hfile pipe,
                   const Viewport initialViewport) :
    RenderEngineBase(),
    _hFile(std::move(pipe)),
    _usingLineRenditions(false),
    _stopUsingLineRenditions(false),
    _usingSoftFont(false),
    _lastTextAttributes(INVALID_COLOR, INVALID_COLOR),
    _lastViewport(initialViewport),
    _pool(til::pmr::get_default_resource()),
    _invalidMap(initialViewport.Dimensions(), false, &_pool),
    _scrollDelta(0, 0),
    _quickReturn(false),
    _clearedAllThisFrame(false),
    _cursorMoved(false),
    _resized(false),
    _suppressResizeRepaint(true),
    _virtualTop(0),
    _circled(false),
    _firstPaint(true),
    _skipCursor(false),
    _exitResult{ S_OK },
    _terminalOwner{ nullptr },
    _newBottomLine{ false },
    _deferredCursorPos{ INVALID_COORDS },
    _inResizeRequest{ false },
    _trace{},
    _bufferLine{},
    _buffer{},
    _formatBuffer{},
    _conversionBuffer{},
    _pfnSetLookingForDSR{}
{
#ifndef UNIT_TESTING
    // When unit testing, we can instantiate a VtEngine without a pipe.
    THROW_HR_IF(E_HANDLE, !_hFile);
#else
    // member is only defined when UNIT_TESTING is.
    _usingTestCallback = false;
#endif
}

// Method Description:
// - Writes a fill of characters to our file handle (repeat of same character over and over)
[[nodiscard]] HRESULT VtEngine::_WriteFill(const size_t n, const char c) noexcept
try
{
    _trace.TraceStringFill(n, c);
#ifdef UNIT_TESTING
    if (_usingTestCallback)
    {
        const std::string str(n, c);
        // Try to get the last error. If that wasn't set, then the test probably
        // doesn't set last error. No matter. We'll just return with E_FAIL
        // then. This is a unit test, we don't particularly care.
        const auto succeeded = _pfnTestCallback(str.data(), str.size());
        auto hr = E_FAIL;
        if (!succeeded)
        {
            const auto err = ::GetLastError();
            // If there wasn't an error in GLE, just use E_FAIL
            hr = SUCCEEDED_WIN32(err) ? hr : HRESULT_FROM_WIN32(err);
        }
        return succeeded ? S_OK : hr;
    }
#endif

    // TODO GH10001: Replace me with REP
    _buffer.append(n, c);
    return S_OK;
}
CATCH_RETURN();

// Method Description:
// - Writes the characters to our file handle. If we're building the unit tests,
//      we can instead write to the test callback, in order to avoid needing to
//      set up pipes and threads for unit tests.
// Arguments:
// - str: The buffer to write to the pipe. Might have nulls in it.
// Return Value:
// - S_OK or suitable HRESULT error from writing pipe.
[[nodiscard]] HRESULT VtEngine::_Write(std::string_view const str) noexcept
{
    _trace.TraceString(str);
#ifdef UNIT_TESTING
    if (_usingTestCallback)
    {
        // Try to get the last error. If that wasn't set, then the test probably
        // doesn't set last error. No matter. We'll just return with E_FAIL
        // then. This is a unit test, we don't particularly care.
        const auto succeeded = _pfnTestCallback(str.data(), str.size());
        auto hr = E_FAIL;
        if (!succeeded)
        {
            const auto err = ::GetLastError();
            // If there wasn't an error in GLE, just use E_FAIL
            hr = SUCCEEDED_WIN32(err) ? hr : HRESULT_FROM_WIN32(err);
        }
        return succeeded ? S_OK : hr;
    }
#endif

    try
    {
        _buffer.append(str);

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT VtEngine::_Flush() noexcept
{
    if (_hFile)
    {
        auto fSuccess = !!WriteFile(_hFile.get(), _buffer.data(), gsl::narrow_cast<DWORD>(_buffer.size()), nullptr, nullptr);
        _buffer.clear();
        if (!fSuccess)
        {
            _exitResult = HRESULT_FROM_WIN32(GetLastError());
            _hFile.reset();
            if (_terminalOwner)
            {
                _terminalOwner->CloseOutput();
            }
            return _exitResult;
        }
    }

    return S_OK;
}

// Method Description:
// - Wrapper for _Write.
[[nodiscard]] HRESULT VtEngine::WriteTerminalUtf8(const std::string_view str) noexcept
{
    return _Write(str);
}

// Method Description:
// - Writes a wstring to the tty, encoded as full utf-8. This is one
//      implementation of the WriteTerminalW method.
// Arguments:
// - wstr - wstring of text to be written
// Return Value:
// - S_OK or suitable HRESULT error from either conversion or writing pipe.
[[nodiscard]] HRESULT VtEngine::_WriteTerminalUtf8(const std::wstring_view wstr) noexcept
{
    RETURN_IF_FAILED(til::u16u8(wstr, _conversionBuffer));
    return _Write(_conversionBuffer);
}

// Method Description:
// - Writes a wstring to the tty, encoded as "utf-8" where characters that are
//      outside the ASCII range are encoded as '?'
//   This mainly exists to maintain compatibility with the inbox telnet client.
//   This is one implementation of the WriteTerminalW method.
// Arguments:
// - wstr - wstring of text to be written
// Return Value:
// - S_OK or suitable HRESULT error from writing pipe.
[[nodiscard]] HRESULT VtEngine::_WriteTerminalAscii(const std::wstring_view wstr) noexcept
{
    std::string needed;
    needed.reserve(wstr.size());

    for (const auto& wch : wstr)
    {
        // We're explicitly replacing characters outside ASCII with a ? because
        //      that's what telnet wants.
        needed.push_back((wch > L'\x7f') ? '?' : static_cast<char>(wch));
    }

    return _Write(needed);
}

// Method Description:
// - Writes a wstring to the tty when the characters are from the DRCS soft font.
//       It is assumed that the character set has already been designated in the
//       client terminal, so we just need to re-map our internal representation
//       of the characters into ASCII.
// Arguments:
// - wstr - wstring of text to be written
// Return Value:
// - S_OK or suitable HRESULT error from writing pipe.
[[nodiscard]] HRESULT VtEngine::_WriteTerminalDrcs(const std::wstring_view wstr) noexcept
{
    std::string needed;
    needed.reserve(wstr.size());

    for (const auto& wch : wstr)
    {
        // Our DRCS characters use the range U+EF20 to U+EF7F from the Unicode
        // Private Use Area. To map them back to ASCII we just mask with 7F.
        needed.push_back(wch & 0x7F);
    }

    return _Write(needed);
}

// Method Description:
// - This method will update the active font on the current device context
//      Does nothing for vt, the font is handed by the terminal.
// Arguments:
// - FontDesired - reference to font information we should use while instantiating a font.
// - Font - reference to font information where the chosen font information will be populated.
// Return Value:
// - HRESULT S_OK
[[nodiscard]] HRESULT VtEngine::UpdateFont(const FontInfoDesired& /*pfiFontDesired*/,
                                           _Out_ FontInfo& /*pfiFont*/) noexcept
{
    return S_OK;
}

// Method Description:
// - This method will modify the DPI we're using for scaling calculations.
//      Does nothing for vt, the dpi is handed by the terminal.
// Arguments:
// - iDpi - The Dots Per Inch to use for scaling. We will use this relative to
//      the system default DPI defined in Windows headers as a constant.
// Return Value:
// - HRESULT S_OK
[[nodiscard]] HRESULT VtEngine::UpdateDpi(const int /*iDpi*/) noexcept
{
    return S_OK;
}

// Method Description:
// - This method will update our internal reference for how big the viewport is.
//      If the viewport has changed size, then we'll need to send an update to
//      the terminal.
// Arguments:
// - srNewViewport - The bounds of the new viewport.
// Return Value:
// - HRESULT S_OK
[[nodiscard]] HRESULT VtEngine::UpdateViewport(const til::inclusive_rect& srNewViewport) noexcept
{
    auto hr = S_OK;
    const auto newView = Viewport::FromInclusive(srNewViewport);
    const auto oldSize = _lastViewport.Dimensions();
    const auto newSize = newView.Dimensions();

    if (oldSize != newSize)
    {
        // Don't emit a resize event if we've requested it be suppressed
        if (!_suppressResizeRepaint)
        {
            hr = _ResizeWindow(newSize.width, newSize.height);
        }

        if (_resizeQuirk)
        {
            // GH#3490 - When the viewport width changed, don't do anything extra here.
            // If the buffer had areas that were invalid due to the resize, then the
            // buffer will have triggered its own invalidations for what it knows is
            // invalid. Previously, we'd invalidate everything if the width changed,
            // because we couldn't be sure if lines were reflowed.
            _invalidMap.resize(newSize);
        }
        else
        {
            if (SUCCEEDED(hr))
            {
                _invalidMap.resize(newSize, true); // resize while filling in new space with repaint requests.

                // Viewport is smaller now - just update it all.
                if (oldSize.height > newSize.height || oldSize.width > newSize.width)
                {
                    hr = InvalidateAll();
                }
            }
        }

        _resized = true;
    }

    // See MSFT:19408543
    // Always clear the suppression request, even if the new size was the same
    //      as the last size. We're always going to get a UpdateViewport call
    //      for our first frame. However, we start with _suppressResizeRepaint set,
    //      to prevent that first UpdateViewport call from emitting our size.
    // If we only clear the flag when the new viewport is different, this can
    //      lead to the first _actual_ resize being suppressed.
    _suppressResizeRepaint = false;
    _lastViewport = newView;

    return hr;
}

// Method Description:
// - This method will figure out what the new font should be given the starting font information and a DPI.
// - When the final font is determined, the FontInfo structure given will be updated with the actual resulting font chosen as the nearest match.
// - NOTE: It is left up to the underling rendering system to choose the nearest font. Please ask for the font dimensions if they are required using the interface. Do not use the size you requested with this structure.
// - If the intent is to immediately turn around and use this font, pass the optional handle parameter and use it immediately.
//      Does nothing for vt, the font is handed by the terminal.
// Arguments:
// - FontDesired - reference to font information we should use while instantiating a font.
// - Font - reference to font information where the chosen font information will be populated.
// - iDpi - The DPI we will have when rendering
// Return Value:
// - S_FALSE: This is unsupported by the VT Renderer and should use another engine's value.
[[nodiscard]] HRESULT VtEngine::GetProposedFont(const FontInfoDesired& /*pfiFontDesired*/,
                                                _Out_ FontInfo& /*pfiFont*/,
                                                const int /*iDpi*/) noexcept
{
    return S_FALSE;
}

// Method Description:
// - Retrieves the current pixel size of the font we have selected for drawing.
// Arguments:
// - pFontSize - receives the current X by Y size of the font.
// Return Value:
// - S_FALSE: This is unsupported by the VT Renderer and should use another engine's value.
[[nodiscard]] HRESULT VtEngine::GetFontSize(_Out_ til::size* const pFontSize) noexcept
{
    *pFontSize = { 1, 1 };
    return S_FALSE;
}

// Method Description:
// - Sets the test callback for this instance. Instead of rendering to a pipe,
//      this instance will instead render to a callback for testing.
// Arguments:
// - pfn: a callback to call instead of writing to the pipe.
// Return Value:
// - <none>
void VtEngine::SetTestCallback(_In_ std::function<bool(const char* const, size_t const)> pfn)
{
#ifdef UNIT_TESTING

    _pfnTestCallback = pfn;
    _usingTestCallback = true;

#else
    THROW_HR(E_FAIL);
#endif
}

// Method Description:
// - Returns true if the entire viewport has been invalidated. That signals we
//      should use a VT Clear Screen sequence as an optimization.
// Arguments:
// - <none>
// Return Value:
// - true if the entire viewport has been invalidated
bool VtEngine::_AllIsInvalid() const
{
    return _invalidMap.all();
}

// Method Description:
// - Prevent the renderer from emitting output on the next resize. This prevents
//      the host from echoing a resize to the terminal that requested it.
// Arguments:
// - <none>
// Return Value:
// - S_OK
[[nodiscard]] HRESULT VtEngine::SuppressResizeRepaint() noexcept
{
    _suppressResizeRepaint = true;
    return S_OK;
}

// Method Description:
// - "Inherit" the cursor at the given position. We won't need to move it
//      anywhere, so update where we last thought the cursor was.
//  Also update our "virtual top", indicating where should clip all updates to
//      (we don't want to paint the empty region above the inherited cursor).
//  Also ignore the next InvalidateCursor call.
// Arguments:
// - coordCursor: The cursor position to inherit from.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT VtEngine::InheritCursor(const til::point coordCursor) noexcept
{
    _virtualTop = coordCursor.y;
    _lastText = coordCursor;
    _skipCursor = true;
    // Prevent us from clearing the entire viewport on the first paint
    _firstPaint = false;
    return S_OK;
}

void VtEngine::SetTerminalOwner(Microsoft::Console::VirtualTerminal::VtIo* const terminalOwner)
{
    _terminalOwner = terminalOwner;
}

// Method Description:
// - sends a sequence to request the end terminal to tell us the
//      cursor position. The terminal will reply back on the vt input handle.
//   Flushes the buffer as well, to make sure the request is sent to the terminal.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
HRESULT VtEngine::RequestCursor() noexcept
{
    RETURN_IF_FAILED(_RequestCursor());
    RETURN_IF_FAILED(_Flush());
    return S_OK;
}

// Method Description:
// - Sends a notification through to the `VtInputThread` that it should
//   watch for and capture the response from a DSR message we're about to send.
//   This is typically `RequestCursor` at the time of writing this, but in theory
//   could be another DSR as well.
// Arguments:
// - <none>
// Return Value:
// - S_OK if all goes well. Invalid state error if no notification function is installed.
//   (see `SetLookingForDSRCallback` to install one.)
[[nodiscard]] HRESULT VtEngine::_ListenForDSR() noexcept
{
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_STATE), !_pfnSetLookingForDSR);
    _pfnSetLookingForDSR(true);
    return S_OK;
}

// Method Description:
// - Tell the vt renderer to begin a resize operation. During a resize
//   operation, the vt renderer should _not_ request to be repainted during a
//   text buffer circling event. Any callers of this method should make sure to
//   call EndResize to make sure the renderer returns to normal behavior.
//   See GH#1795 for context on this method.
// Arguments:
// - <none>
// Return Value:
// - <none>
void VtEngine::BeginResizeRequest()
{
    _inResizeRequest = true;
}

// Method Description:
// - Tell the vt renderer to end a resize operation.
//   See BeginResize for more details.
//   See GH#1795 for context on this method.
// Arguments:
// - <none>
// Return Value:
// - <none>
void VtEngine::EndResizeRequest()
{
    _inResizeRequest = false;
}

// Method Description:
// - Configure the renderer for the resize quirk. This changes the behavior of
//   conpty to _not_ InvalidateAll the entire viewport on a resize operation.
//   This is used by the Windows Terminal, because it is prepared to be
//   connected to a conpty, and handles its own buffer specifically for a
//   conpty scenario.
// - See also: GH#3490, #4354, #4741
// Arguments:
// - resizeQuirk - True to turn on the quirk. False otherwise.
// Return Value:
// - true iff we were started with the `--resizeQuirk` flag enabled.
void VtEngine::SetResizeQuirk(const bool resizeQuirk)
{
    _resizeQuirk = resizeQuirk;
}

// Method Description:
// - Configure the renderer to understand that we're operating in limited-draw
//   passthrough mode. We do not need to handle full responsibility for replicating
//   buffer state to the attached terminal.
// Arguments:
// - passthrough - True to turn on passthrough mode. False otherwise.
// Return Value:
// - true iff we were started with an output mode for passthrough. false otherwise.
void VtEngine::SetPassthroughMode(const bool passthrough) noexcept
{
    _passthrough = passthrough;
}

void VtEngine::SetLookingForDSRCallback(std::function<void(bool)> pfnLooking) noexcept
{
    _pfnSetLookingForDSR = pfnLooking;
}

void VtEngine::SetTerminalCursorTextPosition(const til::point cursor) noexcept
{
    _lastText = cursor;
}

// Method Description:
// - Manually emit a "Erase Scrollback" sequence to the connected terminal. We
//   need to do this in certain cases that we've identified where we believe the
//   client wanted the entire terminal buffer cleared, not just the viewport.
//   For more information, see GH#3126.
// - This is unimplemented in the win-telnet, xterm-ascii renderers - inbox
//   telnet.exe doesn't know how to handle a ^[[3J. This _is_ implemented in the
//   Xterm256Engine.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we wrote the sequences successfully, otherwise an appropriate HRESULT
[[nodiscard]] HRESULT VtEngine::ManuallyClearScrollback() noexcept
{
    return S_OK;
}

// Method Description:
// - Send a sequence to the connected terminal to request win32-input-mode from
//   them. This will enable the connected terminal to send us full INPUT_RECORDs
//   as input. If the terminal doesn't understand this sequence, it'll just
//   ignore it.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
HRESULT VtEngine::RequestWin32Input() noexcept
{
    // It's important that any additional modes set here are also mirrored in
    // the AdaptDispatch::HardReset method, since that needs to re-enable them
    // in the connected terminal after passing through an RIS sequence.
    RETURN_IF_FAILED(_RequestWin32Input());
    RETURN_IF_FAILED(_RequestFocusEventMode());
    RETURN_IF_FAILED(_Flush());
    return S_OK;
}

HRESULT VtEngine::SwitchScreenBuffer(const bool useAltBuffer) noexcept
{
    RETURN_IF_FAILED(_SwitchScreenBuffer(useAltBuffer));
    RETURN_IF_FAILED(_Flush());
    return S_OK;
}

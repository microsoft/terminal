// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "vtrenderer.hpp"
#include "../../inc/conattrs.hpp"
#include "../../types/inc/convert.hpp"

// For _vcprintf
#include <conio.h>
#include <stdarg.h>

#pragma hdrstop

using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

const COORD VtEngine::INVALID_COORDS = { -1, -1 };

// Routine Description:
// - Creates a new VT-based rendering engine
// - NOTE: Will throw if initialization failure. Caller must catch.
// Arguments:
// - <none>
// Return Value:
// - An instance of a Renderer.
VtEngine::VtEngine(_In_ wil::unique_hfile pipe,
                   const IDefaultColorProvider& colorProvider,
                   const Viewport initialViewport) :
    RenderEngineBase(),
    _hFile(std::move(pipe)),
    _colorProvider(colorProvider),
    _LastFG(INVALID_COLOR),
    _LastBG(INVALID_COLOR),
    _lastWasBold(false),
    _lastViewport(initialViewport),
    _invalidRect(Viewport::Empty()),
    _fInvalidRectUsed(false),
    _lastRealCursor({ 0 }),
    _lastText({ 0 }),
    _scrollDelta({ 0 }),
    _quickReturn(false),
    _clearedAllThisFrame(false),
    _cursorMoved(false),
    _resized(false),
    _suppressResizeRepaint(true),
    _virtualTop(0),
    _circled(false),
    _firstPaint(true),
    _skipCursor(false),
    _pipeBroken(false),
    _exitResult{ S_OK },
    _terminalOwner{ nullptr },
    _newBottomLine{ false },
    _deferredCursorPos{ INVALID_COORDS },
    _inResizeRequest{ false },
    _trace{}
{
#ifndef UNIT_TESTING
    // When unit testing, we can instantiate a VtEngine without a pipe.
    THROW_HR_IF(E_HANDLE, _hFile.get() == INVALID_HANDLE_VALUE);
#else
    // member is only defined when UNIT_TESTING is.
    _usingTestCallback = false;
#endif
}

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
        RETURN_LAST_ERROR_IF(!_pfnTestCallback(str.data(), str.size()));
        return S_OK;
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
#ifdef UNIT_TESTING
    if (_hFile.get() == INVALID_HANDLE_VALUE)
    {
        // Do not flush during Unit Testing because we won't have a valid file.
        return S_OK;
    }
#endif

    if (!_pipeBroken)
    {
        bool fSuccess = !!WriteFile(_hFile.get(), _buffer.data(), static_cast<DWORD>(_buffer.size()), nullptr, nullptr);
        _buffer.clear();
        if (!fSuccess)
        {
            _exitResult = HRESULT_FROM_WIN32(GetLastError());
            _pipeBroken = true;
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
// - Wrapper for ITerminalOutputConnection. See _Write.
[[nodiscard]] HRESULT VtEngine::WriteTerminalUtf8(const std::string& str) noexcept
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
[[nodiscard]] HRESULT VtEngine::_WriteTerminalUtf8(const std::wstring& wstr) noexcept
{
    try
    {
        const auto converted = ConvertToA(CP_UTF8, wstr);
        return _Write(converted);
    }
    CATCH_RETURN();
}

// Method Description:
// - Writes a wstring to the tty, encoded as "utf-8" where characters that are
//      outside the ASCII range are encoded as '?'
//   This mainly exists to maintain compatability with the inbox telnet client.
//   This is one implementation of the WriteTerminalW method.
// Arguments:
// - wstr - wstring of text to be written
// Return Value:
// - S_OK or suitable HRESULT error from writing pipe.
[[nodiscard]] HRESULT VtEngine::_WriteTerminalAscii(const std::wstring& wstr) noexcept
{
    const size_t cchActual = wstr.length();

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
// - Helper for calling _Write with a string for formatting a sequence. Used
//      extensively by VtSequences.cpp
// Arguments:
// - pFormat: the pointer to the string to write to the pipe.
// - ...: a va_list of args to format the string with.
// Return Value:
// - S_OK, E_INVALIDARG for a invalid format string, or suitable HRESULT error
//      from writing pipe.
[[nodiscard]] HRESULT VtEngine::_WriteFormattedString(const std::string* const pFormat, ...) noexcept
{
    HRESULT hr = E_FAIL;
    va_list argList;
    va_start(argList, pFormat);

    int cchNeeded = _scprintf(pFormat->c_str(), argList);
    // -1 is the _scprintf error case https://msdn.microsoft.com/en-us/library/t32cf9tb.aspx
    if (cchNeeded > -1)
    {
        wistd::unique_ptr<char[]> psz = wil::make_unique_nothrow<char[]>(cchNeeded + 1);
        RETURN_IF_NULL_ALLOC(psz);

        int cchWritten = _vsnprintf_s(psz.get(), cchNeeded + 1, cchNeeded, pFormat->c_str(), argList);
        hr = _Write({ psz.get(), gsl::narrow<size_t>(cchWritten) });
    }
    else
    {
        hr = E_INVALIDARG;
    }

    va_end(argList);
    return hr;
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
[[nodiscard]] HRESULT VtEngine::UpdateViewport(const SMALL_RECT srNewViewport) noexcept
{
    HRESULT hr = S_OK;
    const Viewport oldView = _lastViewport;
    const Viewport newView = Viewport::FromInclusive(srNewViewport);

    _lastViewport = newView;

    if ((oldView.Height() != newView.Height()) || (oldView.Width() != newView.Width()))
    {
        // Don't emit a resize event if we've requested it be suppressed
        if (!_suppressResizeRepaint)
        {
            hr = _ResizeWindow(newView.Width(), newView.Height());
        }
    }

    // See MSFT:19408543
    // Always clear the suppression request, even if the new size was the same
    //      as the last size. We're always going to get a UpdateViewport call
    //      for our first frame. However, we start with _suppressResizeRepaint set,
    //      to prevent that first UpdateViewport call from emitting our size.
    // If we only clear the flag when the new viewport is different, this can
    //      lead to the first _actual_ resize being suppressed.
    _suppressResizeRepaint = false;

    if (SUCCEEDED(hr))
    {
        // Viewport is smaller now - just update it all.
        if (oldView.Height() > newView.Height() || oldView.Width() > newView.Width())
        {
            hr = InvalidateAll();
        }
        else
        {
            // At least one of the directions grew.
            // First try and add everything to the right of the old viewport,
            //      then everything below where the old viewport ended.
            if (oldView.Width() < newView.Width())
            {
                short left = oldView.RightExclusive();
                short top = 0;
                short right = newView.RightInclusive();
                short bottom = oldView.BottomInclusive();
                Viewport rightOfOldViewport = Viewport::FromInclusive({ left, top, right, bottom });
                hr = _InvalidCombine(rightOfOldViewport);
            }
            if (SUCCEEDED(hr) && oldView.Height() < newView.Height())
            {
                short left = 0;
                short top = oldView.BottomExclusive();
                short right = newView.RightInclusive();
                short bottom = newView.BottomInclusive();
                Viewport belowOldViewport = Viewport::FromInclusive({ left, top, right, bottom });
                hr = _InvalidCombine(belowOldViewport);
            }
        }
    }
    _resized = true;
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
// - pFontSize - recieves the current X by Y size of the font.
// Return Value:
// - S_FALSE: This is unsupported by the VT Renderer and should use another engine's value.
[[nodiscard]] HRESULT VtEngine::GetFontSize(_Out_ COORD* const pFontSize) noexcept
{
    *pFontSize = COORD({ 1, 1 });
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
    return _lastViewport == _invalidRect;
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
[[nodiscard]] HRESULT VtEngine::InheritCursor(const COORD coordCursor) noexcept
{
    _virtualTop = coordCursor.Y;
    _lastText = coordCursor;
    _skipCursor = true;
    // Prevent us from clearing the entire viewport on the first paint
    _firstPaint = false;
    return S_OK;
}

void VtEngine::SetTerminalOwner(Microsoft::Console::ITerminalOwner* const terminalOwner)
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

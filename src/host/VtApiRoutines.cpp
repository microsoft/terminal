// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "VtApiRoutines.h"
#include "../interactivity/inc/ServiceLocator.hpp"
#include "../types/inc/convert.hpp"

using namespace Microsoft::Console::Interactivity;

// When someone attempts to use the console APIs to do a "read back"
// of the console buffer, we have to give them **something**.
// These two structures are just some gaudy-colored replacement character
// text to give them data but represent they've done something that cannot
// be supported under VT passthrough mode.
// ----
// They can't be supported because in passthrough we maintain no internal
// buffer to answer these questions, and there is no VT sequence that lets
// us query the final terminal's buffer state. Even if a VT sequence did exist
// (and we personally believe it shouldn't), there's a possibility that it would
// read a massive amount of data and cause severe perf issues as applications coded
// to this old API are likely leaning on it heavily and asking for this data in a
// loop via VT would be a nightmare of parsing and formatting and over-the-wire transmission.

static constexpr CHAR_INFO s_readBackUnicode{
    { UNICODE_REPLACEMENT },
    FOREGROUND_INTENSITY | FOREGROUND_RED | BACKGROUND_GREEN
};

static constexpr CHAR_INFO s_readBackAscii{
    { L'?' },
    FOREGROUND_INTENSITY | FOREGROUND_RED | BACKGROUND_GREEN
};

VtApiRoutines::VtApiRoutines() :
    m_inputCodepage(ServiceLocator::LocateGlobals().getConsoleInformation().CP),
    m_outputCodepage(ServiceLocator::LocateGlobals().getConsoleInformation().OutputCP),
    m_inputMode(),
    m_outputMode(),
    m_pUsualRoutines(),
    m_pVtEngine(),
    m_listeningForDSR(false)
{
}

#pragma warning(push)
#pragma warning(disable : 4100) // unreferenced param

void VtApiRoutines::GetConsoleInputCodePageImpl(ULONG& codepage) noexcept
{
    codepage = m_inputCodepage;
    return;
}

void VtApiRoutines::GetConsoleOutputCodePageImpl(ULONG& codepage) noexcept
{
    codepage = m_outputCodepage;
    return;
}

void VtApiRoutines::GetConsoleInputModeImpl(InputBuffer& context,
                                            ULONG& mode) noexcept
{
    mode = m_inputMode;
    return;
}

void VtApiRoutines::GetConsoleOutputModeImpl(SCREEN_INFORMATION& context,
                                             ULONG& mode) noexcept
{
    mode = m_outputMode;
    return;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleInputModeImpl(InputBuffer& context,
                                                             const ULONG mode) noexcept
{
    m_inputMode = mode;
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleOutputModeImpl(SCREEN_INFORMATION& context,
                                                              const ULONG Mode) noexcept
{
    m_outputMode = Mode;
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::GetNumberOfConsoleInputEventsImpl(const InputBuffer& context,
                                                                       ULONG& events) noexcept
{
    return m_pUsualRoutines->GetNumberOfConsoleInputEventsImpl(context, events);
}

void VtApiRoutines::_SynchronizeCursor(std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    // If we're about to tell the caller to wait, let's synchronize the cursor we have with what
    // the terminal is presenting in case there's a cooked read going on.
    // TODO GH#10001: we only need to do this in cooked read mode.
    if (waiter)
    {
        m_listeningForDSR = true;
        (void)m_pVtEngine->_ListenForDSR();
        (void)m_pVtEngine->RequestCursor();
    }
}

[[nodiscard]] HRESULT VtApiRoutines::PeekConsoleInputAImpl(IConsoleInputObject& context,
                                                           std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                           const size_t eventsToRead,
                                                           INPUT_READ_HANDLE_DATA& readHandleState,
                                                           std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    const auto hr = m_pUsualRoutines->PeekConsoleInputAImpl(context, outEvents, eventsToRead, readHandleState, waiter);
    _SynchronizeCursor(waiter);
    return hr;
}

[[nodiscard]] HRESULT VtApiRoutines::PeekConsoleInputWImpl(IConsoleInputObject& context,
                                                           std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                           const size_t eventsToRead,
                                                           INPUT_READ_HANDLE_DATA& readHandleState,
                                                           std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    const auto hr = m_pUsualRoutines->PeekConsoleInputWImpl(context, outEvents, eventsToRead, readHandleState, waiter);
    _SynchronizeCursor(waiter);
    return hr;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleInputAImpl(IConsoleInputObject& context,
                                                           std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                           const size_t eventsToRead,
                                                           INPUT_READ_HANDLE_DATA& readHandleState,
                                                           std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    const auto hr = m_pUsualRoutines->ReadConsoleInputAImpl(context, outEvents, eventsToRead, readHandleState, waiter);
    _SynchronizeCursor(waiter);
    return hr;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleInputWImpl(IConsoleInputObject& context,
                                                           std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                           const size_t eventsToRead,
                                                           INPUT_READ_HANDLE_DATA& readHandleState,
                                                           std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    const auto hr = m_pUsualRoutines->ReadConsoleInputWImpl(context, outEvents, eventsToRead, readHandleState, waiter);
    _SynchronizeCursor(waiter);
    return hr;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleAImpl(IConsoleInputObject& context,
                                                      gsl::span<char> buffer,
                                                      size_t& written,
                                                      std::unique_ptr<IWaitRoutine>& waiter,
                                                      const std::string_view initialData,
                                                      const std::wstring_view exeName,
                                                      INPUT_READ_HANDLE_DATA& readHandleState,
                                                      const HANDLE clientHandle,
                                                      const DWORD controlWakeupMask,
                                                      DWORD& controlKeyState) noexcept
{
    const auto hr = m_pUsualRoutines->ReadConsoleAImpl(context, buffer, written, waiter, initialData, exeName, readHandleState, clientHandle, controlWakeupMask, controlKeyState);
    // If we're about to tell the caller to wait, let's synchronize the cursor we have with what
    // the terminal is presenting in case there's a cooked read going on.
    // TODO GH10001: we only need to do this in cooked read mode.
    if (clientHandle)
    {
        m_listeningForDSR = true;
        (void)m_pVtEngine->_ListenForDSR();
        (void)m_pVtEngine->RequestCursor();
    }
    return hr;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleWImpl(IConsoleInputObject& context,
                                                      gsl::span<char> buffer,
                                                      size_t& written,
                                                      std::unique_ptr<IWaitRoutine>& waiter,
                                                      const std::string_view initialData,
                                                      const std::wstring_view exeName,
                                                      INPUT_READ_HANDLE_DATA& readHandleState,
                                                      const HANDLE clientHandle,
                                                      const DWORD controlWakeupMask,
                                                      DWORD& controlKeyState) noexcept
{
    const auto hr = m_pUsualRoutines->ReadConsoleWImpl(context, buffer, written, waiter, initialData, exeName, readHandleState, clientHandle, controlWakeupMask, controlKeyState);
    // If we're about to tell the caller to wait, let's synchronize the cursor we have with what
    // the terminal is presenting in case there's a cooked read going on.
    // TODO GH10001: we only need to do this in cooked read mode.
    if (clientHandle)
    {
        m_listeningForDSR = true;
        (void)m_pVtEngine->_ListenForDSR();
        (void)m_pVtEngine->RequestCursor();
    }
    return hr;
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleAImpl(IConsoleOutputObject& context,
                                                       const std::string_view buffer,
                                                       size_t& read,
                                                       bool requiresVtQuirk,
                                                       std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    if (CP_UTF8 == m_outputCodepage)
    {
        (void)m_pVtEngine->WriteTerminalUtf8(buffer);
    }
    else
    {
        (void)m_pVtEngine->WriteTerminalW(ConvertToW(m_outputCodepage, buffer));
    }

    (void)m_pVtEngine->_Flush();
    read = buffer.size();
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleWImpl(IConsoleOutputObject& context,
                                                       const std::wstring_view buffer,
                                                       size_t& read,
                                                       bool requiresVtQuirk,
                                                       std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    (void)m_pVtEngine->WriteTerminalW(buffer);
    (void)m_pVtEngine->_Flush();
    read = buffer.size();
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleLangIdImpl(LANGID& langId) noexcept
{
    return m_pUsualRoutines->GetConsoleLangIdImpl(langId);
}

[[nodiscard]] HRESULT VtApiRoutines::FillConsoleOutputAttributeImpl(IConsoleOutputObject& OutContext,
                                                                    const WORD attribute,
                                                                    const size_t lengthToWrite,
                                                                    const til::point startingCoordinate,
                                                                    size_t& cellsModified) noexcept
{
    (void)m_pVtEngine->_CursorPosition(startingCoordinate);
    (void)m_pVtEngine->_SetGraphicsRendition16Color(static_cast<BYTE>(attribute), true);
    (void)m_pVtEngine->_SetGraphicsRendition16Color(static_cast<BYTE>(attribute >> 4), false);
    (void)m_pVtEngine->_WriteFill(lengthToWrite, s_readBackAscii.Char.AsciiChar);
    (void)m_pVtEngine->_Flush();
    cellsModified = lengthToWrite;
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::FillConsoleOutputCharacterAImpl(IConsoleOutputObject& OutContext,
                                                                     const char character,
                                                                     const size_t lengthToWrite,
                                                                     const til::point startingCoordinate,
                                                                     size_t& cellsModified) noexcept
{
    // I mean... if you get your jollies by using UTF8 for single byte codepoints...
    // we may as well skip a lot of conversion work and just write it out.
    if (m_outputCodepage == CP_UTF8 && character <= 0x7F)
    {
        (void)m_pVtEngine->_CursorPosition(startingCoordinate);
        (void)m_pVtEngine->_WriteFill(lengthToWrite, character);
        (void)m_pVtEngine->_Flush();
        cellsModified = lengthToWrite;
        return S_OK;
    }
    else
    {
        const auto wstr = ConvertToW(m_outputCodepage, std::string_view{ &character, 1 });
        return FillConsoleOutputCharacterWImpl(OutContext, wstr.front(), lengthToWrite, startingCoordinate, cellsModified);
    }
}

[[nodiscard]] HRESULT VtApiRoutines::FillConsoleOutputCharacterWImpl(IConsoleOutputObject& OutContext,
                                                                     const wchar_t character,
                                                                     const size_t lengthToWrite,
                                                                     const til::point startingCoordinate,
                                                                     size_t& cellsModified,
                                                                     const bool enablePowershellShim) noexcept
{
    (void)m_pVtEngine->_CursorPosition(startingCoordinate);
    const std::wstring_view sv{ &character, 1 };

    // TODO GH10001: horrible. it'll WC2MB over and over...we should do that once then emit... and then rep...
    // TODO GH10001: there's probably an optimization for if ((character & 0x7F) == character) --> call the UTF8 one.
    for (size_t i = 0; i < lengthToWrite; ++i)
    {
        (void)m_pVtEngine->WriteTerminalW(sv);
    }

    (void)m_pVtEngine->_Flush();
    cellsModified = lengthToWrite;
    return S_OK;
}

//// Process based. Restrict in protocol side?
//HRESULT GenerateConsoleCtrlEventImpl(const ULONG ProcessGroupFilter,
//                                             const ULONG ControlEvent);

void VtApiRoutines::SetConsoleActiveScreenBufferImpl(SCREEN_INFORMATION& newContext) noexcept
{
    return;
}

void VtApiRoutines::FlushConsoleInputBuffer(InputBuffer& context) noexcept
{
    m_pUsualRoutines->FlushConsoleInputBuffer(context);
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleInputCodePageImpl(const ULONG codepage) noexcept
{
    m_inputCodepage = codepage;
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleOutputCodePageImpl(const ULONG codepage) noexcept
{
    m_outputCodepage = codepage;
    return S_OK;
}

void VtApiRoutines::GetConsoleCursorInfoImpl(const SCREEN_INFORMATION& context,
                                             ULONG& size,
                                             bool& isVisible) noexcept
{
    // TODO GH10001: good luck capturing this out of the input buffer when it comes back in.
    //m_pVtEngine->RequestCursor();
    return;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleCursorInfoImpl(SCREEN_INFORMATION& context,
                                                              const ULONG size,
                                                              const bool isVisible) noexcept
{
    isVisible ? (void)m_pVtEngine->_ShowCursor() : (void)m_pVtEngine->_HideCursor();
    (void)m_pVtEngine->_Flush();
    return S_OK;
}

//// driver will pare down for non-Ex method
void VtApiRoutines::GetConsoleScreenBufferInfoExImpl(const SCREEN_INFORMATION& context,
                                                     CONSOLE_SCREEN_BUFFER_INFOEX& data) noexcept
{
    // TODO GH10001: this is technically full of potentially incorrect data. do we care? should we store it in here with set?
    return m_pUsualRoutines->GetConsoleScreenBufferInfoExImpl(context, data);
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleScreenBufferInfoExImpl(SCREEN_INFORMATION& context,
                                                                      const CONSOLE_SCREEN_BUFFER_INFOEX& data) noexcept
{
    (void)m_pVtEngine->_ResizeWindow(data.srWindow.Right - data.srWindow.Left, data.srWindow.Bottom - data.srWindow.Top);
    (void)m_pVtEngine->_CursorPosition(til::wrap_coord(data.dwCursorPosition));
    (void)m_pVtEngine->_SetGraphicsRendition16Color(static_cast<BYTE>(data.wAttributes), true);
    (void)m_pVtEngine->_SetGraphicsRendition16Color(static_cast<BYTE>(data.wAttributes >> 4), false);
    //color table?
    // popup attributes... hold internally?
    // TODO GH10001: popups are gonna erase the stuff behind them... deal with that somehow.
    (void)m_pVtEngine->_Flush();
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleScreenBufferSizeImpl(SCREEN_INFORMATION& context,
                                                                    const til::size size) noexcept
{
    // Don't transmit. The terminal figures out its own buffer size.
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleCursorPositionImpl(SCREEN_INFORMATION& context,
                                                                  const til::point position) noexcept
{
    if (m_listeningForDSR)
    {
        context.GetActiveBuffer().GetTextBuffer().GetCursor().SetPosition(position);
        m_pVtEngine->SetTerminalCursorTextPosition(position);
    }
    else
    {
        (void)m_pVtEngine->_CursorPosition(position);
        (void)m_pVtEngine->_Flush();
    }
    return S_OK;
}

void VtApiRoutines::GetLargestConsoleWindowSizeImpl(const SCREEN_INFORMATION& context,
                                                    til::size& size) noexcept
{
    m_pUsualRoutines->GetLargestConsoleWindowSizeImpl(context, size); // This is likely super weird but not weirder than existing ConPTY answers.
    return;
}

[[nodiscard]] HRESULT VtApiRoutines::ScrollConsoleScreenBufferAImpl(SCREEN_INFORMATION& context,
                                                                    const til::inclusive_rect& source,
                                                                    const til::point target,
                                                                    std::optional<til::inclusive_rect> clip,
                                                                    const char fillCharacter,
                                                                    const WORD fillAttribute) noexcept
{
    // TODO GH10001: Use DECCRA
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::ScrollConsoleScreenBufferWImpl(SCREEN_INFORMATION& context,
                                                                    const til::inclusive_rect& source,
                                                                    const til::point target,
                                                                    std::optional<til::inclusive_rect> clip,
                                                                    const wchar_t fillCharacter,
                                                                    const WORD fillAttribute,
                                                                    const bool enableCmdShim) noexcept
{
    // TODO GH10001: Use DECCRA
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleTextAttributeImpl(SCREEN_INFORMATION& context,
                                                                 const WORD attribute) noexcept
{
    (void)m_pVtEngine->_SetGraphicsRendition16Color(static_cast<BYTE>(attribute), true);
    (void)m_pVtEngine->_SetGraphicsRendition16Color(static_cast<BYTE>(attribute >> 4), false);
    (void)m_pVtEngine->_Flush();
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleWindowInfoImpl(SCREEN_INFORMATION& context,
                                                              const bool isAbsolute,
                                                              const til::inclusive_rect& windowRect) noexcept
{
    (void)m_pVtEngine->_ResizeWindow(windowRect.Right - windowRect.Left + 1, windowRect.Bottom - windowRect.Top + 1);
    (void)m_pVtEngine->_Flush();
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleOutputAttributeImpl(const SCREEN_INFORMATION& context,
                                                                    const til::point origin,
                                                                    gsl::span<WORD> buffer,
                                                                    size_t& written) noexcept
{
    std::fill_n(buffer.data(), buffer.size(), s_readBackUnicode.Attributes); // should be same as the ascii one.
    written = buffer.size();
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleOutputCharacterAImpl(const SCREEN_INFORMATION& context,
                                                                     const til::point origin,
                                                                     gsl::span<char> buffer,
                                                                     size_t& written) noexcept
{
    std::fill_n(buffer.data(), buffer.size(), s_readBackAscii.Char.AsciiChar);
    written = buffer.size();
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleOutputCharacterWImpl(const SCREEN_INFORMATION& context,
                                                                     const til::point origin,
                                                                     gsl::span<wchar_t> buffer,
                                                                     size_t& written) noexcept
{
    std::fill_n(buffer.data(), buffer.size(), s_readBackUnicode.Char.UnicodeChar);
    written = buffer.size();
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleInputAImpl(InputBuffer& context,
                                                            const gsl::span<const INPUT_RECORD> buffer,
                                                            size_t& written,
                                                            const bool append) noexcept
{
    return m_pUsualRoutines->WriteConsoleInputAImpl(context, buffer, written, append);
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleInputWImpl(InputBuffer& context,
                                                            const gsl::span<const INPUT_RECORD> buffer,
                                                            size_t& written,
                                                            const bool append) noexcept
{
    return m_pUsualRoutines->WriteConsoleInputWImpl(context, buffer, written, append);
}

extern HRESULT _ConvertCellsToWInplace(const UINT codepage,
                                       gsl::span<CHAR_INFO> buffer,
                                       const Viewport& rectangle) noexcept;

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleOutputAImpl(SCREEN_INFORMATION& context,
                                                             gsl::span<CHAR_INFO> buffer,
                                                             const Microsoft::Console::Types::Viewport& requestRectangle,
                                                             Microsoft::Console::Types::Viewport& writtenRectangle) noexcept
{
    // No UTF8 optimization because the entire `CHAR_INFO` grid system doesn't make sense for UTF-8
    // with up to 4 bytes per cell...or more!

    RETURN_IF_FAILED(_ConvertCellsToWInplace(m_outputCodepage, buffer, requestRectangle));
    return WriteConsoleOutputWImpl(context, buffer, requestRectangle, writtenRectangle);
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleOutputWImpl(SCREEN_INFORMATION& context,
                                                             gsl::span<CHAR_INFO> buffer,
                                                             const Microsoft::Console::Types::Viewport& requestRectangle,
                                                             Microsoft::Console::Types::Viewport& writtenRectangle) noexcept
{
    auto cursor = requestRectangle.Origin();

    const size_t width = requestRectangle.Width();
    size_t pos = 0;

    while (pos < buffer.size())
    {
        (void)m_pVtEngine->_CursorPosition(cursor);

        const auto subspan = buffer.subspan(pos, width);

        for (const auto& ci : subspan)
        {
            (void)m_pVtEngine->_SetGraphicsRendition16Color(static_cast<BYTE>(ci.Attributes), true);
            (void)m_pVtEngine->_SetGraphicsRendition16Color(static_cast<BYTE>(ci.Attributes >> 4), false);
            (void)m_pVtEngine->WriteTerminalW(std::wstring_view{ &ci.Char.UnicodeChar, 1 });
        }

        ++cursor.Y;
        pos += width;
    }

    (void)m_pVtEngine->_Flush();

    //TODO GH10001: trim to buffer size?
    writtenRectangle = requestRectangle;
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleOutputAttributeImpl(IConsoleOutputObject& OutContext,
                                                                     const gsl::span<const WORD> attrs,
                                                                     const til::point target,
                                                                     size_t& used) noexcept
{
    (void)m_pVtEngine->_CursorPosition(target);

    for (const auto& attr : attrs)
    {
        (void)m_pVtEngine->_SetGraphicsRendition16Color(static_cast<BYTE>(attr), true);
        (void)m_pVtEngine->_SetGraphicsRendition16Color(static_cast<BYTE>(attr >> 4), false);
        (void)m_pVtEngine->WriteTerminalUtf8(std::string_view{ &s_readBackAscii.Char.AsciiChar, 1 });
    }

    (void)m_pVtEngine->_Flush();

    used = attrs.size();
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleOutputCharacterAImpl(IConsoleOutputObject& OutContext,
                                                                      const std::string_view text,
                                                                      const til::point target,
                                                                      size_t& used) noexcept
{
    if (m_outputCodepage == CP_UTF8)
    {
        (void)m_pVtEngine->_CursorPosition(target);
        (void)m_pVtEngine->WriteTerminalUtf8(text);
        (void)m_pVtEngine->_Flush();
        return S_OK;
    }
    else
    {
        return WriteConsoleOutputCharacterWImpl(OutContext, ConvertToW(m_outputCodepage, text), target, used);
    }
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleOutputCharacterWImpl(IConsoleOutputObject& OutContext,
                                                                      const std::wstring_view text,
                                                                      const til::point target,
                                                                      size_t& used) noexcept
{
    (void)m_pVtEngine->_CursorPosition(target);
    (void)m_pVtEngine->WriteTerminalW(text);
    (void)m_pVtEngine->_Flush();
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleOutputAImpl(const SCREEN_INFORMATION& context,
                                                            gsl::span<CHAR_INFO> buffer,
                                                            const Microsoft::Console::Types::Viewport& sourceRectangle,
                                                            Microsoft::Console::Types::Viewport& readRectangle) noexcept
{
    std::fill_n(buffer.data(), buffer.size(), s_readBackAscii);
    // TODO GH10001: do we need to constrict readRectangle to within the known buffer size... probably.
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleOutputWImpl(const SCREEN_INFORMATION& context,
                                                            gsl::span<CHAR_INFO> buffer,
                                                            const Microsoft::Console::Types::Viewport& sourceRectangle,
                                                            Microsoft::Console::Types::Viewport& readRectangle) noexcept
{
    std::fill_n(buffer.data(), buffer.size(), s_readBackUnicode);
    // TODO GH10001: do we need to constrict readRectangle to within the known buffer size... probably.
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleTitleAImpl(gsl::span<char> title,
                                                          size_t& written,
                                                          size_t& needed) noexcept
{
    written = 0;
    needed = 0;

    if (!title.empty())
    {
        title.front() = ANSI_NULL;
    }

    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleTitleWImpl(gsl::span<wchar_t> title,
                                                          size_t& written,
                                                          size_t& needed) noexcept
{
    written = 0;
    needed = 0;

    if (!title.empty())
    {
        title.front() = UNICODE_NULL;
    }

    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleOriginalTitleAImpl(gsl::span<char> title,
                                                                  size_t& written,
                                                                  size_t& needed) noexcept
{
    written = 0;
    needed = 0;

    if (!title.empty())
    {
        title.front() = ANSI_NULL;
    }

    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleOriginalTitleWImpl(gsl::span<wchar_t> title,
                                                                  size_t& written,
                                                                  size_t& needed) noexcept
{
    written = 0;
    needed = 0;

    if (!title.empty())
    {
        title.front() = UNICODE_NULL;
    }

    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleTitleAImpl(const std::string_view title) noexcept
{
    return SetConsoleTitleWImpl(ConvertToW(m_inputCodepage, title));
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleTitleWImpl(const std::wstring_view title) noexcept
{
    (void)m_pVtEngine->UpdateTitle(title);
    (void)m_pVtEngine->_Flush();
    return S_OK;
}

void VtApiRoutines::GetNumberOfConsoleMouseButtonsImpl(ULONG& buttons) noexcept
{
    buttons = 2;
    return;
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleFontSizeImpl(const SCREEN_INFORMATION& context,
                                                            const DWORD index,
                                                            til::size& size) noexcept
{
    size.X = 8;
    size.Y = 12;
    return S_OK;
}

//// driver will pare down for non-Ex method
[[nodiscard]] HRESULT VtApiRoutines::GetCurrentConsoleFontExImpl(const SCREEN_INFORMATION& context,
                                                                 const bool isForMaximumWindowSize,
                                                                 CONSOLE_FONT_INFOEX& consoleFontInfoEx) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleDisplayModeImpl(SCREEN_INFORMATION& context,
                                                               const ULONG flags,
                                                               til::size& newSize) noexcept
{
    return S_OK;
}

void VtApiRoutines::GetConsoleDisplayModeImpl(ULONG& flags) noexcept
{
    flags = 0;
    return;
}

[[nodiscard]] HRESULT VtApiRoutines::AddConsoleAliasAImpl(const std::string_view source,
                                                          const std::string_view target,
                                                          const std::string_view exeName) noexcept
{
    return m_pUsualRoutines->AddConsoleAliasAImpl(source, target, exeName);
}

[[nodiscard]] HRESULT VtApiRoutines::AddConsoleAliasWImpl(const std::wstring_view source,
                                                          const std::wstring_view target,
                                                          const std::wstring_view exeName) noexcept
{
    return m_pUsualRoutines->AddConsoleAliasWImpl(source, target, exeName);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasAImpl(const std::string_view source,
                                                          gsl::span<char> target,
                                                          size_t& written,
                                                          const std::string_view exeName) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasAImpl(source, target, written, exeName);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasWImpl(const std::wstring_view source,
                                                          gsl::span<wchar_t> target,
                                                          size_t& written,
                                                          const std::wstring_view exeName) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasWImpl(source, target, written, exeName);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasesLengthAImpl(const std::string_view exeName,
                                                                  size_t& bufferRequired) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasesLengthAImpl(exeName, bufferRequired);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasesLengthWImpl(const std::wstring_view exeName,
                                                                  size_t& bufferRequired) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasesLengthWImpl(exeName, bufferRequired);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasExesLengthAImpl(size_t& bufferRequired) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasExesLengthAImpl(bufferRequired);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasExesLengthWImpl(size_t& bufferRequired) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasExesLengthWImpl(bufferRequired);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasesAImpl(const std::string_view exeName,
                                                            gsl::span<char> alias,
                                                            size_t& written) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasesAImpl(exeName, alias, written);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasesWImpl(const std::wstring_view exeName,
                                                            gsl::span<wchar_t> alias,
                                                            size_t& written) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasesWImpl(exeName, alias, written);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasExesAImpl(gsl::span<char> aliasExes,
                                                              size_t& written) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasExesAImpl(aliasExes, written);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasExesWImpl(gsl::span<wchar_t> aliasExes,
                                                              size_t& written) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasExesWImpl(aliasExes, written);
}

[[nodiscard]] HRESULT VtApiRoutines::ExpungeConsoleCommandHistoryAImpl(const std::string_view exeName) noexcept
{
    return m_pUsualRoutines->ExpungeConsoleCommandHistoryAImpl(exeName);
}

[[nodiscard]] HRESULT VtApiRoutines::ExpungeConsoleCommandHistoryWImpl(const std::wstring_view exeName) noexcept
{
    return m_pUsualRoutines->ExpungeConsoleCommandHistoryWImpl(exeName);
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleNumberOfCommandsAImpl(const std::string_view exeName,
                                                                     const size_t numberOfCommands) noexcept
{
    return m_pUsualRoutines->SetConsoleNumberOfCommandsAImpl(exeName, numberOfCommands);
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleNumberOfCommandsWImpl(const std::wstring_view exeName,
                                                                     const size_t numberOfCommands) noexcept
{
    return m_pUsualRoutines->SetConsoleNumberOfCommandsWImpl(exeName, numberOfCommands);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleCommandHistoryLengthAImpl(const std::string_view exeName,
                                                                         size_t& length) noexcept
{
    return m_pUsualRoutines->GetConsoleCommandHistoryLengthAImpl(exeName, length);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleCommandHistoryLengthWImpl(const std::wstring_view exeName,
                                                                         size_t& length) noexcept
{
    return m_pUsualRoutines->GetConsoleCommandHistoryLengthWImpl(exeName, length);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleCommandHistoryAImpl(const std::string_view exeName,
                                                                   gsl::span<char> commandHistory,
                                                                   size_t& written) noexcept
{
    return m_pUsualRoutines->GetConsoleCommandHistoryAImpl(exeName, commandHistory, written);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleCommandHistoryWImpl(const std::wstring_view exeName,
                                                                   gsl::span<wchar_t> commandHistory,
                                                                   size_t& written) noexcept
{
    return m_pUsualRoutines->GetConsoleCommandHistoryWImpl(exeName, commandHistory, written);
}

void VtApiRoutines::GetConsoleWindowImpl(HWND& hwnd) noexcept
{
    hwnd = ServiceLocator::LocatePseudoWindow();
    return;
}

void VtApiRoutines::GetConsoleSelectionInfoImpl(CONSOLE_SELECTION_INFO& consoleSelectionInfo) noexcept
{
    consoleSelectionInfo = { 0 };
    return;
}

void VtApiRoutines::GetConsoleHistoryInfoImpl(CONSOLE_HISTORY_INFO& consoleHistoryInfo) noexcept
{
    m_pUsualRoutines->GetConsoleHistoryInfoImpl(consoleHistoryInfo);
    return;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleHistoryInfoImpl(const CONSOLE_HISTORY_INFO& consoleHistoryInfo) noexcept
{
    return m_pUsualRoutines->SetConsoleHistoryInfoImpl(consoleHistoryInfo);
}

[[nodiscard]] HRESULT VtApiRoutines::SetCurrentConsoleFontExImpl(IConsoleOutputObject& context,
                                                                 const bool isForMaximumWindowSize,
                                                                 const CONSOLE_FONT_INFOEX& consoleFontInfoEx) noexcept
{
    return S_OK;
}

#pragma warning(pop)
